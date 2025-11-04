#ifndef FLS_STMT_H
#define FLS_STMT_H

#include "expr.h"

typedef struct Stmt Stmt;

typedef enum {
    STMT_BLOCK,
    STMT_EXPRESSION,
    STMT_FUNCTION,
    STMT_IF,
    STMT_PRINT,
    STMT_RETURN,
    STMT_VAR,
    STMT_WHILE,
    STMT_IMPORT,
    STMT_EXPORT,
} StmtType;

struct Stmt {
    StmtType type;
    union {
        struct { Stmt** statements; } block;
        struct { Expr* expression; } expression;
        struct { Token name; Token* params; int arity; Stmt* body; } function;
        struct { Expr* condition; Stmt* thenBranch; Stmt* elseBranch; } ifStmt;
        struct { Expr* expression; } print;
        struct { Token keyword; Expr* value; } returnStmt;
        struct { Token name; Expr* initializer; } var;
        struct { Expr* condition; Stmt* body; } whileStmt;
        struct { Expr* path; } importStmt;
        struct { Stmt* declaration; } exportStmt;
    } as;
};

Stmt* newBlockStmt(Stmt** statements);
Stmt* newExpressionStmt(Expr* expression);
Stmt* newFunctionStmt(Token name, Token* params, int arity, Stmt* body);
Stmt* newIfStmt(Expr* condition, Stmt* thenBranch, Stmt* elseBranch);
Stmt* newPrintStmt(Expr* expression);
Stmt* newReturnStmt(Token keyword, Expr* value);
Stmt* newVarStmt(Token name, Expr* initializer);
Stmt* newWhileStmt(Expr* condition, Stmt* body);
Stmt* newImportStmt(Expr* path);
Stmt* newExportStmt(Stmt* declaration);

void freeStmt(Stmt* stmt);

#endif // FLS_STMT_H
