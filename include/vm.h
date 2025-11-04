#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "profiler.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
    uint64_t loop_counter;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table modules;
    Table strings;
    Obj* objects;
    bool hadError;
    size_t bytesAllocated;
    size_t nextGC;
    
    Profiler profiler;
    bool enable_preflight;
    uint64_t instruction_count;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* path, const char* source);
void push(Value value);
Value pop();
void runtimeError(const char* format, ...);
void defineNative(const char* name, NativeFn function);
void defineGlobal(const char* name, Value value);
void resetStack();

#endif
