#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "object.h"
#include "memory.h"
#include "value.h"

// Initializes a value array.
void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

// Writes a value to a value array.
void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values,
                                   oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

// Frees a value array.
void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

Value popValueArray(ValueArray* array) {
    if (array->count == 0) {
        // Handle error: popping from an empty array
        return NIL_VAL; // Or some other error indicator
    }
    array->count--;
    return array->values[array->count];
}

Value removeValueArray(ValueArray* array, int index) {
    if (index < 0 || index >= array->count) {
        return NIL_VAL; // Index out of bounds
    }

    Value removedValue = array->values[index];

    // Shift elements to the left
    for (int i = index; i < array->count - 1; i++) {
        array->values[i] = array->values[i + 1];
    }

    array->count--;
    return removedValue;
}

// Prints a value.
void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL: 
            printf("nil"); 
            break;
        case VAL_NUMBER: {
            double num = AS_NUMBER(value);
            if (num == (long long)num && num >= LLONG_MIN && num <= LLONG_MAX) {
                printf("%lld", (long long)num);
            } else {
                printf("%.15g", num);
            }
            break;
        }
        case VAL_OBJ: 
            printObject(value); 
            break;
    }
}

// Checks if two values are equal.
bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
        default:         return false; // Unreachable.
    }
}
