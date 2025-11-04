#ifndef FLS_DICT_H
#define FLS_DICT_H

#include "value.h"

Value newDictNative(int argCount, Value* args);
Value dictSetNative(int argCount, Value* args);
Value dictGetNative(int argCount, Value* args);
Value dictDeleteNative(int argCount, Value* args);
Value dictExistsNative(int argCount, Value* args);

#endif // FLS_DICT_H
