#ifndef AST_h
#define AST_h

#include "array.h"

struct AstNode;
typedef Array(struct AstNode *) Ast;

typedef enum NodeTag {
    NODE_LET,
    NODE_LAMBDA,
    NODE_BLOCK,
    NODE_RETURN,
    NODE_PRINT,
    NODE_CONTROL_FLOW_IF,
    NODE_CONTROL_FLOW_LOOP,
    NODE_BREAK_OR_CONTINUE,

    NODE_ENCLOSED_EXPRESSION,
    NODE_IDENTIFIER,
    NODE_INT_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_NULL_LITERAL,
    NODE_BOOLEAN_LITERAL,
    NODE_SUBSCRIPT,
    NODE_EXPRESSION_LIST,
    NODE_CALL,
    NODE_BINARY,
    NODE_UNARY,
} NodeTag;

typedef struct AstLet {
    char *name;
    struct AstNode *expr;
    u64 constant_pool_index;
} AstLet;

typedef struct AstIf {
    struct AstNode *condition;
    struct AstNode *block;
    struct AstNode *else_branch;
} AstIf;

typedef struct AstBreakCont {
    TokenType which;
    struct AstNode *name; // TODO named loops
} AstBreakCont;

typedef struct AstLoop {
    struct AstNode *condition;
    struct AstNode *block;
} AstLoop;

typedef struct AstLiteral {
    union {
        u64 integer;
        f64 floating;
        char *string;
    };
} AstLiteral;

typedef struct AstPrint {
    struct AstNode *expr;
} AstPrint;

typedef struct AstEnclosed {
    struct AstNode *inner;
} AstEnclosed;

typedef struct AstBoolean {
    u8 value;
} AstBoolean;

typedef struct AstBinary {
    struct AstNode *left;
    struct AstNode *right;
    TokenType op;
} AstBinary;

typedef struct AstSubscript {
    struct AstNode *array;
    struct AstNode *inner_expr;
} AstSubscript;

typedef struct AstExpressionList {
    Ast expressions;
} AstExpressionList;

typedef struct AstCall {
    struct AstNode *name;
    struct AstNode *args;
} AstCall;

typedef struct AstUnary {
    struct AstNode *operand;
    TokenType op;
} AstUnary;

typedef struct AstBlock {
    struct AstNode *parent;
    struct AstNode *final_statement;
    Ast statements;
} AstBlock;

typedef struct AstLambda {
    char *name;
    struct AstNode *args;
    struct AstNode *block;
    u64 constant_pool_index;
} AstLambda;

typedef struct AstReturn {
    struct AstNode *value;
} AstReturn;

typedef struct AstNode {
    NodeTag tag;
    u64 line;
    union {
        AstLet        let;
        AstLiteral    literal;
        AstPrint      print;
        AstBlock      block;
        AstEnclosed   enclosed_expr;
        AstBoolean    boolean;
        AstBinary     binary;
        AstSubscript  subscript;
        AstCall       call;
        AstExpressionList expression_list;
        AstUnary      unary;
        AstLambda     lambda;
        AstReturn     ret;
        AstIf         cf;
        AstLoop       loop;
        AstBreakCont  break_cont;
        
        char *        identifier;
    };
} AstNode;

#endif
