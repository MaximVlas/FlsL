#include <string.h>

#include "environment.h"
#include "memory.h"
#include "object.h"
#include "interpreter.h" // For runtimeError

void initEnvironment(Environment* environment, Environment* enclosing) {
    environment->enclosing = enclosing;
    initTable(&environment->values);
}

void freeEnvironment(Environment* environment) {
    freeTable(&environment->values);
}

void defineVariable(Environment* environment, ObjString* name, Value value) {
    tableSet(&environment->values, name, value);
}

bool getVariable(Environment* environment, Token* name, Value* value) {
    ObjString* key = copyString(name->start, name->length);
    if (tableGet(&environment->values, key, value)) {
        return true;
    }

    if (environment->enclosing != NULL) {
        return getVariable(environment->enclosing, name, value);
    }

    runtimeError(*name, "Undefined variable '%.*s'.", name->length, name->start);
    return false;
}

bool assignVariable(Environment* environment, Token* name, Value value) {
    ObjString* key = copyString(name->start, name->length);
    if (tableSet(&environment->values, key, value)) {
        // If tableSet returns true, it was a new key. For assignment, the key
        // should already exist. We check the enclosing environment.
        if (environment->enclosing != NULL) {
            tableDelete(&environment->values, key); // Remove the mistakenly added key.
            return assignVariable(environment->enclosing, name, value);
        }
        tableDelete(&environment->values, key); // Clean up.
        runtimeError(*name, "Undefined variable '%.*s'.", name->length, name->start);
        return false;
    }
    return true;
}
