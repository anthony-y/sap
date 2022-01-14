#include "context.h"
#include "common.h"
#include "ast.h"
#include "array.h"

#include <stdio.h>
#include <assert.h>

#define UNDEFINED_OBJECT_INDEX 0
#define NULL_OBJECT_INDEX 1
#define PRINT_INSTRUCTIONS_DURING_COMPILE 0

#define DO_EMIT true
#define NO_EMIT false

static void compile_error(Interp *interp, const char *message) {
    printf("Compile error: %s.\n", message);
    interp->error_count++;
}

static void runtime_error(Interp *interp, const char *message) {
    printf("Runtime error: %s.\n", message);
    interp->error_count++;
}

void stack_init(Stack *stack) {
    stack->top = 0;
}

static void runtime_print(Object value) {
    switch (value.tag) {
    case OBJECT_INTEGER:   printf("%ld\n", value.integer); break;
    case OBJECT_FLOATING:  printf("%f\n", value.floating); break;
    case OBJECT_STRING:    printf("%s\n", value.pointer); break;
    case OBJECT_NULL:      printf("null\n"); break;
    case OBJECT_UNDEFINED: printf("undefined\n"); break;
    default: assert(false);
    }
}

void instr(Interp *interp, Op op, s32 arg) {
#if PRINT_INSTRUCTIONS_DURING_COMPILE
    printf("%s %d\n", instruction_strings[op], arg);
#endif

    Instruction i = (Instruction){
        .op = op,
        .arg = arg
    };
    array_add(interp->instructions, i);
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

u64 compile_expr(Interp *interp, AstNode *expr, bool emit) {
    u64 index = 0;
    switch (expr->tag) {
    case NODE_ENCLOSED_EXPRESSION: {
        return compile_expr(interp, expr->enclosed_expr.inner, emit);
    } break;
    case NODE_INT_LITERAL: {
        index = add_constant_int(interp, expr->literal.integer);
    } break;
    case NODE_STRING_LITERAL: {
        index = add_constant_string(interp, expr->literal.string);
    } break;
    case NODE_FLOAT_LITERAL: {
        index = add_constant_float(interp, expr->literal.floating);
    } break;
    case NODE_NULL_LITERAL: {
        return NULL_OBJECT_INDEX;
    } break;
    case NODE_IDENTIFIER: {
        AstNode *maybe_decl = find_decl(interp->ast, expr->identifier);
        if (!maybe_decl) {
            compile_error(interp, "undeclared identifier");
            return 0;
        }
        index = maybe_decl->let.constant_pool_index;
        if (emit) instr(interp, LOAD, index);
        return index;
    } break;
    case NODE_BINARY: {
        assert(false);
    } break;
    default: assert(false);
    }

    if (emit) {
        instr(interp, CONST, index);
    }

    return index;
}

void compile_expression_for_print(Interp *interp, AstNode *expr) {
    if (expr->tag == NODE_ENCLOSED_EXPRESSION) {
        compile_expression_for_print(interp, expr->enclosed_expr.inner);
    } else {
        compile_expr(interp, expr, DO_EMIT);
        if (expr->tag == NODE_IDENTIFIER) {
            instr(interp, PRINT_VAR, 0);   
        } else {
            instr(interp, PRINT_CONST, 0);
        }
    }
}

void compile_print(Interp *interp, AstPrint print) {
    compile_expression_for_print(interp, print.expr);
}

void compile_let(Interp *interp, AstLet *let) {
    u64 value_index = compile_expr(interp, let->expr, NO_EMIT);
    let->constant_pool_index = value_index;
}

void compile_assignment(Interp *interp, AstAssignment ass) {
    u64 target_index = compile_expr(interp, ass.left, NO_EMIT);
    instr(interp, CONST, target_index);
    compile_expr(interp, ass.right, DO_EMIT);
    instr(interp, STORE, 0);
}

Interp compile(Ast ast) {
    Interp interp = {0};

    interp.ast = ast;

    array_init(interp.constant_pool, Object);
    array_init(interp.instructions, Instruction);

    stack_init(&interp.stack);

    Object undefined = (Object){.tag=OBJECT_UNDEFINED, .pointer=NULL};
    array_add(interp.constant_pool, undefined);
    assert(interp.constant_pool.length-1 == UNDEFINED_OBJECT_INDEX);

    Object null_object = (Object){.pointer = NULL, .tag=OBJECT_NULL};
    array_add(interp.constant_pool, null_object);
    assert(interp.constant_pool.length-1 == NULL_OBJECT_INDEX);

    for (u64 i = 0; i < ast.length; i++) {
        AstNode *node = ast.data[i];
        if (!node) {
            break;
        }

        if (node->tag == NODE_LET)        compile_let(&interp, &node->let);
        if (node->tag == NODE_PRINT)      compile_print(&interp, node->print);
        if (node->tag == NODE_ASSIGNMENT) compile_assignment(&interp, node->assignment);
    }
    
    return interp;
}

void run_interpreter(Interp *interp) {
    for (u64 i = 0; i < interp->instructions.length; i++) {
        Instruction instr = interp->instructions.data[i];
        switch (instr.op) {
        case CONST: {
            Object o = (Object){
                .integer = instr.arg,
                .tag = OBJECT_INTEGER,
            };
            stack_push(&interp->stack, o);
        } break;
        case LOAD: {
            stack_push(&interp->stack, interp->constant_pool.data[instr.arg]);
        } break;
        case STORE: {
            u64 new_value_index = stack_pop(&interp->stack).integer;
            u64 target_index    = stack_pop(&interp->stack).integer;

            Object *target = &interp->constant_pool.data[target_index];
            Object new_value = interp->constant_pool.data[new_value_index];

            // TODO type checking

            *target = new_value;
        } break;
        case ADD: {
            u64 left_index  = stack_pop(&interp->stack).integer;
            u64 right_index = stack_pop(&interp->stack).integer;

            Object left = interp->constant_pool.data[left_index];
            Object right = interp->constant_pool.data[right_index];

            if (left.tag == OBJECT_FLOATING || right.tag == OBJECT_FLOATING) {
                // Object result = (Object){
                //     .tag=OBJECT_FLOATING,
                //     .floating=left.floating + right.floating,
                // };
                
                // TODO: things need to be reworked so that they're not working with constant indices all the time.
                //       in other words, we want to be able to pop actual values instead of only constant indices.
                // stack_push(&interp->stack, result);

                f64 result = left.floating + right.floating;
                u64 index = add_constant_float(interp, result);
                Object o = (Object){
                    .integer = index,
                    .tag = OBJECT_INTEGER,
                };
                stack_push(&interp->stack, o);
            } else {
                runtime_error(interp, "can't add anything except a float");
            }
        } break;
        case PRINT_CONST: {
            Object index = stack_pop(&interp->stack);
            Object real_value = interp->constant_pool.data[index.integer];
            runtime_print(real_value);
        } break;
        case PRINT_VAR: {
            runtime_print(stack_pop(&interp->stack));
        } break;
        }
    }
}