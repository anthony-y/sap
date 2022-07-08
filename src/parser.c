#include "parser.h"
#include "context.h"
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

static inline void next(Parser *p);
static bool match_many(Parser *p, int n, ...);
static bool match(Parser *p, TokenType t);
static Token peek(Parser *p);

static void parser_error(Parser *p, const char *fmt, ...);
static AstNode *make_node(Parser *p, NodeTag tag);

static AstNode *parse_statement(Parser *p);
static AstNode *parse_lambda(Parser *p);
static AstNode *parse_let(Parser *p);
static AstNode *parse_return(Parser *p);
static AstNode *parse_if(Parser *p);
static AstNode *parse_break_continue(Parser *p);
static AstNode *parse_loop(Parser *p);
static AstNode *parse_block(Parser *p);
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

static BlockStack block_stack;

void parser_init(Parser *p, const TokenList tokens, char *file_name) {
    p->token = tokens.data;
    p->file_name = file_name;
    p->error_count = 0;
    node_allocator_init(&p->node_allocator);
}

Ast run_parser(Parser *p) {
    init_blocks(&block_stack);

    Ast ast;
    array_init(ast, AstNode *);

    while (true) {
        if (p->token->type == Token_EOF) {
            break;
        }
        array_add(ast, parse_statement(p));
    }

    return ast;
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
        out = parse_lambda(p);
        match(p, Token_SEMI_COLON);
        return out;

    } else if (match(p, Token_RETURN)) {
        out = parse_return(p);

    } else if (match(p, Token_IF)) {
        out = parse_if(p);
        
    } else if (match(p, Token_WHILE)) {
        out = parse_loop(p);
    
    } else if (p->token->type == Token_BREAK || p->token->type == Token_CONTINUE) {
        out = parse_break_continue(p);

    } else {
        out = parse_expression(p);
    }

    if (!match_many(p, 2, Token_SEMI_COLON, Token_EOF)) {
        parser_error(p, "expected semi-colon");
        return NULL;
    }

    return out;
}

static AstNode *parse_block(Parser *p) {
    AstNode *node = make_node(p, NODE_BLOCK);

    Ast block;
    array_init(block, AstNode*);

    node->block.parent = current_block(block_stack);
    push_block(&block_stack, node);

    AstNode *stmt = NULL;
    while ((!match(p, Token_CLOSE_BRACE))) {
        if (p->token->type == Token_EOF) {
            parser_error(p, "unexpected end of file");
            return NULL;
        }
        AstNode *temp = parse_statement(p);
        if (!temp) break;
        array_add(block, temp);
        stmt = temp;
    }
    match(p, Token_CLOSE_BRACE);

    pop_block(&block_stack);

    node->block.statements = block;
    node->block.final_statement = stmt;
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

static AstNode *parse_lambda(Parser *p) {
    AstNode *func = make_node(p, NODE_LAMBDA);

    if (p->token->type != Token_IDENT) {
        parser_error(p, "expected name of function");
        return NULL;
    }

    char *name = p->token->text;
    next(p);

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
        if (!match(p, Token_CLOSE_PAREN)) {
            parser_error(p, "expected )");
            return NULL;
        }
        
        if (ensure_arguments_are_correct(maybe_list)) {
            args = maybe_list;
        } else {
            // TODO: better error
            parser_error(p, "arguments must be declared as identififers");
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

    if (args) {
        if (args->tag == NODE_EXPRESSION_LIST) {
            for (int i = 0; i < args->expression_list.expressions.length; i++) {
                AstNode *arg = args->expression_list.expressions.data[i];
                arg->tag = NODE_LET;
                arg->let.name = arg->identifier;
                arg->let.expr = NULL;
                arg->let.constant_pool_index = 0;
                array_add(block->block.statements, arg);
            }
        }

        else {
            args->tag = NODE_LET;
            args->let.name = args->identifier;
            args->let.expr = NULL;
            args->let.constant_pool_index = 0;
            array_add(block->block.statements, args);
        }
    }

    func->lambda.name = name;
    func->lambda.args = args;
    func->lambda.block = block;
    func->lambda.constant_pool_index = 0;
    return func;
}

static AstNode *parse_return(Parser *p) {
    AstNode *ret = make_node(p, NODE_RETURN);
    ret->ret.value = NULL;

    if (p->token->type == Token_SEMI_COLON) {
        return ret;
    }

    ret->ret.value = parse_expression(p);
    return ret;
}

static AstNode *parse_if(Parser *p) {
    AstNode *cf = make_node(p, NODE_CONTROL_FLOW_IF);

    AstNode *condition = parse_expression(p);
    if (!condition) {
        parser_error(p, "expected 'if' to have a condition");
        return NULL;
    }

    if (!match(p, Token_OPEN_BRACE)) {
        parser_error(p, "expected 'if' to have a block");
        return NULL;
    }

    AstNode *block = parse_block(p);
    if (!block) {
        // Already errored
        return NULL;
    }

    cf->cf.condition = condition;
    cf->cf.block = block;
    return cf;
}

static AstNode *parse_loop(Parser *p) {
    AstNode *cf = make_node(p, NODE_CONTROL_FLOW_LOOP);

    AstNode *condition = parse_expression(p);
    if (!condition) {
        parser_error(p, "expected 'if' to have a condition");
        return NULL;
    }

    if (!match(p, Token_OPEN_BRACE)) {
        parser_error(p, "expected 'if' to have a block");
        return NULL;
    }

    AstNode *block = parse_block(p);
    if (!block) {
        // Already errored
        return NULL;
    }

    cf->cf.condition = condition;
    cf->cf.block = block;
    return cf;
}

static AstNode *parse_break_continue(Parser *p) {
    AstNode *node = make_node(p, NODE_BREAK_OR_CONTINUE);
    node->break_cont.which = p->token->type;
    node->break_cont.name = NULL; // TODO
    next(p); // keyword
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
    while (match_many(p, 5, Token_EQUAL, Token_PLUS_EQUAL, Token_MINUS_EQUAL, Token_SLASH_EQUAL, Token_STAR_EQUAL)) {
        TokenType op = p->before->type;
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
        TokenType op = p->before->type;
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

    // TODO: test array stuff again, this might need to be moved to parse_statement
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
        while ((p->token++)->type != Token_EOF);
        return NULL;
    } break;
    }
}

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
    if (allocator->current->num_nodes+1 > NODE_BLOCK_LENGTH) {
        NodeBlock *next = (NodeBlock *)malloc(sizeof(NodeBlock));
        if (!next) {
            printf("bad news, out of memory");
            return NULL;
        }
        allocator->current->next = next;
        allocator->current = next;
        allocator->num_blocks++;
    }

    AstNode *out = (allocator->current->data + allocator->current->num_nodes);

    allocator->current->num_nodes++;
    allocator->total_nodes++;
    
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
    // node->id = p->node_allocator.total_nodes-1;
    return node;
}
