#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

// A simple Read-Eval-Print-Loop (REPL) for interactive mode.
static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret("<REPL>", line);
    }
}

// Reads an entire file into a heap-allocated string.
static char* readFile(const char* path) {
    if (path == NULL) {
        fprintf(stderr, "Invalid file path\n");
        exit(74);
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fprintf(stderr, "Could not seek file \"%s\".\n", path);
        fclose(file);
        exit(74);
    }

    long fileSize = ftell(file);
    if (fileSize < 0 || fileSize > 100 * 1024 * 1024) {
        fprintf(stderr, "Invalid file size for \"%s\".\n", path);
        fclose(file);
        exit(74);
    }
    rewind(file);

    char* buffer = (char*)malloc((size_t)fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        fclose(file);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), (size_t)fileSize, file);
    if (bytesRead < (size_t)fileSize) {
        fprintf(stderr, "Could not read entire file \"%s\".\n", path);
        free(buffer);
        fclose(file);
        exit(74);
    }

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

// Runs a script from a file.
static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(path, source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else if (argc == 3 && strcmp(argv[1], "--preflight") == 0) {
        vm.enable_preflight = true;
        runFile(argv[2]);
    } else {
        fprintf(stderr, "Usage: fls [--preflight] [path]\n");
        exit(64);
    }

    freeVM();
    return 0;
}
