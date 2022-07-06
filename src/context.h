#ifndef CONTEXT_h
#define CONTEXT_h

#include "lexer.h"
#include "common.h"
#include "array.h"
#include "ast.h"

#define CONTEXT_SCRATCH_SIZE 1024 * 3
#define CONTEXT_STACK_SIZE   64

#define UNDEFINED_OBJECT_INDEX 0
#define NULL_OBJECT_INDEX 1
#define TRUE_OBJECT_INDEX 2
#define FALSE_OBJECT_INDEX 3

#define PRINT_INSTRUCTIONS_DURING_COMPILE 0

char *read_file(const char *path);

typedef struct Scope Scope;

typedef enum ObjectTag {
    OBJECT_UNDEFINED,

    OBJECT_INTEGER,
    OBJECT_FLOATING,
    OBJECT_STRING,
    OBJECT_BOOLEAN,
    OBJECT_NULL,
    OBJECT_SCOPE,
    OBJECT_LAMBDA,
} ObjectTag;

typedef struct ObjectString {
    char *data;
    u64 length;
} ObjectString;

typedef struct Object {
    union {
        s64 integer;
        f64 floating;
        u8  boolean;
        ObjectString string;
        Scope *scope;
        void *pointer;
    };
    ObjectTag tag;
} Object;

typedef struct Stack {
    Object data[CONTEXT_STACK_SIZE];
    u64 top;
} Stack;

typedef struct CallStack {
    Scope *data[CONTEXT_STACK_SIZE];
    u64 top;
} CallStack;

typedef enum Op {
    CONST,

    LOAD,
    LOAD_PC,
    LOAD_ARG,
    LOAD_SCOPE,

    STORE,
    STORE_ARG_OR_RETVAL,

    LOCAL_JUMP,
    BEGIN_FUNC,
    END_FUNC,

    CALL_FUNC,
    RETURN,

    PRINT,

    EQUALS,
    ADD,
    SUB,
    MUL,
    DIV,
    NEG,
    
    HALT,
} Op;
static const char *instruction_strings[20] = {
    "CONST",
    "LOAD",
    "LOAD_PC",
    "LOAD_ARG",
    "LOAD_SCOPE",
    "STORE",
    "STORE_ARG_OR_RETVAL",
    "LOCAL_JUMP",
    "BEGIN_FUNC",
    "END_FUNC",
    "CALL_FUNC",
    "RETURN",
    "PRINT",
    "EQUALS",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "NEG",
    "HALT",
};

typedef struct Instruction {
    Op op; // sizeof(int) in C - we will assume it's 32 bits
    s32 arg;
    u64 line_number;
} Instruction;

typedef Array(Object) Constants;
typedef Array(Instruction) Instructions;

typedef struct Interp {
    Instructions instructions;
    u64 pc;
    u64 last_jump_loc;

    Stack call_storage;
    Stack jump_stack; // TODO: make this not use Objects cus thats slow and bad
    CallStack call_stack;

    Scope *root_scope;
    Scope *scope;

    StringAllocator strings;

    Op    last_op;
    u64   error_count;
    char *file_name;
} Interp;

struct Scope {
    Constants    constant_pool;
    Ast          ast;
    Stack        stack;

    struct Scope *parent;
};

AstNode *find_decl(Scope *scope, char *name);

Interp compile(Ast ast, char *file_name);
void run_interpreter(Interp *interp);
void free_interpreter(Interp *interp);

// Tree-walk interpreter
void run_interpreter_slow(Ast ast, char *file_name);

void     stack_init(Stack *);
void     stack_push(Stack *, Object);
Object   stack_pop(Stack *);
Object   stack_top(Stack);


void frame_push(CallStack *s, Scope *frame);
Scope *frame_pop(CallStack *s);
Scope *frame_top(CallStack s);

/*
struct Module {
    Ast       ast;
    TokenList tokens;
};
*/

#endif
