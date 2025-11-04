# Build configuration for the FLS language (Bytecode VM version)

# Output configuration
OUTPUT_NAME = fls

# Source files
SOURCES = \
	src/main.c \
	src/memory.c \
	src/chunk.c \
	src/debug.c \
	src/value.c \
	src/object.c \
	src/table.c \
	src/lexer.c \
	src/compiler.c \
	src/error.c \
	src/vm.c \
	std/src/io.c \
	std/src/math.c \
	std/src/random.c \
	std/src/dict.c

# Include directories
INCLUDE_DIRS = \
	-Iinclude \
	-Istd/include

# Libraries
LIBS = -lm

# Compiler and linker flags
CFLAGS = \
	-Wall \
	-Wextra \
	-pthread \
	-g \
	-Os \
	-Wno-unused-parameter \
	-Wno-unused-function
	
LDFLAGS = -lm -pthread

# Debug flags (uncomment to enable bytecode and VM execution tracing)
# DEBUG_FLAGS = -DDEBUG_PRINT_CODE -DDEBUG_TRACE_EXECUTION
# CFLAGS += $(DEBUG_FLAGS)

# Object files (derived from sources)
OBJECTS = $(SOURCES:.c=.o)

# Compiler
CC = gcc

# Default target
all: $(OUTPUT_NAME)

# Link object files to create executable
$(OUTPUT_NAME): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(OUTPUT_NAME)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(OUTPUT_NAME)

# Rebuild everything from scratch
rebuild: clean all

# Run the program
run: $(OUTPUT_NAME)
	./$(OUTPUT_NAME)

# Phony targets
.PHONY: all clean rebuild run