#include "parser.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

static AstNode *parse_statement(Parser *p);

static AstNode *parse_expression(Parser *p);
static AstNode *parse_expression_list(Parser *p);
static AstNode *parse_assignment(Parser *p);
static AstNode *parse_logical_or(Parser *p);
static AstNode *parse_logical_and(Parser *p);
static AstNode *parse_equality_comparison(Parser *p);
static AstNode *parse_lt_gt_comparison(Parser *p);
static AstNode *parse_addition_subtraction(Parser *p);
static AstNode *parse_multiplication(Parser *p);
static AstNode *parse_division_modulo(Parser *p);
static AstNode *parse_postfix(Parser *p);
static AstNode *parse_call(Parser *p, AstNode *left);
static AstNode *parse_selector(Parser *p, AstNode *left);
static AstNode *parse_subscript(Parser *p, AstNode *left);
static AstNode *parse_simple_expression(Parser *p);

bool node_allocator_init(NodeAllocator *allocator) {
    NodeBlock *memory = (NodeBlock *)malloc(sizeof(NodeBlock));
    if (!memory) {
        return false;
    }
    *memory = (NodeBlock){0};
    allocator->first   = memory;
    allocator->current = memory;
    allocator->num_blocks = 1;
    return true;
}

AstNode *node_allocator(NodeAllocator *allocator) {
    if (allocator->current->used+1 > NODE_BLOCK_LENGTH) {
        NodeBlock *next = (NodeBlock *)malloc(sizeof(NodeBlock));
        if (!next) {
            printf("bad news, out of memory");
            return NULL;
        }
        allocator->current->next = next;
        allocator->current = next;
        allocator->num_blocks++;
    }
    AstNode *out = allocator->current->data+allocator->current->used;
    allocator->current->used += sizeof(AstNode);
    return out;
}

void node_allocator_free(NodeAllocator *allocator) {
    NodeBlock *buffer = allocator->first;
    while (buffer) {
        NodeBlock *current = buffer;
        buffer = current->next;
        free(current);
    }
}

void parser_init(Parser *p, const TokenList tokens, char *file_name) {
    p->token = tokens.data;
    p->file_name = file_name;
    p->error_count = 0;
    node_allocator_init(&p->node_allocator);
}

static inline void next(Parser *p) {
    p->before = p->token++;
}

static bool match_many(Parser *p, int n, ...) {
    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++) {
        TokenType type = va_arg(args, TokenType);
        if (p->token->type == type) {
            next(p);
            return true;
        }
    }
    return false;
}

static bool match(Parser *p, TokenType t) {
    if (p->token->type == t) {
        next(p);
        return true;
    }
    return false;
}

static Token peek(Parser *p) {
    if (p->token->type == Token_EOF) return *p->token;
    return p->token[1];
}

static void parser_error(Parser *p, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // The weird looking escape characters are to: set the text color to red, print "Error", and then reset the colour.
    fprintf(stderr, "%s:%lu: \033[0;31mSyntax error\033[0m: ", p->file_name, p->token->line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, ".\n");
    va_end(args);

    p->error_count++;
}

static AstNode *make_node(Parser *p, NodeTag tag) {
    AstNode *node = node_allocator(&p->node_allocator);
    assert(node);
    node->tag = tag;
    node->line = p->token->line;
    return node;
}

static AstNode *parse_expression(Parser *p) {
    return parse_expression_list(p);
}

static AstNode *parse_expression_list(Parser *p) {
    AstNode *or = parse_assignment(p);
    
    if (match(p, Token_COMMA)) {
        Ast expressions;
        array_init(expressions, AstNode);
        array_add(expressions, or);

        do {
            AstNode *expr = parse_assignment(p);
            if (!expr) return NULL;
            array_add(expressions, expr);
        } while(match(p, Token_COMMA));
        
        AstNode *new = make_node(p, NODE_EXPRESSION_LIST);
        new->expression_list.expressions = expressions;
        or = new;
    }
    return or;
}

static AstNode *parse_assignment(Parser *p) {
    AstNode *or = parse_logical_or(p);
    while (match(p, Token_EQUAL)) {
        TokenType op = p->token->type;
        AstNode *right = parse_logical_or(p);
        if (!right) {
            return NULL;
        }
        AstNode *new = make_node(p, NODE_BINARY);
        new->binary.left  = or;
        new->binary.right = right;
        new->binary.op    = op;
        or = new;
    }
    return or;
}

static AstNode *parse_logical_or(Parser *p) {
    AstNode *and = parse_logical_and(p);
    while (match(p, Token_ARROW)) {
        TokenType op = p->token->type;
        AstNode *right = parse_logical_and(p);
        if (!right) {
            return NULL;
        }
        AstNode *new = make_node(p, NODE_BINARY);
        new->binary.left  = and;
        new->binary.right = right;
        new->binary.op    = op;
        and = new;
    }
    return and;
}

static AstNode *parse_logical_and(Parser *p) {
    AstNode *compare = parse_equality_comparison(p);
    while (match(p, Token_AMP_AMP)) {
        TokenType op = p->before->type;
        AstNode *right = parse_equality_comparison(p);
        if (!right) {
            return NULL;
        }
        AstNode *new = make_node(p, NODE_BINARY);
        new->binary.left  = compare;
        new->binary.right = right;
        new->binary.op    = op;
        compare = new;
    }
    return compare;
}

static AstNode *parse_equality_comparison(Parser *p) {
    AstNode *lt_gt = parse_lt_gt_comparison(p);
    while (match_many(p, 2, Token_EQUAL_EQUAL, Token_BANG_EQUAL)) {
        TokenType op = p->before->type;
        AstNode *right = parse_lt_gt_comparison(p);
        if (!right) {
            return NULL;
        }
        AstNode *new = make_node(p, NODE_BINARY);
        new->binary.left  = lt_gt;
        new->binary.right = right;
        new->binary.op    = op;
        lt_gt = new;
    }
    return lt_gt;
}

static AstNode *parse_lt_gt_comparison(Parser *p) {
    AstNode *add_sub = parse_addition_subtraction(p);
    while (match_many(p, 4, Token_LESS, Token_LESS_EQUAL, Token_GREATER, Token_GREATER_EQUAL)) {
        TokenType op = p->before->type;
        AstNode *right = parse_addition_subtraction(p);
        if (!right) {
            return NULL;
        }
        AstNode *new = make_node(p, NODE_BINARY);
        new->binary.left  = add_sub;
        new->binary.right = right;
        new->binary.op    = op;
        add_sub = new;
    }
    return add_sub;
}

static AstNode *parse_addition_subtraction(Parser *p) {
    AstNode *mul = parse_multiplication(p);
    while (match_many(p, 2, Token_PLUS, Token_MINUS)) {
        TokenType op = p->before->type;
        AstNode *right = parse_multiplication(p);
        if (!right) {
            return NULL;
        }
        AstNode *new = make_node(p, NODE_BINARY);
        new->binary.left  = mul;
        new->binary.right = right;
        new->binary.op    = op;
        mul = new;
    }
    return mul;
}

static AstNode *parse_multiplication(Parser *p) {
    AstNode *div_mod = parse_division_modulo(p);
    while (match(p, Token_STAR)) {
        TokenType op = p->before->type;
        AstNode *right = parse_division_modulo(p);
        if (!right) {
            return NULL;
        }
        AstNode *new = make_node(p, NODE_BINARY);
        new->binary.left  = div_mod;
        new->binary.right = right;
        new->binary.op    = op;
        div_mod = new;
    }
    return div_mod;
}

static AstNode *parse_division_modulo(Parser *p) {
    AstNode *selector = parse_postfix(p);
    while (match_many(p, 2, Token_SLASH, Token_PERCENT)) {
        TokenType op = p->before->type;
        AstNode *right = parse_postfix(p);
        if (!right) {
            return NULL;
        }
        AstNode *new = make_node(p, NODE_BINARY);
        new->binary.left  = selector;
        new->binary.right = right;
        new->binary.op    = op;
        selector = new;
    }
    return selector;
}

static AstNode *parse_postfix(Parser *p) {
    AstNode *expr = parse_simple_expression(p);
    while (true) {
        if (match(p, Token_OPEN_PAREN)) {
            expr = parse_call(p, expr);
        } else if (match(p, Token_DOT)) {
            expr = parse_selector(p, expr);
        } else if (match(p, Token_OPEN_BRACKET)) {
            expr = parse_subscript(p, expr);
        } else {
            break;
        }
    }
    return expr;
}

static AstNode *parse_call(Parser *p, AstNode *left) {
    AstNode *call = make_node(p, NODE_CALL);
    call->call.name = left;
    call->call.args = NULL;

    if (!match(p, Token_CLOSE_PAREN)) {
        AstNode *inner = parse_expression(p);
        if (!inner) return NULL;
        call->call.args = inner;
        
        if (!match(p, Token_CLOSE_PAREN)) {
            parser_error(p, "expected closing )");
        }
    }
    
    return call;
}

static AstNode *parse_selector(Parser *p, AstNode *left) {
    AstNode *selector = make_node(p, NODE_BINARY);
    selector->binary.op = Token_DOT;

    AstNode *right = parse_expression(p);
    selector->binary.right = right;
    selector->binary.left = left;

    return selector;
}

static AstNode *parse_subscript(Parser *p, AstNode *left) {
    AstNode *subscript = make_node(p, NODE_SUBSCRIPT);
    subscript->subscript.array = left;
    subscript->subscript.inner_expr = NULL;

    if (!match(p, Token_CLOSE_BRACKET)) {
        AstNode *inner = parse_expression(p);
        if (!inner) return NULL;
        subscript->subscript.inner_expr = inner;
    }

    if (!match(p, Token_CLOSE_BRACKET)) {
        parser_error(p, "expected closing ]");
    }

    return subscript;
}

static AstNode *parse_simple_expression(Parser *p) {
    switch (p->token->type) {
    case Token_OPEN_PAREN: {
        next(p);
        AstNode *node = make_node(p, NODE_ENCLOSED_EXPRESSION);
        AstNode *inner = parse_expression(p);
        if (!inner) {
            return NULL;
        }
        if (p->token->type != Token_CLOSE_PAREN) {
            parser_error(p, "expected closing parenthese");
            return NULL;
        }
        next(p);
        node->enclosed_expr.inner = inner;
        return node;
    } break;

    case Token_OPEN_BRACKET: {
        next(p);
        return NULL;
    } break;

    case Token_MINUS: {
        AstNode *node = make_node(p, NODE_UNARY);
        node->unary.op = p->token->type;
        next(p);
        AstNode *operand = parse_assignment(p);
        if (!operand) return NULL;
        node->unary.operand = operand;
        return node;
    } break;

    case Token_INT_LIT: {
        AstNode *node = make_node(p, NODE_INT_LITERAL);
        node->literal.integer = atoi(p->token->text);
        next(p);
        return node;
    } break;

    case Token_FLOAT_LIT: {
        AstNode *node = make_node(p, NODE_FLOAT_LITERAL);
        node->literal.floating = atof(p->token->text);
        next(p);
        return node;
    } break;
    
    case Token_STRING_LIT: {
        AstNode *node = make_node(p, NODE_STRING_LITERAL);
        node->literal.string = p->token->text; // lives in string arena
        next(p);
        return node;
    } break;
    
    case Token_IDENT: {
        AstNode *node = make_node(p, NODE_IDENTIFIER);
        node->identifier = p->token->text;
        next(p);
        return node;
    } break;

    case Token_TRUE: {
        AstNode *node = make_node(p, NODE_BOOLEAN_LITERAL);
        node->boolean.value = 1;
        next(p);
        return node;
    } break;

    case Token_FALSE: {
        AstNode *node = make_node(p, NODE_BOOLEAN_LITERAL);
        node->boolean.value = 0;
        next(p);
        return node;
    } break;

    case Token_NULL: {
        AstNode *node = make_node(p, NODE_NULL_LITERAL);
        node->literal.string = NULL;
        next(p);
        return node;
    } break;

    default: {
        parser_error(p, "unexpected token '%.*s'", p->token->length, p->token->text);
        return NULL;
    } break;
    }
}

static AstNode *parse_block(Parser *p) {
    Ast block;
    array_init(block, AstNode*);

    while ((!match(p, Token_CLOSE_BRACE)) && p->token->type != Token_EOF) {
        AstNode *stmt = parse_statement(p);
        array_add(block, stmt);
    }

    AstNode *node = make_node(p, NODE_BLOCK);
    node->block.statements = block;
    node->block.parent = NULL;
    return node;
}

static AstNode *parse_let(Parser *p) {
    if (p->token->type != Token_IDENT) {
        parser_error(p, "expected name on variable declaration");
        return NULL;
    }

    AstNode *node = make_node(p, NODE_LET);
    node->let.name = p->token->text;
    node->let.expr = NULL;

    next(p); // skip identifier

    if (p->token->type == Token_EQUAL) {
        next(p);

        AstNode *expr = parse_expression(p);
        if (!expr) return NULL; // already errored

        node->let.expr = expr;
        return node;
    }

    // Automatic semi-colon insertion.
    // Probably still a bug though lol.
    if (p->token->type == Token_SEMI_COLON) {
        return node;
    }

    parser_error(p, "expected name on variable declaration");
    return NULL;
}

static bool ensure_arguments_are_correct(AstNode *args) {
    if (args->tag == NODE_EXPRESSION_LIST) {
        for (int i = 0; i < args->expression_list.expressions.length; i++) {
            AstNode *expr = args->expression_list.expressions.data[i];
            if (expr->tag != NODE_IDENTIFIER) return false;
        }
    } else if (args->tag != NODE_IDENTIFIER) {
        return false;
    }
    return true;
}

static AstNode *parse_function(Parser *p) {
    if (!match(p, Token_IDENT)) {
        parser_error(p, "expected identifier");
        return NULL;
    }
    char *name = p->before->text;

    if (!match(p, Token_OPEN_PAREN)) {
        parser_error(p, "expected argument list");
        return NULL;
    }
    AstNode *args = NULL;

    if (!match(p, Token_CLOSE_PAREN)) {
        AstNode *maybe_list = parse_expression_list(p);
        if (!maybe_list) {
            parser_error(p, "expected argument list");
            assert(false);
            return NULL;
        }
        ensure_arguments_are_correct(maybe_list);
        if (!match(p, Token_CLOSE_PAREN)) {
            parser_error(p, "expected )");
            return NULL;
        }
    }

    if (!match(p, Token_OPEN_BRACE)) {
        parser_error(p, "expected block");
        return NULL;
    }

    AstNode *block = parse_block(p);
    if (!block) {
        return NULL;
    }

    AstNode *func = make_node(p, NODE_FUNCTION);
    func->function.name = name;
    func->function.args = args;
    func->function.block = block;
    return func;
}

static AstNode *parse_statement(Parser *p) {
    AstNode *out = NULL;
    if (p->token->type == Token_EOF) {
        return NULL;
    } else if (match(p, Token_OPEN_BRACE)) {
        out = parse_block(p);
    } else if (match(p, Token_LET)) {
        out = parse_let(p);
    } else if (match(p, Token_FUNC)) {
        out = parse_function(p);
    } else if (match(p, Token_SEMI_COLON)) {
        // Already advanced
    } else {
        out = parse_expression(p);
    }
    return NULL;
}

Ast run_parser(Parser *p) {
    Ast ast;
    array_init(ast, AstNode *);

    while (true) {
        if (p->token->type == Token_EOF) {
            break;
        }

        array_add(ast, parse_statement(p));

        next(p);
    }

    return ast;
}
