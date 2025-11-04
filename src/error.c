#include <error.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "error.h"
#include "vm.h"

void reportError(bool isCompileError, const char* moduleName, int line, const char* lineStr, int col, int length, const char* message) {
    fprintf(stderr, "\n%s%s Error: %s%s\n",
            ANSI_BOLD, isCompileError ? ANSI_COLOR_RED "Compile" : ANSI_COLOR_RED "Runtime",
            ANSI_COLOR_RESET ANSI_BOLD, message);

    fprintf(stderr, "%s  --> %s:%d%s\n", ANSI_COLOR_BLUE, moduleName, line, ANSI_COLOR_RESET);
    fprintf(stderr, "%s   |%s\n", ANSI_COLOR_BLUE, ANSI_COLOR_RESET);
    fprintf(stderr, "%s%4d |%s %s\n", ANSI_COLOR_BLUE, line, ANSI_COLOR_RESET, lineStr);
    fprintf(stderr, "%s   |%s ", ANSI_COLOR_BLUE, ANSI_COLOR_RESET);

    for (int i = 0; i < col; i++) {
        fputc(' ', stderr);
    }

    fprintf(stderr, "%s%s^", ANSI_BOLD, ANSI_COLOR_RED);
    for (int i = 0; i < length - 1; i++) {
        fputc('~', stderr);
    }
    fprintf(stderr, " Here%s\n", ANSI_COLOR_RESET);
}

void runtimeError(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    ObjFunction* function = frame->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    int line = function->chunk.lines[instruction];

    // Open the source file to get the line content
    FILE* file = fopen(function->module->name->chars, "r");
    if (file == NULL) {
        // Fallback for when the source is not available
        fprintf(stderr, "Runtime Error: %s\n", message);
        fprintf(stderr, "  --> %s:%d in %s\n",
                function->module->name->chars,
                line,
                function->name == NULL ? "script" : function->name->chars);
    } else {
        char lineStr[1024];
        int currentLine = 0;
        while (fgets(lineStr, sizeof(lineStr), file)) {
            currentLine++;
            if (currentLine == line) {
                // Trim newline character
                lineStr[strcspn(lineStr, "\r\n")] = 0;
                reportError(false, function->module->name->chars, line, lineStr, 0, 1, message);
                break;
            }
        }
        fclose(file);
    }

    // Print the stack trace
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}
