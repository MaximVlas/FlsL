#include <stdio.h>
#include <string.h>

#include "dict.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

// Creates a new, empty dictionary (map).
Value newDictNative(int argCount, Value* args) {
    if (argCount != 0) {
        runtimeError("newDict() takes no arguments.");
        return NIL_VAL;
    }
    return OBJ_VAL(newMap());
}

// Sets a key-value pair in a dictionary.
Value dictSetNative(int argCount, Value* args) {
    if (argCount != 3 || !IS_MAP(args[0]) || !IS_STRING(args[1])) {
        runtimeError("dictSet() expects a dictionary, a string key, and a value.");
        return NIL_VAL;
    }
    ObjMap* map = AS_MAP(args[0]);
    ObjString* key = AS_STRING(args[1]);
    tableSet(&map->table, key, args[2]);
    return NIL_VAL; // Or maybe return the value?
}

// Gets a value from a dictionary.
Value dictGetNative(int argCount, Value* args) {
    if (argCount != 2 || !IS_MAP(args[0]) || !IS_STRING(args[1])) {
        runtimeError("dictGet() expects a dictionary and a string key.");
        return NIL_VAL;
    }
    ObjMap* map = AS_MAP(args[0]);
    ObjString* key = AS_STRING(args[1]);
    Value value;
    if (tableGet(&map->table, key, &value)) {
        return value;
    }
    return NIL_VAL; // Key not found
}

// Deletes a key-value pair from a dictionary.
Value dictDeleteNative(int argCount, Value* args) {
    if (argCount != 2 || !IS_MAP(args[0]) || !IS_STRING(args[1])) {
        runtimeError("dictDelete() expects a dictionary and a string key.");
        return NIL_VAL;
    }
    ObjMap* map = AS_MAP(args[0]);
    ObjString* key = AS_STRING(args[1]);
    return BOOL_VAL(tableDelete(&map->table, key));
}

// Checks if a key exists in a dictionary.
Value dictExistsNative(int argCount, Value* args) {
    if (argCount != 2 || !IS_MAP(args[0]) || !IS_STRING(args[1])) {
        runtimeError("dictExists() expects a dictionary and a string key.");
        return NIL_VAL;
    }
    ObjMap* map = AS_MAP(args[0]);
    ObjString* key = AS_STRING(args[1]);
    Value value;
    return BOOL_VAL(tableGet(&map->table, key, &value));
}
