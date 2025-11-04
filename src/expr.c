#include <stdio.h>
#include <stdlib.h>
#include "expr.h"
#include "memory.h"

static Expr* allocateExpr(ExprType type) {
    Expr* expr = (Expr*)malloc(sizeof(Expr));
    if (expr == NULL) {
        fprintf(stderr, "Fatal: Ran out of memory.\n");
        exit(1);
    }
    expr->type = type;
    return expr;
}

Expr* newAssign(Token name, Expr* value) {
    Expr* expr = allocateExpr(EXPR_ASSIGN);
    expr->as.assign.name = name;
    expr->as.assign.value = value;
    return expr;
}

Expr* newBinary(Expr* left, Token operator, Expr* right) {
    Expr* expr = allocateExpr(EXPR_BINARY);
    expr->as.binary.left = left;
    expr->as.binary.operator = operator;
    expr->as.binary.right = right;
    return expr;
}

Expr* newCall(Expr* callee, Expr** arguments, int argCount) {
    Expr* expr = allocateExpr(EXPR_CALL);
    expr->as.call.callee = callee;
    expr->as.call.arguments = arguments;
    expr->as.call.argCount = argCount;
    return expr;
}

Expr* newGrouping(Expr* expression) {
    Expr* expr = allocateExpr(EXPR_GROUPING);
    expr->as.grouping.expression = expression;
    return expr;
}

Expr* newLiteral(Value value) {
    Expr* expr = allocateExpr(EXPR_LITERAL);
    expr->as.literal.value = value;
    return expr;
}

Expr* newLogical(Expr* left, Token operator, Expr* right) {
    Expr* expr = allocateExpr(EXPR_LOGICAL);
    expr->as.logical.left = left;
    expr->as.logical.operator = operator;
    expr->as.logical.right = right;
    return expr;
}

Expr* newUnary(Token operator, Expr* right) {
    Expr* expr = allocateExpr(EXPR_UNARY);
    expr->as.unary.operator = operator;
    expr->as.unary.right = right;
    return expr;
}

Expr* newVariable(Token name) {
    Expr* expr = allocateExpr(EXPR_VARIABLE);
    expr->as.variable.name = name;
    return expr;
}

void freeExpr(Expr* expr) {
    if (expr == NULL) return;

    switch (expr->type) {
        case EXPR_ASSIGN:
            freeExpr(expr->as.assign.value);
            break;
        case EXPR_BINARY:
            freeExpr(expr->as.binary.left);
            freeExpr(expr->as.binary.right);
            break;
        case EXPR_CALL:
            freeExpr(expr->as.call.callee);
            for (int i = 0; i < expr->as.call.argCount; i++) {
                freeExpr(expr->as.call.arguments[i]);
            }
            FREE_ARRAY(Expr*, expr->as.call.arguments, expr->as.call.argCount);
            break;
        case EXPR_GROUPING:
            freeExpr(expr->as.grouping.expression);
            break;
        case EXPR_LITERAL:
            break;
        case EXPR_LOGICAL:
            freeExpr(expr->as.logical.left);
            freeExpr(expr->as.logical.right);
            break;
        case EXPR_UNARY:
            freeExpr(expr->as.unary.right);
            break;
        case EXPR_VARIABLE:
            break;
    }
    free(expr);
}
