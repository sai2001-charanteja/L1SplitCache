#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h> 

/************************************************************
 *  Fixed project constants (from spec)
 ************************************************************/
#define ADDRESS_SIZE_BITS 32      // 32-bit addresses
#define ILINE_SIZE_BYTES   64      // 64-byte cache line
#define DLINE_SIZE_BYTES   64      // 64-byte cache line
#define NUM_SETS_DCACHE   16384   // 16K sets
#define NUM_SETS_ICACHE   16384   // 16K sets
#define IWAYS             4       // I$ is 4-way set associative
#define DWAYS             8       // D$ is 8-way set associative

/************************************************************
 *  Trace opcodes (first column of trace file)
 ************************************************************/

#define OP_DREAD          0   // Data read  (D$)
#define OP_DWRITE         1   // Data write (D$)
#define OP_IFETCH         2   // Instruction fetch (I$)
#define OP_INVALIDATE     3   // Invalidate from L2 (D$ only)
#define OP_RFO_SNOOP      4   // RFO snoop from L2 (D$ only)
#define OP_CLEAR          8   // Clear both caches + stats
#define OP_PRINT          9   // Print cache state

 /************************************************************
   *  Cache Type
   ************************************************************/

typedef enum {
    INSTRUCTION = 0,  // Invalid
    DATA
} cache_typee;


 /************************************************************
  *  MESI protocol states
  ************************************************************/

typedef enum {
    MESI_I = 0,  // Invalid
    MESI_S,      // Shared
    MESI_E,      // Exclusive
    MESI_M       // Modified
} mesi_t;

/************************************************************
 *  Basic line and stats types (shared by both caches)
 ************************************************************/

typedef struct {
    unsigned long reads;
    unsigned long writes;
    unsigned long hits;
    unsigned long misses;
} stats_t;

typedef struct {
    //uint8_t  valid;  // 0 = empty / unused, 1 = has something (possibly I)
    uint32_t tag;    // Tag bits for the line
    mesi_t   mesi;   // MESI state
    int      lru;    // LRU rank: bigger = more recently used
} line_t;

/************************************************************
 *  Cache specific types (n-way)
 ************************************************************/

typedef struct {
	line_t* ways;   //Array of n ways
} set_t;

typedef struct {
    const char* name;     // "Cache name"
    stats_t    stats;
    set_t* sets;     // array of NUM_SETS entries
} cache_t;

typedef struct {
	cache_t* core;
}cores;

/************************************************************
 *  Global caches and mode variable
 ************************************************************/

static cache_t I_cache;       // Instruction cache
static cache_t D_cache;       // Data cache

// Simulation mode: 0 = summary only; 1 = also print L2 messages
static int g_mode = 0;

/************************************************************
 *  Address decode helper
 ************************************************************/
static void decode_addressbyCacheType(uint32_t addr,
    uint32_t* tag,
    uint32_t* index,
    uint32_t* offset,cache_typee ctype)
{
    int offsetbits,indexbits,tagbits;

    if (ctype == INSTRUCTION) { // Instrcution cache
        offsetbits = ceil(log2(ILINE_SIZE_BYTES));
		indexbits = ceil(log2(NUM_SETS_ICACHE));
    }
    else { // Data cache
		offsetbits = ceil(log2(DLINE_SIZE_BYTES));
        indexbits = ceil(log2(NUM_SETS_DCACHE));
    }
    tagbits = ADDRESS_SIZE_BITS - (offsetbits + indexbits);
    *offset = addr & ((1u << offsetbits) - 1u);
    *index = (addr >> offsetbits) & ((1u << indexbits) - 1u);
    *tag = (addr >> (offsetbits + indexbits)) & ((1u << tagbits) - 1u);
}

/************************************************************
 *  General Function For Cache Info
 ************************************************************/

static int get_ways_byCacheType(cache_typee ctype)
{
    if (ctype== INSTRUCTION) { // Instrcution cache
        return IWAYS;
    }
    else { // Data cache
        return DWAYS;
    }
}

static int get_numsets_byCacheType(cache_typee ctype)
{
    if (ctype == INSTRUCTION) { // Instrcution cache
        return NUM_SETS_ICACHE;
    }
    else { // Data cache
        return NUM_SETS_DCACHE;
    }
}

/************************************************************
 *  L2 message helpers (only print in mode 1)
 ************************************************************/

static void msg_read_from_l2(void) {
    if (g_mode == 1) printf("Read from L2\n");
}

static void msg_rfo_from_l2(void) {
    if (g_mode == 1) printf("Read for Ownership from L2\n");
}

static void msg_write_to_l2(void) {
    if (g_mode == 1) printf("Write to L2\n");
}

static void msg_return_data_to_l2(void) {
    if (g_mode == 1) printf("Return data to L2\n");
}

/************************************************************
 *  MESI string helper
 ************************************************************/

static const char* mesi_str(mesi_t m)
{
    switch (m) {
    case MESI_I: return "I";
    case MESI_S: return "S";
    case MESI_E: return "E";
    case MESI_M: return "M";
    default:     return "?";
    }
}

/************************************************************
 *  Cache helper functions (search, LRU, etc.)
 ************************************************************/

 // Find way with matching tag and a non-Invalid MESI state.
 // Returns way index or -1 if not found.
static int cache_find_way(cache_t* c, uint32_t index, uint32_t tag,cache_typee ctype)
{
	int num_ways = get_ways_byCacheType(ctype);
    set_t* set = &c->sets[index];
    for (int w = 0; w < num_ways; ++w) {
        line_t* ln = &set->ways[w];
        if (ln->mesi != MESI_I && ln->tag == tag) {
            return w;
        }
    }
    return -1;
}


// Find first invalid way MESI_I. Returns way or -1.
static int cache_find_invalid_way(cache_t* c, uint32_t index,cache_typee ctype)
{
	int num_ways = get_ways_byCacheType(ctype);
    set_t* set = &c->sets[index];
    for (int w = 0; w < num_ways; ++w) {
        line_t* ln = &set->ways[w];
        if (ln->mesi == MESI_I) {
            return w;
        }
    }
    return -1;
}

// Find the LRU way (smallest lru value).
static int cache_find_lru_way(cache_t* c, uint32_t index,cache_typee ctype)
{
	int num_ways = get_ways_byCacheType(ctype);
    
    set_t* set = &c->sets[index];
    int max_lru = 0;
    int max_lru_way = 0;

    for (int way_idx = 0; way_idx < num_ways; ++way_idx) {  // Finding the maximum LRU value in valid lines
        line_t* ln = &set->ways[way_idx];
        if (ln->lru > max_lru) {
            max_lru = ln->lru;
            max_lru_way = way_idx;
        }
    }
    return max_lru_way;
    


}

// Update LRU for a specific way: make it most recently used.
static void cache_update_lru(cache_t* c, uint32_t index, int way,cache_typee ctype)
{
	int num_ways = get_ways_byCacheType(ctype);
    set_t* set = &c->sets[index]; // Set 
    int max_lru = set->ways[way].lru;

    for (int way_idx = 0; way_idx < num_ways; ++way_idx) {
        line_t* ln = &set->ways[way_idx];
        if (ln->lru <= max_lru) {
            ln->lru++;
        }
    }
    set->ways[way].lru = 0;
}

/************************************************************
 *  Cache initialization and clear
 ************************************************************/

static void cache_init(cache_t* c, char* name,cache_typee ctype)
{
    int num_of_sets = get_numsets_byCacheType(ctype);
    int num_of_ways = get_ways_byCacheType(ctype);
    c->name = name;
    c->stats.reads = 0;
    c->stats.writes = 0;
    c->stats.hits = 0;
    c->stats.misses = 0;

    c->sets = (set_t*)calloc(num_of_sets, sizeof(set_t));
    if (!c->sets) {
        fprintf(stderr, "Failed to allocate %s sets\n",name);
        exit(1);
    }
    else {
        for (int idx = 0;idx < num_of_sets;idx++) {
            c->sets[idx].ways = calloc(num_of_ways, sizeof(line_t));
            set_t* set = &c->sets[idx];
            if (!c->sets[idx].ways) {
                fprintf(stderr, "Failed to allocate %s Ways\n",name);
                exit(1);
            }
            else {
                for (int j = 0;j < num_of_ways;j++) {
                    line_t* ln = &set->ways[j];
                    ln->mesi = MESI_I;
                    ln->lru = num_of_ways - 1;
                }
            }
        }
    }
}

// Clear cache contents and stats (used when opcode 8 is seen).
static void cache_clear(cache_t* c, cache_typee ctype)
{
    int num_of_sets = get_numsets_byCacheType(ctype);
    int num_of_ways = get_ways_byCacheType(ctype);
    for (int i = 0; i < num_of_sets; ++i) {
        set_t* set = &c->sets[i];
        for (int w = 0; w < num_of_ways; ++w) {
            line_t* ln = &set->ways[w];
            ln->tag = 0;
            ln->mesi = MESI_I;
            ln->lru = num_of_ways - 1;
        }
    }
    c->stats.reads = 0;
    c->stats.writes = 0;
    c->stats.hits = 0;
    c->stats.misses = 0;
}

static void DecimalToBinary(int n,int nway)
{
	int num_of_bits;
    num_of_bits = ceil(log2(nway));
	char* outbuf = (char*)malloc((num_of_bits+1) * sizeof(char));
    for (int i = num_of_bits - 1; i >= 0; --i) {
        outbuf[i] = (n & 1) ? '1' : '0';
        n >>= 1;
    }
    outbuf[num_of_bits] = '\0';
    printf(" LRU=%s\n", outbuf);
    free(outbuf);
}
/************************************************************
 *  Print cache state and stats
 ************************************************************/

static void cache_print(cache_t* c, cache_typee ctype)
{
    int num_of_sets = get_numsets_byCacheType(ctype);
    int num_of_ways = get_ways_byCacheType(ctype);
    printf("=== %s Contents ===\n", c->name);
    for (uint32_t i = 0; i < num_of_sets; ++i) {
        set_t* set = &c->sets[i];
        int printed_header = 0;

        for (int w = 0; w < num_of_ways; ++w) {
            line_t* ln = &set->ways[w];
            if (ln->mesi != MESI_I) {
                if (!printed_header) {
                    printf("Set 0x%04x:\n", i);
                    printed_header = 1;
                }
                printf("  way%d TAG=0x%03x STATE=%s ",
                    w, ln->tag, mesi_str(ln->mesi));
                DecimalToBinary(ln->lru, num_of_ways);
            }
        }
    }
}

static void cache_print_stats(cache_t* c,cache_typee ctype)
{
    unsigned long total = c->stats.hits + c->stats.misses;
    double ratio = 0.0;
    if (total > 0) {
        ratio = (double)c->stats.hits * 100.0 / (double)total;
    }

    printf("=== %s Statistics ===\n", c->name);
    printf("Cache reads     : %lu\n", c->stats.reads);
    if(ctype == DATA)
        printf("Cache writes    : %lu\n", c->stats.writes);
    printf("Cache hits      : %lu\n", c->stats.hits);
    printf("Cache misses    : %lu\n", c->stats.misses);
    printf("Cache hit ratio : %5.2f %%\n", ratio);
}

/************************************************************
 *  I$ access (opcode 2) – instruction fetch
 ************************************************************/

static void icache_ifetch(cache_t* c, uint32_t addr)
{
	cache_typee ctype = INSTRUCTION;

    uint32_t tag, index, offset;
    decode_addressbyCacheType(addr, &tag, &index, &offset, ctype);

    c->stats.reads++;

    int way = cache_find_way(c, index, tag, ctype);
    if (way >= 0) {
        // Hit
        c->stats.hits++;
        if ((&(c->sets[index]))->ways[way].mesi == MESI_E)
            (&(c->sets[index]))->ways[way].mesi = MESI_S;
        cache_update_lru(c, index, way,ctype);
    }
    else {
        // Miss
        c->stats.misses++;

        int insert_way = cache_find_invalid_way(c, index,ctype);
        if (insert_way < 0) {
            insert_way = cache_find_lru_way(c, index, ctype);
            // No write-back for I$ (never Modified)
        }

        set_t* set = &c->sets[index];
        line_t* ln = &set->ways[insert_way];

        ln->tag = tag;
        ln->mesi = MESI_E;   // I$ uses I/S/E only

        msg_read_from_l2();
        cache_update_lru(c, index, insert_way,ctype);
    }
}

/************************************************************
 *  D$ accesses (opcodes 0,1,3,4)
 ************************************************************/

 // Data read (opcode 0)
static void dcache_read(cache_t* c, uint32_t addr)
{
	cache_typee ctype = DATA;

    uint32_t tag, index, offset;
    decode_addressbyCacheType(addr, &tag, &index, &offset, ctype);

    c->stats.reads++;

    int way = cache_find_way(c, index, tag,ctype);
    if (way >= 0) {
        // Hit
        c->stats.hits++;
        if ((&(c->sets[index]))->ways[way].mesi == MESI_E)
            (&(c->sets[index]))->ways[way].mesi = MESI_S;
               

        cache_update_lru(c, index, way,ctype);
    }
    else {
        // Miss
        c->stats.misses++;

        int insert_way = cache_find_invalid_way(c, index,ctype);
        if (insert_way < 0) { // None of them is invalid
            insert_way = cache_find_lru_way(c, index,ctype);
            set_t* set = &c->sets[index];
            line_t* victim = &set->ways[insert_way];

            if (victim->mesi == MESI_M) {
                // Evict modified line -> write-back to L2
                msg_write_to_l2();
            }
        }

        set_t* set = &c->sets[index];
        line_t* ln = &set->ways[insert_way];

        ln->tag = tag;
        ln->mesi = MESI_E;   // Assume Exclusive on read miss

        msg_read_from_l2();

        cache_update_lru(c, index, insert_way,ctype);
    }
}

// Data write (opcode 1)
static void dcache_write(cache_t* c, uint32_t addr)
{
	cache_typee ctype = DATA;
    uint32_t tag, index, offset;
    decode_addressbyCacheType(addr, &tag, &index, &offset, 0);

    c->stats.writes++;

    int way = cache_find_way(c, index, tag,ctype);
    if (way >= 0) {
        // Write hit
        c->stats.hits++;
        set_t* set = &c->sets[index];
        line_t* ln = &set->ways[way];

        // E/S/M -> M on write
        if (ln->mesi == MESI_E || ln->mesi == MESI_S) {
            ln->mesi = MESI_M;
        }

        cache_update_lru(c, index, way,ctype);
    }
    else {
        // Write miss: write-allocate + Read-For-Ownership
        c->stats.misses++;

        int insert_way = cache_find_invalid_way(c, index,ctype);
        if (insert_way < 0) {
            insert_way = cache_find_lru_way(c, index,ctype);
            set_t* set = &c->sets[index];
            line_t* victim = &set->ways[insert_way];

            if (victim->mesi == MESI_M) {
                msg_write_to_l2();  // write back old modified line
            }
        }

        set_t* set = &c->sets[index];
        line_t* ln = &set->ways[insert_way];

        ln->tag = tag;

        // Miss -> Read-For-Ownership from L2
        msg_rfo_from_l2();

        // Starts in E, then first write does WT and goes M
        ln->mesi = MESI_E;
        msg_write_to_l2();    // first write-through to L2
        ln->mesi = MESI_M;    // now Modified

        cache_update_lru(c, index, insert_way,ctype);
    }
}

// Invalidate (opcode 3) – D$ only
static void dcache_invalidate(cache_t* c, uint32_t addr)
{
	cache_typee ctype = DATA;
    uint32_t tag, index, offset;
    decode_addressbyCacheType(addr, &tag, &index, &offset, 0);

    int way = cache_find_way(c, index, tag,ctype);
    if (way >= 0) {
        set_t* set = &c->sets[index];
        line_t* ln = &set->ways[way];

        ln->mesi = MESI_I;
      //  ln->lru = DWAYS-1;

		// Print may be missing 
    }
}

// RFO snoop (opcode 4) – D$ only
static void dcache_rfo_snoop(cache_t* c, uint32_t addr)
{
	cache_typee ctype = DATA;
    uint32_t tag, index, offset;
    decode_addressbyCacheType(addr, &tag, &index, &offset, 0);

    int way = cache_find_way(c, index, tag,ctype);
    if (way >= 0) {
        set_t* set = &c->sets[index];
        line_t* ln = &set->ways[way];

        if (ln->mesi == MESI_M || ln->mesi == MESI_E || ln->mesi == MESI_S) {
            msg_return_data_to_l2();
            ln->mesi = MESI_I;
           // ln->lru = DWAYS-1;
        }
    }
}

/************************************************************
 *  Dispatch one trace operation
 ************************************************************/

static void process_op(int opcode, uint32_t addr)
{
    switch (opcode) {
    case OP_DREAD:
		dcache_read(&D_cache, addr); 
        break;
    case OP_DWRITE:
        dcache_write(&D_cache, addr); 
        break;
    case OP_IFETCH:
        icache_ifetch(&I_cache, addr);
        break;
    case OP_INVALIDATE:
        dcache_invalidate(&D_cache, addr);
        break;
    case OP_RFO_SNOOP:
        dcache_rfo_snoop(&D_cache, addr);
        break;
    case OP_CLEAR:
        cache_clear(&I_cache,INSTRUCTION);
        cache_clear(&D_cache,DATA);
        break;
    case OP_PRINT:
        cache_print(&D_cache, DATA);
        cache_print(&I_cache, INSTRUCTION);
        break;
    default:
        // Unknown opcode; ignore
        break;
    }
}

/************************************************************
 *  Main: parse arguments, read trace, run simulation
 ************************************************************/



int main(int argc, char** argv)
{
    // output_file_name  --trace TRACEFILE [--mode 0|1]
    if (argc < 2) {
        fprintf(stderr, "Usage: %s --trace TRACEFILE [--mode 0|1]\n", argv[0]);
        return 1;
    }

    // Default mode is 0
    g_mode = 0;
    if (argc >= 5 && strcmp(argv[1], "--trace") != 0) {
        fprintf(stderr, "Usage: %s --trace TRACEFILE [--mode 0|1]\n", argv[0]);
        return 1;
    }

    if (argc >= 5 && strcmp(argv[3], "--mode") == 0) {
        g_mode = atoi(argv[4]);
    }

    cache_init(&I_cache,"I$",INSTRUCTION);
    cache_init(&D_cache,"D$",DATA);

    FILE* fp = fopen(argv[2], "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    char linebuf[256];

    while (fgets(linebuf, sizeof(linebuf), fp)) {
        // Skip empty / comment lines
        if (linebuf[0] == '\n' || linebuf[0] == '\r' || linebuf[0] == '#')
            continue;

        char* p = linebuf;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '#')
            continue;

        int opcode = -1;
        char addr_str[64];

        int count = sscanf(p, "%d %63s", &opcode, addr_str);
        if (count < 1) continue;
        if (opcode < 0) continue;
        if (count == 1) addr_str[0] = '\0';

        // Parse address as hex (with or without 0x prefix)
        uint32_t addr = (uint32_t)strtoul(addr_str, NULL, 16);

        process_op(opcode, addr);
    }
    fclose(fp);

    cache_print_stats(&I_cache,INSTRUCTION);
    cache_print_stats(&D_cache,DATA);

    free(I_cache.sets);
    free(D_cache.sets);

    return 0;
}
