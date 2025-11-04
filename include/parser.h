#ifndef FLS_PARSER_H
#define FLS_PARSER_H

#include "lexer.h"
#include "stmt.h"

// The main entry point for the parsing module.
// It now returns a dynamic array of statements, representing the program.
// The caller is responsible for freeing this array.
// Returns NULL if a parsing error occurs.
Stmt** parse(const char* source);

#endif // FLS_PARSER_H
