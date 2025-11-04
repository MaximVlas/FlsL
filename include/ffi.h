#ifndef fls_ffi_h
#define fls_ffi_h

#include "value.h"

Value ffiLoadNative(int argCount, Value* args);
Value ffiGetNative(int argCount, Value* args);
Value ffiCallNative(int argCount, Value* args);
Value ffiCloseNative(int argCount, Value* args);

#endif
