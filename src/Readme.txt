Overview
---------
This project implements a dual-cache simulator consisting of an L1 Instruction Cache (I$) and an L1 Data Cache (D$) with full MESI coherence, support for invalidations, snoops, and interactions with an L2 model.

The simulator consumes a trace file containing memory operations and produces cache state dumps, performance statistics, and—when enabled—L2 communication events.
***************************************************************************
You run the simulator using: 
	outputfile --trace <tracefile> --mode <0|1> [--max-events N] [--quiet]
***************************************************************************

Cache Dynamic Reconfiguration
---------------------
The simulator supports user-configurable cache parameters through a file named cache_config.txt, which must reside in the same directory as the executable.

Configurable Parameters
***************************************************************************
ADDRESS_SIZE_BITS = 32
ILINE_SIZE_BYTES  = 64
DLINE_SIZE_BYTES  = 64
NUM_SETS_DCACHE   = 16384
NUM_SETS_ICACHE   = 16384
IWAYS             = 4
DWAYS             = 8
***************************************************************************

These parameters allow you to modify the cache geometry—line size, associativity, and number of sets—without recompiling the entire project.


--------------------------------
Compiling Notes (Math Library) |
--------------------------------
The simulator uses math.h, so Linux builds require linking the math library:
***************************************************************************
gcc source_file.c -lm
***************************************************************************

On Windows (MinGW/MSYS/Visual Studio), the math library is linked automatically, so -lm is not needed.

------------------------------------------------------------------------------------------