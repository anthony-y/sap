#ifndef CONTEXT_h
#define CONTEXT_h

#include "lexer.h"
#include "common.h"
#include "array.h"
#include "ast.h"

#define CONTEXT_SCRATCH_SIZE 1024 * 3
#define CONTEXT_STACK_SIZE   32

#define UNDEFINED_OBJECT_INDEX 0
#define NULL_OBJECT_INDEX 1
#define TRUE_OBJECT_INDEX 2
#define FALSE_OBJECT_INDEX 3

#define PRINT_INSTRUCTIONS_DURING_COMPILE 0

char *read_file(const char *path);

typedef enum ObjectTag {
    OBJECT_UNDEFINED,

    OBJECT_INTEGER,
    OBJECT_FLOATING,
    OBJECT_STRING,
    OBJECT_BOOLEAN,
    OBJECT_NULL,
} ObjectTag;

typedef struct Object {
    union {
        s64 integer;
        f64 floating;
        u8 *pointer;
        u8  boolean;
    };
    ObjectTag tag;
} Object;

typedef struct Stack {
    Object data[CONTEXT_STACK_SIZE];
    u64 top;
} Stack;

typedef enum Op {
    CONST,
    STORE,
    ADD,
    LOAD,
    PRINT,
    EQUALS,
    SUB,
    MUL,
    DIV,
    NEG,
} Op;
static const char *instruction_strings[10] = {
    "CONST",
    "STORE",
    "ADD",
    "LOAD",
    "PRINT",
    "EQUALS",
    "SUB",
    "MUL",
    "DIV",
    "NEG",
};

typedef struct Instruction {
    Op op; // sizeof(int) in C - we will assume it's 32 bits
    s32 arg;
    u64 line_number;
} Instruction;

typedef Array(Object) Constants;
typedef Array(Instruction) Instructions;

typedef struct Interp {
    Constants    constant_pool;
    Instructions instructions;
    Ast          ast;
    Stack        stack;

    Op    last_op;
    u64   error_count;
    char *file_name;
} Interp;

AstNode *find_decl(Ast ast, char *name);

Interp compile(Ast ast, char *file_name);
void run_interpreter(Interp *interp);

void     stack_init(Stack *);
void     stack_push(Stack *, Object);
Object   stack_pop(Stack *);
Object   stack_top(Stack);

/*
struct Module {
    Ast       ast;
    TokenList tokens;
};
*/

#endif
