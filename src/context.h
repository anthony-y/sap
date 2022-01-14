#ifndef CONTEXT_h
#define CONTEXT_h

#include "lexer.h"
#include "common.h"
#include "token.h"
#include "arena.h"
#include "array.h"
#include "ast.h"

#include <stdarg.h>

#define CONTEXT_SCRATCH_SIZE 1024 * 3
#define CONTEXT_STACK_SIZE   32

typedef enum ObjectTag {
    OBJECT_UNDEFINED,

    OBJECT_INTEGER,
    OBJECT_FLOATING,
    OBJECT_STRING,
    OBJECT_NULL,
} ObjectTag;

typedef struct Object {
    union {
        s64 integer;
        f64 floating;
        u8 *pointer;
    };
    ObjectTag tag;
} Object;

typedef struct Stack {
    Object data[CONTEXT_STACK_SIZE];
    u64 top;
} Stack;

typedef enum Op {
    CONST,
    PRINT_CONST,
    STORE,
    ADD,
    LOAD,
    PRINT_VAR,
} Op;
static const char *instruction_strings[10] = {
    "CONST",
    "PRINT_CONST",
    "STORE",
    "ADD",
    "LOAD",
    "PRINT_VAR",
};

typedef struct Instruction {
    Op op; // sizeof(int) in C - we will assume it's 32 bits
    s32 arg;
} Instruction;

typedef Array(Object) Constants;
typedef Array(Instruction) Instructions;

typedef struct Interp {
    Constants    constant_pool;
    Instructions instructions;
    Ast          ast;
    Stack        stack;

    u64 error_count;
} Interp;

Interp compile(Ast ast);
void run_interpreter(Interp *interp);
AstNode *find_decl(Ast ast, char *name);
void stack_init(Stack *);
void stack_push(Stack *, Object);
Object stack_pop(Stack *);
Object stack_top(Stack);

/*
struct Module {
    Ast       ast;
    TokenList tokens;
};

void compile_error(Context *, Token, const char *fmt, ...);
void compile_error_start(Context *, Token, const char *fmt, ...);
void compile_error_add_line(Context *, const char *fmt, ...);
void compile_error_end();
void compile_warning(Context *, Token, const char *fmt, ...);
*/

char *read_file(const char *path);

#endif
