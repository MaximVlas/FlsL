#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "lexer.h"
#include "object.h"
#include "error.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// Parser structure to hold state during compilation.
typedef struct {
    Lexer* lexer;
    Token current;
    Token previous;
    ObjModule* module;
    bool hadError;
    bool panicMode;
} Parser;

// Precedence levels for expressions, from lowest to highest.
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * / %
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

// Function pointer types for parsing.
typedef void (*ParseFn)(bool canAssign);

// A rule for parsing a specific token type, including prefix/infix handlers and precedence.
typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

// Local variable representation.
typedef struct {
    Token name;
    int depth;
} Local;

// Type of function being compiled (script-level, or a user-defined function).
typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

// Compiler state for a single function.
typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

Parser parser;
Compiler* current = NULL;

// Gets the chunk for the function currently being compiled.
static Chunk* currentChunk() {
    return &current->function->chunk;
}

// Reports an error at a specific token.
static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    const char* lineStart = token->start;
    while (lineStart > parser.lexer->start && *(lineStart - 1) != '\n') {
        lineStart--;
    }

    const char* lineEnd = token->start;
    while (*lineEnd != '\0' && *lineEnd != '\n') {
        lineEnd++;
    }

    int col = (int)(token->start - lineStart) + 1;
    int lineLength = (int)(lineEnd - lineStart);
    char* lineStr = (char*)malloc(lineLength + 1);
    if (lineStr == NULL) {
        fprintf(stderr, "Memory allocation failed in error reporting.\n");
        exit(1);
    }
    memcpy(lineStr, lineStart, lineLength);
    lineStr[lineLength] = '\0';

    const char* moduleName = parser.module->name != NULL ? parser.module->name->chars : "<script>";
    reportError(true, moduleName, token->line, lineStr, col, token->length, message);

    free(lineStr);
    parser.hadError = true;
}

// Reports an error at the previous token.
static void error(const char* message) {
    errorAt(&parser.previous, message);
}

// Reports an error at the current token.
static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

// Advances the parser to the next token.
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken(parser.lexer);
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

// Consumes the current token if it's of the expected type, otherwise reports an error.
static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

// Checks if the current token is of a given type.
static bool check(TokenType type) {
    return parser.current.type == type;
}

// Consumes the current token if it matches the type.
static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

// Emits a single byte to the current chunk.
static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

// Emits two bytes to the current chunk.
static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

// Emits a loop instruction.
static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

// Emits a jump instruction and returns its location for later patching.
static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

// Emits a return instruction.
static void emitReturn() {
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

// Creates a constant in the chunk and returns its index.
static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

// Emits an OP_CONSTANT instruction.
static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

// Patches a jump instruction at a given location to jump to the current position.
static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

// Initializes a new compiler.
static void initCompiler(Compiler* compiler, FunctionType type, ObjModule* module) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    compiler->function->module = module;
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

// Finishes compilation and returns the compiled function.
static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL
            ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

// Enters a new scope.
static void beginScope() {
    current->scopeDepth++;
}

// Exits the current scope, popping local variables.
static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
               current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    }
}

// Forward declarations for recursive parsing functions.
static void expression();
static void declaration();
static void statement();
static void funDeclaration(bool isExport);
static void varDeclaration(bool isExport);
static void importStatement();
static void list(bool canAssign);
static void subscript(bool canAssign);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

// Parses a binary operator.
static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:       emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emitByte(OP_ADD); break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
        case TOKEN_PERCENT:       emitByte(OP_MODULO); break;
        default: return; // Unreachable.
    }
}

// Parses a function call's argument list.
static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RPAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expect ')' after arguments.");
    return argCount;
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void list(bool canAssign) {
    emitByte(OP_NEW_LIST);
    if (!check(TOKEN_RBRACKET)) {
        do {
            expression();
            emitByte(OP_LIST_APPEND);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RBRACKET, "Expect ']' after list literal.");
}

static void subscript(bool canAssign) {
  expression();
  consume(TOKEN_RBRACKET, "Expect ']' after subscript.");

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitByte(OP_SET_SUBSCRIPT);
  } else {
    emitByte(OP_GET_SUBSCRIPT);
  }
}

// Parses a literal value (false, nil, true, number).
static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return; // Unreachable.
    }
}

// Parses a grouping expression `(...)`.
static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RPAREN, "Expect ')' after expression.");
}

// Parses a number literal.
static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

// Parses a logical 'or' expression.
static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

// Parses a string literal.
static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                    parser.previous.length - 2)));
}

// Checks if two identifiers are equal.
static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// Resolves a local variable by name.
static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

// Parses a variable name and adds it as a constant.
static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// Adds a local variable to the compiler's list.
static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1; // Mark as uninitialized
}

// Declares a local variable.
static void declareVariable() {
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

// Parses a variable name.
static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

// Marks the last declared local variable as initialized.
static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

// Defines a variable by emitting the appropriate instruction.
static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

// Parses a variable expression.
static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

// Parses a unary expression.
static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
}

// Parses a logical 'and' expression.
static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

// The table of parsing rules.
ParseRule rules[] = {
    [TOKEN_LPAREN]        = {grouping, call,   PREC_CALL},
    [TOKEN_LBRACKET]      = {list,     subscript,   PREC_CALL},
    [TOKEN_RPAREN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LBRACE]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RBRACE]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_PERCENT]       = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

// Main parsing function that dispatches based on precedence.
static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

// Retrieves the parsing rule for a given token type.
static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

// Parses an expression.
static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

// Parses a block of statements `{...}`.
static void block() {
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RBRACE, "Expect '}' after block.");
}

// Parses a function declaration.
static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type, current->function->module);
    beginScope();

    consume(TOKEN_LPAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RPAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expect ')' after parameters.");
    consume(TOKEN_LBRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
}

// Parses a top-level function declaration.
static void importStatement() {
    consume(TOKEN_STRING, "Expect module path string.");
    // Emit the constant and then the import instruction.
    Value modulePath = OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2));
    emitConstant(modulePath);
    emitByte(OP_IMPORT);
    consume(TOKEN_SEMICOLON, "Expect ';' after import statement.");
}

static void funDeclaration(bool isExport) {
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);

    if (isExport) {
        emitBytes(OP_EXPORT, global);
    }
}

// ...

static void varDeclaration(bool isExport) {
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);

    if (isExport) {
        emitBytes(OP_EXPORT, global);
    }
}

// Parses an expression statement.
static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

// Parses a for statement.
static void forStatement() {
    beginScope();
    consume(TOKEN_LPAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(TOKEN_VAR)) {
        varDeclaration(false);
    } else {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.
    }

    if (!match(TOKEN_RPAREN)) {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RPAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition.
    }

    endScope();
}

// Parses an if statement.
static void ifStatement() {
    consume(TOKEN_LPAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RPAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}

// Parses a print statement.


// Parses a return statement.
static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

// Parses a while statement.
static void whileStatement() {
    int loopStart = currentChunk()->count;
    consume(TOKEN_LPAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RPAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

// Resynchronizes the parser after an error to avoid cascade errors.
static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ; // Do nothing.
        }

        advance();
    }
}

// Parses a statement.


// Parses a statement.
static void statement() {
    if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LBRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

// Parses a declaration (variable or function).
static void declaration() {
    bool isExport = match(TOKEN_EXPORT);

    if (match(TOKEN_FUN)) {
        funDeclaration(isExport);
    } else if (match(TOKEN_VAR)) {
        varDeclaration(isExport);
    } else if (match(TOKEN_IMPORT)) {
        if (isExport) {
            error("Cannot export an import statement.");
        }
        importStatement();
    } else {
        if (isExport) {
            error("Can only export function and variable declarations.");
        }
        statement();
    }

    if (parser.panicMode) synchronize();
}

// Main compilation function.
ObjFunction* compile(const char* source, ObjModule* module) {
    Lexer lexer;
    initLexer(&lexer, source);
    parser.lexer = &lexer;
    parser.module = module;

    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT, module);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}
