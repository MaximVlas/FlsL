#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "parser.h"
#include "expr.h"
#include "error.h"
#include "object.h"

typedef struct {
    Lexer* lexer;
    Token current;
    Token previous;
    int hadError;
    int panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} Precedence;

typedef Expr* (*ParseFnPrefix)();
typedef Expr* (*ParseFnInfix)(Expr*);

typedef struct {
    ParseFnPrefix prefix;
    ParseFnInfix infix;
    Precedence precedence;
} ParseRule;

static Parser parser;

// Forward declarations to resolve ordering issues.
static Expr* expression();
static Stmt* statement();
static Stmt* declaration();
static ParseRule* getRule(TokenType type);
static Expr* parsePrecedence(Precedence precedence);
static Stmt* varDeclaration();
static Stmt* importStatement();
static Stmt* expressionStatement();
static Stmt* block();

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    const char* lineStart = token->start;
    while (lineStart > parser.source && *lineStart != '\n') {
        lineStart--;
    }
    if (*lineStart == '\n') {
        lineStart++;
    }

    const char* lineEnd = token->start;
    while (*lineEnd != '\0' && *lineEnd != '\n') {
        lineEnd++;
    }

    int col = (int)(token->start - lineStart) + 1;
    int lineLength = (int)(lineEnd - lineStart);
    if (lineLength > 1024) lineLength = 1024;
    
    char* lineStr = (char*)malloc((size_t)lineLength + 1);
    if (lineStr == NULL) {
        return;
    }
    memcpy(lineStr, lineStart, (size_t)lineLength);
    lineStr[lineLength] = '\0';

    reportError(true, parser.module->name->chars, token->line, lineStr, col, token->length, message);
    free(lineStr);
    parser.hadError = true;
}

static void error(const char* message) { errorAt(&parser.previous, message); }
static void errorAtCurrent(const char* message) { errorAt(&parser.current, message); }

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = scanToken(parser.lexer);
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static Expr* grouping() {
    Expr* expr = expression();
    consume(TOKEN_RPAREN, "Expect ')' after expression.");
    return newGrouping(expr);
}

static Expr* number() {
    double value = strtod(parser.previous.start, NULL);
    return newLiteral(NUMBER_VAL(value));
}

static Expr* string() {
    return newLiteral(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static Expr* variable() {
    return newVariable(parser.previous);
}

static Expr* unary() {
    Token operator = parser.previous;
    Expr* right = parsePrecedence(PREC_UNARY);
    return newUnary(operator, right);
}

static Expr* binary(Expr* left) {
    Token operator = parser.previous;
    ParseRule* rule = getRule(operator.type);
    Expr* right = parsePrecedence((Precedence)(rule->precedence + 1));
    return newBinary(left, operator, right);
}

static Expr* logical(Expr* left) {
    Token operator = parser.previous;
    ParseRule* rule = getRule(operator.type);
    Expr* right = parsePrecedence((Precedence)(rule->precedence + 1));
    return newLogical(left, operator, right);
}

static Expr* call(Expr* callee) {
    int argCount = 0;
    Expr** arguments = NULL;
    if (!check(TOKEN_RPAREN)) {
        do {
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            arguments = GROW_ARRAY(Expr*, arguments, argCount, argCount + 1);
            arguments[argCount++] = expression();
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expect ')' after arguments.");
    return newCall(callee, arguments, argCount);
}

static Expr* literal() {
    switch (parser.previous.type) {
        case TOKEN_FALSE: return newLiteral(BOOL_VAL(false));
        case TOKEN_NIL: return newLiteral(NIL_VAL);
        case TOKEN_TRUE: return newLiteral(BOOL_VAL(true));
        default: return NULL;
    }
}

ParseRule rules[] = {
    [TOKEN_LPAREN]      = {grouping, call,   PREC_CALL},
    [TOKEN_MINUS]       = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]        = {NULL,     binary, PREC_TERM},
    [TOKEN_SLASH]       = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]        = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]        = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]  = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL_EQUAL] = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]     = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]={NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]        = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]  = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]  = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]      = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]      = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]         = {NULL,     logical,PREC_AND},
    [TOKEN_OR]          = {NULL,     logical,PREC_OR},
    [TOKEN_FALSE]       = {literal,  NULL,   PREC_NONE},
    [TOKEN_NIL]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_TRUE]        = {literal,  NULL,   PREC_NONE},
};

static Expr* parsePrecedence(Precedence precedence) {
    advance();
    ParseFnPrefix prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return NULL;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    Expr* leftExpr = prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFnInfix infixRule = getRule(parser.previous.type)->infix;
        if (infixRule == NULL) break;
        leftExpr = infixRule(leftExpr);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        if (leftExpr->type == EXPR_VARIABLE) {
            Token name = leftExpr->as.variable.name;
            Expr* value = expression();
            freeExpr(leftExpr);
            return newAssign(name, value);
        }
        error("Invalid assignment target.");
    }

    return leftExpr;
}

static ParseRule* getRule(TokenType type) { return &rules[type]; }
static Expr* expression() { return parsePrecedence(PREC_ASSIGNMENT); }

static Stmt* block() {
    int capacity = 8;
    int count = 0;
    Stmt** statements = (Stmt**)malloc(sizeof(Stmt*) * capacity);
    if (statements == NULL) { error("Could not allocate memory for block."); return NULL; }

    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        if (count + 1 > capacity) {
            int oldCapacity = capacity;
            capacity = GROW_CAPACITY(oldCapacity);
            statements = (Stmt**)reallocate(statements, sizeof(Stmt*) * oldCapacity, sizeof(Stmt*) * capacity);
        }
        statements[count++] = declaration();
    }

    consume(TOKEN_RBRACE, "Expect '}' after block.");
    
    if (count + 1 > capacity) {
        statements = (Stmt**)reallocate(statements, sizeof(Stmt*) * capacity, sizeof(Stmt*) * (capacity + 1));
    }
    statements[count] = NULL;
    
    return newBlockStmt(statements);
}

static Stmt* printStatement() {
    Expr* value = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    return newPrintStmt(value);
}

static Stmt* returnStatement() {
    Token keyword = parser.previous;
    Expr* value = NULL;
    if (!check(TOKEN_SEMICOLON)) {
        value = expression();
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    return newReturnStmt(keyword, value);
}

static Stmt* ifStatement() {
    consume(TOKEN_LPAREN, "Expect '(' after 'if'.");
    Expr* condition = expression();
    consume(TOKEN_RPAREN, "Expect ')' after if condition.");

    Stmt* thenBranch = statement();
    Stmt* elseBranch = NULL;
    if (match(TOKEN_ELSE)) {
        elseBranch = statement();
    }

    return newIfStmt(condition, thenBranch, elseBranch);
}

static Stmt* whileStatement() {
    consume(TOKEN_LPAREN, "Expect '(' after 'while'.");
    Expr* condition = expression();
    consume(TOKEN_RPAREN, "Expect ')' after condition.");
    Stmt* body = statement();
    return newWhileStmt(condition, body);
}

static Stmt* forStatement() {
    consume(TOKEN_LPAREN, "Expect '(' after 'for'.");
    Stmt* initializer;
    if (match(TOKEN_SEMICOLON)) { initializer = NULL; } 
    else if (match(TOKEN_VAR)) { initializer = varDeclaration(); } 
    else { initializer = expressionStatement(); }

    Expr* condition = NULL;
    if (!match(TOKEN_SEMICOLON)) {
        condition = expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    }

    Expr* increment = NULL;
    if (!match(TOKEN_RPAREN)) {
        increment = expression();
        consume(TOKEN_RPAREN, "Expect ')' after for clauses.");
    }

    Stmt* body = statement();
    if (increment != NULL) {
        Stmt** blockStmts = (Stmt**)malloc(sizeof(Stmt*) * 3);
        blockStmts[0] = body;
        blockStmts[1] = newExpressionStmt(increment);
        blockStmts[2] = NULL;
        body = newBlockStmt(blockStmts);
    }
    
    if (condition == NULL) { condition = newLiteral(BOOL_VAL(true)); }
    body = newWhileStmt(condition, body);

    if (initializer != NULL) {
        Stmt** blockStmts = (Stmt**)malloc(sizeof(Stmt*) * 3);
        blockStmts[0] = initializer;
        blockStmts[1] = body;
        blockStmts[2] = NULL;
        body = newBlockStmt(blockStmts);
    }
    return body;
}

static Stmt* expressionStatement() {
    Expr* expr = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    return newExpressionStmt(expr);
}

static Stmt* function(char* kind) {
    (void)kind; // Unused parameter
    Token name;
    consume(TOKEN_IDENTIFIER, "Expect function name.");
    name = parser.previous;

    consume(TOKEN_LPAREN, "Expect '(' after function name.");
    
    int arity = 0;
    Token* params = NULL;
    if (!check(TOKEN_RPAREN)) {
        do {
            if (arity == 255) {
                error("Can't have more than 255 parameters.");
            }
            params = GROW_ARRAY(Token, params, arity, arity + 1);
            consume(TOKEN_IDENTIFIER, "Expect parameter name.");
            params[arity++] = parser.previous;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expect ')' after parameters.");
    consume(TOKEN_LBRACE, "Expect '{' before function body.");
    Stmt* body = block();
    
    return newFunctionStmt(name, params, arity, body);
}

static void synchronize() {
    parser.panicMode = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS: case TOKEN_FUN: case TOKEN_VAR:
            case TOKEN_FOR: case TOKEN_IF: case TOKEN_WHILE:
            case TOKEN_PRINT: case TOKEN_RETURN:
                return;
            default: ;
        }
        advance();
    }
}

static Stmt* statement() {
    if (match(TOKEN_PRINT)) return printStatement();
    if (match(TOKEN_FOR)) return forStatement();
    if (match(TOKEN_IF)) return ifStatement();
    if (match(TOKEN_RETURN)) return returnStatement();
    if (match(TOKEN_WHILE)) return whileStatement();
    if (match(TOKEN_LBRACE)) return block();
    return expressionStatement();
}

static Token parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return parser.previous;
}

static Stmt* varDeclaration() {
    Token global = parseVariable("Expect variable name.");
    Expr* initializer = NULL;
    if (match(TOKEN_EQUAL)) {
        initializer = expression();
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    return newVarStmt(global, initializer);
}

static Stmt* importStatement() {
    consume(TOKEN_STRING, "Expect module path string.");
    Expr* path = string(); // Re-use the existing string parsing logic
    consume(TOKEN_SEMICOLON, "Expect ';' after import statement.");
    return newImportStmt(path);
}

static Stmt* declaration() {
    if (match(TOKEN_EXPORT)) {
        return newExportStmt(declaration());
    }
    if (match(TOKEN_IMPORT)) return importStatement();
    if (match(TOKEN_FUN)) return function("function");
    if (match(TOKEN_VAR)) return varDeclaration();
    Stmt* stmt = statement();
    if (parser.panicMode) synchronize();
    return stmt;
}

Stmt** parse(const char* source) {
    Lexer lexer;
    initLexer(&lexer, source);
    parser.lexer = &lexer;
    parser.hadError = 0;
    parser.panicMode = 0;

    int capacity = 8;
    int count = 0;
    Stmt** statements = (Stmt**)malloc(sizeof(Stmt*) * capacity);
    if (statements == NULL) { error("Could not allocate memory for parser."); return NULL; }

    advance();
    while (!match(TOKEN_EOF)) {
        if (count + 1 > capacity) {
            int oldCapacity = capacity;
            capacity = GROW_CAPACITY(oldCapacity);
            statements = (Stmt**)reallocate(statements, sizeof(Stmt*) * oldCapacity, sizeof(Stmt*) * capacity);
        }
        statements[count++] = declaration();
    }
    
    if (count + 1 > capacity) {
        statements = (Stmt**)reallocate(statements, sizeof(Stmt*) * capacity, sizeof(Stmt*) * (capacity + 1));
    }
    statements[count] = NULL;

    if (parser.hadError) {
        for (int i = 0; i < count; i++) {
            freeStmt(statements[i]);
        }
        free(statements);
        return NULL;
    }

    return statements;
}
