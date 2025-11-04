#include <time.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "vm.h"
#include "object.h"
#include "value.h"

// State for the xorshift* PRNG
static uint64_t state;

// Function to seed the random number generator
static void seedRandom() {
    state = (uint64_t)time(NULL);
    if (state == 0) { // time() can fail, provide a fallback
        state = 0xdeadbeefcafebabe;
    }
}

// xorshift* generator
static uint64_t random_uint64() {
    uint64_t x = state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

// Native 'random' function: returns a random float between 0 and 1.
static Value randomNative(int argCount, Value* args) {
    if (argCount != 0) {
        runtimeError("random() takes no arguments.");
        return NIL_VAL;
    }
    double random_double = (double)(random_uint64() >> 11) * 0x1.0p-53;
    return NUMBER_VAL(random_double);
}

// Native 'randomInt' function: returns a random integer between min and max (inclusive).
static Value randomIntNative(int argCount, Value* args) {
    if (argCount != 2) {
        runtimeError("randomInt() takes 2 arguments (min, max).");
        return NIL_VAL;
    }
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        runtimeError("Arguments must be numbers.");
        return NIL_VAL;
    }

    int min = (int)AS_NUMBER(args[0]);
    int max = (int)AS_NUMBER(args[1]);

    if (min > max) {
        runtimeError("min cannot be greater than max.");
        return NIL_VAL;
    }

    // Correctly handle the range calculation for the modulo operator.
    uint64_t range = (uint64_t)max - (uint64_t)min + 1;
    return NUMBER_VAL(min + (double)(random_uint64() % range));
}

void initRandomLibrary() {
    seedRandom();
    defineNative("random", randomNative);
    defineNative("randomInt", randomIntNative);
}
