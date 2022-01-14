#ifndef AST_h
#define AST_h

#include "array.h"

struct AstNode;
typedef Array(struct AstNode *) Ast;

typedef enum NodeTag {
    NODE_LET,
    NODE_FUNC,
    NODE_BLOCK,
    NODE_ASSIGNMENT,

    NODE_ENCLOSED_EXPRESSION,
    NODE_IDENTIFIER,
    NODE_INT_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_NULL_LITERAL,
    NODE_BINARY,

    NODE_PRINT,
} NodeTag;

typedef struct AstLet {
    char *name;
    struct AstNode *expr;
    u64 constant_pool_index;
} AstLet;

typedef struct AstFunc {
    char *name;
    Ast block;
    Ast params;
} AstFunc;

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

typedef struct AstAssignment {
    struct AstNode *left;
    struct AstNode *right;
} AstAssignment;

typedef struct AstEnclosed {
    struct AstNode *inner;
} AstEnclosed;

typedef struct AstBinary {
    struct AstNode *left;
    struct AstNode *right;
    TokenType op;
} AstBinary;

typedef struct AstNode {
    NodeTag tag;
    union {
        AstLet        let;
        AstFunc       func;
        AstLiteral    literal;
        AstPrint      print;
        Ast           block;
        AstAssignment assignment;
        AstEnclosed   enclosed_expr;
        AstBinary     binary;
        
        char *        identifier;
    };
} AstNode;

#endif