#ifndef fls_error_h
#define fls_error_h

#include "common.h"
#include <stdbool.h>

// ANSI color codes for formatted output
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"
#include "vm.h"

void reportError(bool isCompileError, const char* moduleName, int line, const char* lineStr, int col, int length, const char* message);
void runtimeError(const char* format, ...);

#endif
