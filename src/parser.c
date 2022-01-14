#include "parser.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

void parser_init(Parser *p, const TokenList tokens, char *file_name) {
    p->curr = tokens.tokens;
    p->prev = tokens.tokens;
    p->file_name = file_name;
    p->error_count = 0;
}

static void next(Parser *p) {
    p->prev = p->curr++;
}

static Token peek(Parser *p) {
    assert(p->curr->type != Token_EOF);
    return p->curr[1];
}

static void parser_error(Parser *p, const char *message) {
    printf("Syntax error: %s.\n", message);
    p->error_count++;
}

static AstNode *parse_simple_expression(Parser *p) {
    switch (p->curr->type) {
    case Token_OPEN_PAREN: {
        next(p);
        AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
        node->tag = NODE_ENCLOSED_EXPRESSION;
        AstNode *inner = parse_simple_expression(p);
        if (!inner) {
            return NULL;
        }
        if (p->curr->type != Token_CLOSE_PAREN) {
            parser_error(p, "expected closing parenthese");
            return NULL;
        }
        next(p);
        node->enclosed_expr.inner = inner;
        return node;
    } break;
    case Token_INT_LIT: {
        AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
        node->tag = NODE_INT_LITERAL;
        node->literal.integer = atoi(p->curr->text);
        next(p);
        return node;
    } break;
    case Token_FLOAT_LIT: {
        AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
        node->tag = NODE_FLOAT_LITERAL;
        node->literal.floating = atof(p->curr->text);
        next(p);

        // if (p->curr->type == Token_PLUS) {
        //     next(p);
        //     AstNode *right = parse_simple_expression(p);
        //     if (!right) return NULL;

        //     AstNode *binary = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
        //     binary->tag = NODE_BINARY;
        //     binary->binary.right = right;
        //     binary->binary.left = node;
        //     binary->binary.op = Token_PLUS;
            
        //     return binary;
        // }
        
        return node;
    } break;
    case Token_STRING_LIT: {
        AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
        node->tag = NODE_STRING_LITERAL;
        node->literal.string = p->curr->text; // lives in string arena
        next(p);
        return node;
    } break;
    case Token_NULL: {
        AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
        node->tag = NODE_NULL_LITERAL;
        node->literal.string = NULL;
        next(p);
        return node;
    } break;
    case Token_IDENT: {
        AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
        node->tag = NODE_IDENTIFIER;
        node->identifier = p->curr->text;
        next(p);
        return node;
    } break;
    default: {
        parser_error(p, "expected an expression");
        return NULL;
    } break;
    }
}

static AstNode *parse_let(Parser *p) {
    next(p);

    if (p->curr->type != Token_IDENT) {
        parser_error(p, "expected name on variable declaration");
        return NULL;
    }

    AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
    node->tag = NODE_LET;
    node->let.name = p->curr->text;
    node->let.expr = NULL;

    next(p); // skip identifier

    if (p->curr->type == Token_EQUAL) {
        next(p);

        AstNode *expr = parse_simple_expression(p);
        if (!expr) return NULL; // already errored

        node->let.expr = expr;
        return node;
    }

    // Automatic semi-colon insertion.
    // Probably still a bug though lol.
    if (p->curr->type == Token_SEMI_COLON) {
        return node;
    }

    parser_error(p, "expected name on variable declaration");
    return NULL;
}

AstNode *parse_print(Parser *p) {
    next(p);

    AstNode *expr = parse_simple_expression(p);
    if (!expr) return NULL;

    AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
    node->tag = NODE_PRINT;
    node->print.expr = expr;

    return node;
}

AstNode *parse_assignment(Parser *p) {
    AstNode *left = parse_simple_expression(p);
    if (left->tag != NODE_IDENTIFIER) {
        parser_error(p, "expected an identifier");
        return NULL;
    }

    if (p->curr->type != Token_EQUAL) {
        parser_error(p, "expected assignment operator (equals)");
        return NULL;
    }

    next(p); // =

    AstNode *right = parse_simple_expression(p);
    if (!right) {
        // Already errored...
        return NULL;
    }

    AstNode *node = (AstNode *)malloc(sizeof(AstNode)); // TODO LEAK
    node->tag = NODE_ASSIGNMENT;
    node->assignment.left  = left;
    node->assignment.right = right;

    return node;
}

Ast run_parser(Parser *p) {
    Ast ast;
    array_init(ast, AstNode *);

    while (true) {
        if (p->curr->type == Token_EOF) break;

        if (p->curr->type == Token_LET) {
            AstNode *node = parse_let(p);
            array_add(ast, node);
        }

        if (p->curr->type == Token_PRINT) {
            AstNode *node = parse_print(p);
            array_add(ast, node);
        }

        if (p->curr->type == Token_IDENT) {
            AstNode *node = parse_assignment(p);
            array_add(ast, node);
        }

        next(p);
    }

    return ast;
}