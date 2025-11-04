#ifndef FLS_STD_IO_H
#define FLS_STD_IO_H

#include "value.h"

Value printNative(int argCount, Value* args);
Value printlnNative(int argCount, Value* args);

// File I/O
Value readFileNative(int argCount, Value* args);
Value writeFileNative(int argCount, Value* args);
Value appendFileNative(int argCount, Value* args);
Value pathExistsNative(int argCount, Value* args);
Value deleteFileNative(int argCount, Value* args);
Value renameNative(int argCount, Value* args);
Value createDirNative(int argCount, Value* args);
Value removeDirNative(int argCount, Value* args);
Value fileSizeNative(int argCount, Value* args);
Value isDirNative(int argCount, Value* args);
Value isFileNative(int argCount, Value* args);

// String utils
Value startsWithNative(int argCount, Value* args);
Value substringNative(int argCount, Value* args);
Value splitNative(int argCount, Value* args);

#endif // FLS_STD_IO_H
