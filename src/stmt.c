#include <stdio.h>
#include <stdlib.h>
#include "stmt.h"
#include "memory.h"

static Stmt* allocateStmt(StmtType type) {
    Stmt* stmt = (Stmt*)malloc(sizeof(Stmt));
    if (stmt == NULL) {
        fprintf(stderr, "Fatal: Ran out of memory.\n");
        exit(1);
    }
    stmt->type = type;
    return stmt;
}

Stmt* newBlockStmt(Stmt** statements) {
    Stmt* stmt = allocateStmt(STMT_BLOCK);
    stmt->as.block.statements = statements;
    return stmt;
}

Stmt* newExpressionStmt(Expr* expression) {
    Stmt* stmt = allocateStmt(STMT_EXPRESSION);
    stmt->as.expression.expression = expression;
    return stmt;
}

Stmt* newFunctionStmt(Token name, Token* params, int arity, Stmt* body) {
    Stmt* stmt = allocateStmt(STMT_FUNCTION);
    stmt->as.function.name = name;
    stmt->as.function.params = params;
    stmt->as.function.arity = arity;
    stmt->as.function.body = body;
    return stmt;
}

Stmt* newIfStmt(Expr* condition, Stmt* thenBranch, Stmt* elseBranch) {
    Stmt* stmt = allocateStmt(STMT_IF);
    stmt->as.ifStmt.condition = condition;
    stmt->as.ifStmt.thenBranch = thenBranch;
    stmt->as.ifStmt.elseBranch = elseBranch;
    return stmt;
}

Stmt* newPrintStmt(Expr* expression) {
    Stmt* stmt = allocateStmt(STMT_PRINT);
    stmt->as.print.expression = expression;
    return stmt;
}

Stmt* newReturnStmt(Token keyword, Expr* value) {
    Stmt* stmt = allocateStmt(STMT_RETURN);
    stmt->as.returnStmt.keyword = keyword;
    stmt->as.returnStmt.value = value;
    return stmt;
}

Stmt* newVarStmt(Token name, Expr* initializer) {
    Stmt* stmt = allocateStmt(STMT_VAR);
    stmt->as.var.name = name;
    stmt->as.var.initializer = initializer;
    return stmt;
}

Stmt* newWhileStmt(Expr* condition, Stmt* body) {
    Stmt* stmt = allocateStmt(STMT_WHILE);
    stmt->as.whileStmt.condition = condition;
    stmt->as.whileStmt.body = body;
    return stmt;
}

Stmt* newImportStmt(Expr* path) {
    Stmt* stmt = allocateStmt(STMT_IMPORT);
    stmt->as.importStmt.path = path;
    return stmt;
}

Stmt* newExportStmt(Stmt* declaration) {
    Stmt* stmt = allocateStmt(STMT_EXPORT);
    stmt->as.exportStmt.declaration = declaration;
    return stmt;
}

void freeStmt(Stmt* stmt) {
    if (stmt == NULL) return;

    switch (stmt->type) {
        case STMT_BLOCK: {
            for (int i = 0; stmt->as.block.statements[i] != NULL; i++) {
                freeStmt(stmt->as.block.statements[i]);
            }
            free(stmt->as.block.statements);
            break;
        }
        case STMT_EXPRESSION:
            freeExpr(stmt->as.expression.expression);
            break;
        case STMT_FUNCTION:
            // The ObjFunction owns the body and params, so we don't free them here.
            // The garbage collector will handle the ObjFunction.
            break;
        case STMT_IF:
            freeExpr(stmt->as.ifStmt.condition);
            freeStmt(stmt->as.ifStmt.thenBranch);
            if (stmt->as.ifStmt.elseBranch != NULL) {
                freeStmt(stmt->as.ifStmt.elseBranch);
            }
            break;
        case STMT_PRINT:
            freeExpr(stmt->as.print.expression);
            break;
        case STMT_RETURN:
            if (stmt->as.returnStmt.value != NULL) {
                freeExpr(stmt->as.returnStmt.value);
            }
            break;
        case STMT_VAR:
            if (stmt->as.var.initializer != NULL) {
                freeExpr(stmt->as.var.initializer);
            }
            break;
        case STMT_WHILE:
            freeExpr(stmt->as.whileStmt.condition);
            freeStmt(stmt->as.whileStmt.body);
            break;
        case STMT_IMPORT:
            freeExpr(stmt->as.importStmt.path);
            break;
        case STMT_EXPORT:
            freeStmt(stmt->as.exportStmt.declaration);
            break;
    }
    free(stmt);
}
