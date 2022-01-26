#include "context.h"
#include "common.h"
#include "ast.h"
#include "array.h"

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

static void compile_error(Interp *interp, AstNode *node, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // The weird looking escape characters are to: set the text color to red, print "Error", and then reset the colour.
    fprintf(stderr, "%s:%lu: \033[0;31mCompile error\033[0m: ", interp->file_name, node->line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, ".\n");
    va_end(args);

    interp->error_count++;
}

void stack_init(Stack *stack) {
    stack->top = 0;
}

void instr(Interp *interp, Op op, s32 arg, u64 line_number) {
    Instruction i = (Instruction){
        .op = op,
        .arg = arg,
        .line_number = line_number,
    };
    array_add(interp->instructions, i);

    interp->last_op = op;

#if PRINT_INSTRUCTIONS_DURING_COMPILE
    printf("%s %d\n", instruction_strings[op], arg);
#endif
}

u64 add_constant_int(Interp *interp, s64 i) {
    Object o = (Object){
        .integer = i,
        .tag = OBJECT_INTEGER,
    };
    array_add(interp->constant_pool, o);
    return interp->constant_pool.length-1;
}

u64 add_constant_string(Interp *interp, char *s) {
    Object o = (Object){
        .pointer = s,
        .tag = OBJECT_STRING,
    };
    array_add(interp->constant_pool, o);
    return interp->constant_pool.length-1;
}

u64 add_constant_float(Interp *interp, f64 f) {
    Object o = (Object){
        .floating = f,
        .tag = OBJECT_FLOATING,
    };
    array_add(interp->constant_pool, o);
    return interp->constant_pool.length-1;
}

u64 reserve_constant(Interp *interp) {
    Object o = (Object){0};
    array_add(interp->constant_pool, o);
    return interp->constant_pool.length-1;
}

u64 compile_expr(Interp *interp, AstNode *expr) {
    switch (expr->tag) {
    case NODE_ENCLOSED_EXPRESSION: {
        return compile_expr(interp, expr->enclosed_expr.inner);
    } break;

    case NODE_INT_LITERAL: {
        return add_constant_int(interp, expr->literal.integer);
    } break;

    case NODE_STRING_LITERAL: {
        return add_constant_string(interp, expr->literal.string);
    } break;

    case NODE_FLOAT_LITERAL: {
        return add_constant_float(interp, expr->literal.floating);
    } break;

    case NODE_NULL_LITERAL: {
        return NULL_OBJECT_INDEX;
    } break;

    case NODE_BOOLEAN_LITERAL: {
        return (expr->boolean.value ? TRUE_OBJECT_INDEX : FALSE_OBJECT_INDEX);
    } break;

    case NODE_UNARY: {
        u64 result = reserve_constant(interp);
        switch (expr->unary.op) {
        case Token_MINUS: {
            u64 operand_index = compile_expr(interp, expr->unary.operand);
            instr(interp, LOAD, operand_index, expr->line);
            instr(interp, NEG, 0, expr->line);
            instr(interp, STORE, result, expr->line);
        } break;
        }
        return result;
    } break;

    case NODE_IDENTIFIER: {
        AstNode *maybe_decl = find_decl(interp->ast, expr->identifier);
        if (!maybe_decl) {
            compile_error(interp, expr, "undeclared identifier '%s'", expr->identifier);
            return 0;
        }
        return maybe_decl->let.constant_pool_index;
    } break;

    case NODE_BINARY: {
        u64 index = reserve_constant(interp);
        u64 leftidx = compile_expr(interp, expr->binary.left);
        u64 rightix = compile_expr(interp, expr->binary.right);

        instr(interp, LOAD, leftidx, expr->line);
        instr(interp, LOAD, rightix, expr->line);

        switch (expr->binary.op) {
        case Token_EQUAL_EQUAL: {
            instr(interp, EQUALS, 0, expr->line);
        } break;
        case Token_PLUS: {
            instr(interp, ADD, 0, expr->line);
        } break;
        case Token_MINUS: {
            instr(interp, SUB, 0, expr->line);
        } break;
        case Token_STAR: {
            instr(interp, MUL, 0, expr->line);
        } break;
        case Token_SLASH: {
            instr(interp, DIV, 0, expr->line);
        } break;
        }
        instr(interp, STORE, index, expr->line);
        return index;
    } break;

    default: {
        assert(false);
    } break;
    }
}

void compile_let(Interp *interp, AstLet *let) {
    u64 line_number = ((AstNode *)let)->line;

    u64 variable_index = reserve_constant(interp);
    let->constant_pool_index = variable_index;
    if (!let->expr) return; // TODO maybe STORE NULL_OBJECT_INDEX

    u64 value_index = compile_expr(interp, let->expr);
    instr(interp, LOAD, value_index, line_number);
    instr(interp, STORE, variable_index, line_number);
}

void compile_assignment(Interp *interp, AstBinary *ass) {
    u64 line_number = ((AstNode *)ass)->line;

    u64 target_index = compile_expr(interp, ass->left);
    u64 value_index  = compile_expr(interp, ass->right);
    instr(interp, LOAD, value_index, line_number);
    instr(interp, STORE, target_index, line_number);
}

u64 compile_loads_for_expression_list(Interp *interp, AstNode *list) {
    u64 i = list->expression_list.expressions.length;
    u64 j = i;
    for (; j > 0; j--) {
        AstNode *expr = list->expression_list.expressions.data[j-1];
        u64 value_index = compile_expr(interp, expr);
        instr(interp, LOAD, value_index, expr->line);
    }
    return i;
}

void compile_call(Interp *interp, AstNode *call) {
    s32 num_args = 0;

    if (call->call.args) {
        if (call->call.args->tag == NODE_EXPRESSION_LIST) {
            num_args = compile_loads_for_expression_list(interp, call->call.args);
        } else {
            num_args = 1;
            u64 value_index = compile_expr(interp, call->call.args);
            instr(interp, LOAD, value_index, call->line);
        }
    }

    AstNode *name = call->call.name;
    if (name->tag == NODE_IDENTIFIER) {
        char *name_ident = name->identifier;
        if (strcmp(name_ident, "print") == 0) {
            instr(interp, PRINT, num_args, name->line);
            return;
        }
    }
}

Interp compile(Ast ast, char *file_name) {
    Interp interp = {0};

    interp.ast = ast;
    interp.file_name = file_name;

    array_init(interp.constant_pool, Object);
    array_init(interp.instructions, Instruction);

    stack_init(&interp.stack);

    // Add transient constants (null, undefined, true, false)
    {
        Object undefined = (Object){.tag=OBJECT_UNDEFINED, .pointer=NULL};
        array_add(interp.constant_pool, undefined);
        assert(interp.constant_pool.length-1 == UNDEFINED_OBJECT_INDEX);

        Object null_object = (Object){.tag=OBJECT_NULL, .pointer = NULL};
        array_add(interp.constant_pool, null_object);
        assert(interp.constant_pool.length-1 == NULL_OBJECT_INDEX);

        Object true_object = (Object){.tag=OBJECT_BOOLEAN, .boolean = 1};
        array_add(interp.constant_pool, true_object);
        assert(interp.constant_pool.length-1 == TRUE_OBJECT_INDEX);

        Object false_object = (Object){.tag=OBJECT_BOOLEAN, .boolean = 0};
        array_add(interp.constant_pool, false_object);
        assert(interp.constant_pool.length-1 == FALSE_OBJECT_INDEX);
    }

    for (u64 i = 0; i < ast.length; i++) {
        AstNode *node = ast.data[i];
        if (!node) break;

        switch (node->tag) {
        case NODE_LET: {
            compile_let(&interp, &node->let);
        } break;

        case NODE_CALL: {
            compile_call(&interp, node);
        } break;

        case NODE_BINARY: {
            if (node->binary.op == Token_EQUAL) {
                compile_assignment(&interp, &node->binary);
            }
        } break;

        default: {
            assert(false);
        } break;
        }
    }
    
    return interp;
}
