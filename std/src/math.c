#define _GNU_SOURCE  // For feature test macros
#include <math.h>
#include <stdbool.h>

#include "math.h"
#include "object.h"
#include "vm.h"

// Declare math functions we'll be using
double sqrt(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double fabs(double x);
double pow(double x, double y);
double log(double x);
double log10(double x);
double exp(double x);
double floor(double x);
double ceil(double x);
double round(double x);
double fmod(double x, double y);

#define NATIVE_MATH_FUNC(name, c_func) \
Value name(int argCount, Value* args) { \
    if (argCount != 1) { \
        runtimeError(#c_func "() takes exactly 1 argument (%d given).", argCount); \
        return NIL_VAL; \
    } \
    if (!IS_NUMBER(args[0])) { \
        runtimeError(#c_func "() argument must be a number."); \
        return NIL_VAL; \
    } \
    return NUMBER_VAL(c_func(AS_NUMBER(args[0]))); \
}

#define NATIVE_MATH_FUNC_2(name, c_func) \
Value name(int argCount, Value* args) { \
    if (argCount != 2) { \
        runtimeError(#c_func "() takes exactly 2 arguments (%d given).", argCount); \
        return NIL_VAL; \
    } \
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) { \
        runtimeError(#c_func "() arguments must be numbers."); \
        return NIL_VAL; \
    } \
    return NUMBER_VAL(c_func(AS_NUMBER(args[0]), AS_NUMBER(args[1]))); \
}

NATIVE_MATH_FUNC(sqrtNative, sqrt)
NATIVE_MATH_FUNC(sinNative, sin)
NATIVE_MATH_FUNC(cosNative, cos)
NATIVE_MATH_FUNC(tanNative, tan)
NATIVE_MATH_FUNC(absNative, fabs)
NATIVE_MATH_FUNC_2(powNative, pow)
NATIVE_MATH_FUNC(logNative, log)
NATIVE_MATH_FUNC(log10Native, log10)
NATIVE_MATH_FUNC(expNative, exp)
NATIVE_MATH_FUNC(floorNative, floor)
NATIVE_MATH_FUNC(ceilNative, ceil)
NATIVE_MATH_FUNC(roundNative, round)
NATIVE_MATH_FUNC_2(fmodNative, fmod)

void initMathLibrary() {
    defineNative("sqrt", sqrtNative);
    defineNative("sin", sinNative);
    defineNative("cos", cosNative);
    defineNative("tan", tanNative);
    defineNative("fabs", absNative);
    defineNative("pow", powNative);
    defineNative("log", logNative);
    defineNative("log10", log10Native);
    defineNative("exp", expNative);
    defineNative("floor", floorNative);
    defineNative("ceil", ceilNative);
    defineNative("round", roundNative);
    defineNative("fmod", fmodNative);

    defineGlobal("PI", NUMBER_VAL(3.14159265358979323846));
}
