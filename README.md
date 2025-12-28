# Split L1 Cache & MESI Protocol Simulator

A dual-cache simulator implementing an **L1 Instruction Cache (I$)** and an **L1 Data Cache (D$)** with full **MESI coherence**. This project models hardware behavior, snoop logic, and L2 cache interactions.

---

## 🚀 Project Overview
The simulator consumes a memory operation trace file and produces cache state dumps and performance statistics. It is written in C and is designed to demonstrate how cache coherence protocols maintain data integrity in multi-core systems.

### Features
* **Split L1 Architecture:** Separate Instruction and Data caches.
* **MESI Protocol:** Full state machine (Modified, Exclusive, Shared, Invalid).
* **Configurable Geometry:** Change cache sizes and associativity via a config file.
* **Trace-Driven:** Processes standard memory traces to simulate real-world workloads.

---

## 🛠️ Compilation

The simulator uses the `math.h` library. Use the following commands to compile:

**Linux / macOS:**
```bash
gcc source_file.c -lm -o cache_sim

```
# Windows (MinGW/Visual Studio):
```bash
gcc source_file.c -o cache_sim

```

# Usage
## Run the executable via the command line with the following syntax:

```Bash
./cache_sim --trace <tracefile> --mode <0|1> [--max-events N] [--quiet]
```

## Cache Dynamic Reconfiguration

The simulator supports **runtime cache configuration** through a file named:

> ⚠️ The file **must be located in the same directory as the executable**.

This mechanism allows you to modify cache geometry **without recompiling** the project.

---

## Configurable Cache Parameters

#### ADDRESS_SIZE_BITS = 32
#### ILINE_SIZE_BYTES = 64
#### DLINE_SIZE_BYTES = 64
#### NUM_SETS_DCACHE = 16384
#### NUM_SETS_ICACHE = 16384
#### IWAYS = 4
#### DWAYS = 8


### Parameter Description

- **ADDRESS_SIZE_BITS** : Physical address width  
- **ILINE_SIZE_BYTES**  : Instruction cache line size  
- **DLINE_SIZE_BYTES**  : Data cache line size  
- **NUM_SETS_ICACHE**   : Number of sets in the instruction cache  
- **NUM_SETS_DCACHE**   : Number of sets in the data cache  
- **IWAYS**             : Instruction cache associativity  
- **DWAYS**             : Data cache associativity  

---

## Compiling Notes (Math Library)

The simulator uses functions from `math.h`.

### Linux

You must explicitly link the math library:

```bash
gcc source_file.c -lm
```

# Windows (MinGW / MSYS / Visual Studio)

The math library is linked automatically, so -lm is not required.
