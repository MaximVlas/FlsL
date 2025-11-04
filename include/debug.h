#ifndef FLS_DEBUG_H
#define FLS_DEBUG_H

#include "chunk.h"

// Disassembles all the instructions in a chunk, printing them to stdout.
// This is an invaluable tool for debugging the compiler.
void disassembleChunk(Chunk* chunk, const char* name);

// Disassembles a single instruction at a given offset within a chunk.
// Returns the offset of the next instruction.
int disassembleInstruction(Chunk* chunk, int offset);

#endif // FLS_DEBUG_H
