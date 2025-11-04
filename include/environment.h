#ifndef FLS_ENVIRONMENT_H
#define FLS_ENVIRONMENT_H

#include "common.h"
#include "table.h"
#include "value.h"
#include "lexer.h" // Added to define the Token type

// An Environment struct holds the variables for a single scope.
// It points to the enclosing (parent) scope, creating a linked list.
typedef struct Environment {
    struct Environment* enclosing;
    Table values;
} Environment;

// Initializes an environment.
void initEnvironment(Environment* environment, Environment* enclosing);

// Frees the table within an environment.
void freeEnvironment(Environment* environment);

// Defines a new variable in the current scope.
void defineVariable(Environment* environment, ObjString* name, Value value);

// Gets a variable's value from the environment chain.
bool getVariable(Environment* environment, Token* name, Value* value);

// Assigns a new value to an existing variable in the environment chain.
bool assignVariable(Environment* environment, Token* name, Value value);

#endif // FLS_ENVIRONMENT_H
