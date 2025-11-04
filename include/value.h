#ifndef FLS_VALUE_H
#define FLS_VALUE_H

#include "common.h"

// Forward declarations to break circular dependencies.
typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// Macros for checking the type of a Value.
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

// Macros for converting a Value back to its C type.
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)     ((value).as.obj)

// Macros for creating a Value from a C type.
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

// A dynamic array to hold a list of values.
typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

// Function prototypes for value operations.
bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
Value popValueArray(ValueArray* array);
Value removeValueArray(ValueArray* array, int index);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif // FLS_VALUE_H
