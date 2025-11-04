#ifndef FLS_EXPR_H
#define FLS_EXPR_H

#include "lexer.h"
#include "value.h"

typedef struct Expr Expr;

typedef enum {
    EXPR_ASSIGN,
    EXPR_BINARY,
    EXPR_CALL,
    EXPR_GROUPING,
    EXPR_LITERAL,
    EXPR_LOGICAL,
    EXPR_UNARY,
    EXPR_VARIABLE
} ExprType;

struct Expr {
    ExprType type;
    union {
        struct { Token name; Expr* value; } assign;
        struct { Expr* left; Token operator; Expr* right; } binary;
        struct { Expr* callee; int argCount; Expr** arguments; } call;
        struct { Expr* expression; } grouping;
        struct { Value value; } literal;
        struct { Expr* left; Token operator; Expr* right; } logical;
        struct { Token operator; Expr* right; } unary;
        struct { Token name; } variable;
    } as;
};

Expr* newAssign(Token name, Expr* value);
Expr* newBinary(Expr* left, Token operator, Expr* right);
Expr* newCall(Expr* callee, Expr** arguments, int argCount);
Expr* newGrouping(Expr* expression);
Expr* newLiteral(Value value);
Expr* newLogical(Expr* left, Token operator, Expr* right);
Expr* newUnary(Token operator, Expr* right);
Expr* newVariable(Token name);

void freeExpr(Expr* expr);

#endif // FLS_EXPR_H
