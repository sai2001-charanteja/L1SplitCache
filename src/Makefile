# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -O2 -std=c11

# Source and output
SRC = L1SplitCache.c
OUT = L1SplitCache

# Default trace + mode (so "make run" works immediately)
TRACE = Trace_files/scenario1_trace.txt
MODE  = 0
N = 100

# Build the executable
all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) -lm

# Run with arguments
run: $(OUT)
	./$(OUT) --trace $(TRACE) --mode $(MODE) --max-events $(N) --quiet

# Clean build files
clean:
	rm -f $(OUT)
