#ifndef CONTEXT_h
#define CONTEXT_h

#include "lexer.h"
#include "common.h"
#include "array.h"
#include "ast.h"

#define CONTEXT_SCRATCH_SIZE 1024 * 3
#define CONTEXT_STACK_SIZE   128

#define UNDEFINED_OBJECT_INDEX 0
#define NULL_OBJECT_INDEX 1
#define TRUE_OBJECT_INDEX 2
#define FALSE_OBJECT_INDEX 3

#define PRINT_INSTRUCTIONS_DURING_COMPILE 0

char *read_file(const char *path);

typedef struct StackFrame StackFrame;

typedef enum ObjectTag {
    OBJECT_UNDEFINED,

    OBJECT_INTEGER,
    OBJECT_FLOATING,
    OBJECT_STRING,
    OBJECT_BOOLEAN,
    OBJECT_NULL,
    OBJECT_SCOPE,
    OBJECT_LAMBDA,
    OBJECT_ARRAY,
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
        Array(struct Object) array;
        StackFrame *scope;
        void *pointer;
    };
    bool non_mutable;
    ObjectTag tag;
} Object;

typedef struct Stack {
    Object data[CONTEXT_STACK_SIZE];
    u64 top;
} Stack;

typedef struct CallStack {
    StackFrame *data[CONTEXT_STACK_SIZE];
    u64 top;
} CallStack;

typedef struct BlockStack {
    AstNode *blocks[CONTEXT_STACK_SIZE];
    int top;
} BlockStack;

typedef enum Op {
    CONST,

    LOAD,
    LOAD_PC,
    LOAD_ARG,
    LOAD_SCOPE,

    STORE,
    STORE_ARG_OR_RETVAL,

    CALL_FUNC,
    POP_SCOPE_RETURN,
    
    POP_SCOPE,

    JUMP,
    JUMP_TRUE,
    JUMP_FALSE,

    BEGIN_BLOCK,
    END_BLOCK,

    PRINT,
    APPEND,

    EQUALS,
    LESS_THAN_EQUALS,
    GREATER_THAN_EQUALS,
    LESS_THAN,
    GREATER_THAN,

    ADD,
    SUB,
    MUL,
    DIV,
    NEG,
    
    HALT,
} Op;
static const char *instruction_strings[28] = {
    "CONST",
    "LOAD",
    "LOAD_PC",
    "LOAD_ARG",
    "LOAD_SCOPE",
    "STORE",
    "STORE_ARG_OR_RETVAL",
    "CALL_FUNC",
    "POP_SCOPE_RETURN",
    "POP_SCOPE",
    "JUMP",
    "JUMP_TRUE",
    "JUMP_FALSE",
    "BEGIN_BLOCK",
    "END_BLOCK",
    "PRINT",
    "APPEND",
    "EQUALS",
    "LESS_THAN_EQUALS",
    "GREATER_THAN_EQUALS",
    "LESS_THAN",
    "GREATER_THAN",
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

    Stack call_storage;
    Stack jump_stack; // TODO: make this not use Objects cus thats slow and bad
    CallStack call_stack;

    StackFrame *root_scope;
    StackFrame *scope;

    StringAllocator strings;

    Op    last_op;
    u64   error_count;
    char *file_name;
} Interp;

struct StackFrame {
    Constants    constant_pool;
    Ast          ast;
    Stack        stack;

    struct StackFrame *parent;
};

AstNode *find_decl_in_frame(StackFrame *in, char *name);
AstNode *find_decl(AstNode *block, StackFrame *root_scope, char *name);

Interp compile(Ast ast, char *file_name);
void run_interpreter(Interp *interp);
void free_interpreter(Interp *interp);

void   stack_push(Stack *, Object);
Object stack_pop(Stack *);
Object stack_top(Stack);

void frame_push(Interp *s, StackFrame *frame);
StackFrame *frame_pop(Interp *s);
StackFrame *frame_top(Interp *s);

void init_blocks(BlockStack *);
void push_block(BlockStack *, AstNode *);
AstNode *pop_block(BlockStack *s);
AstNode *current_block(BlockStack s);

/*
struct Module {
    Ast       ast;
    TokenList tokens;
};
*/

#endif
