#include "context.h"
#include "common.h"
#include "ast.h"
#include "array.h"

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

void compile_block(Interp *interp, AstNode *block);
void compile_call(Interp *interp, AstNode *call);
u64 add_scope_object(Interp *interp, Scope *scope);
void instr(Interp *interp, Op op, s32 arg, u64 line_number);

void add_primitive_objects(Scope *scope) {
    Object undefined = (Object){.tag=OBJECT_UNDEFINED, .pointer=NULL};
    array_add(scope->constant_pool, undefined);
    assert(scope->constant_pool.length-1 == UNDEFINED_OBJECT_INDEX);

    Object null_object = (Object){.tag=OBJECT_NULL, .pointer = NULL};
    array_add(scope->constant_pool, null_object);
    assert(scope->constant_pool.length-1 == NULL_OBJECT_INDEX);

    Object true_object = (Object){.tag=OBJECT_BOOLEAN, .boolean = 1};
    array_add(scope->constant_pool, true_object);
    assert(scope->constant_pool.length-1 == TRUE_OBJECT_INDEX);

    Object false_object = (Object){.tag=OBJECT_BOOLEAN, .boolean = 0};
    array_add(scope->constant_pool, false_object);
    assert(scope->constant_pool.length-1 == FALSE_OBJECT_INDEX);
}

u64 push_scope(Interp *interp, Ast ast) {
    Scope *new_scope = malloc(sizeof(Scope));

    new_scope->ast = ast;

    array_init(new_scope->constant_pool, Object);
    add_primitive_objects(new_scope);

    stack_init(&new_scope->stack);

    new_scope->parent = interp->scope;
    u64 index = add_scope_object(interp, new_scope);
    interp->scope = new_scope;
    return index;
}

void pop_scope(Interp *interp) {
    // TODO: memory leak! create a linear scope allocator which can free them all at the end
    interp->scope = interp->scope->parent;
}

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
    printf("Line %ld : %s %d\n", line_number, instruction_strings[op], arg);
#endif
}

u64 add_constant_int(Interp *interp, s64 i) {
    Object o = (Object){
        .integer = i,
        .tag = OBJECT_INTEGER,
    };
    array_add(interp->scope->constant_pool, o);
    return interp->scope->constant_pool.length-1;
}

u64 add_constant_string(Interp *interp, char *s) {
    Object o = (Object){
        .pointer = s,
        .tag = OBJECT_STRING,
    };
    array_add(interp->scope->constant_pool, o);
    return interp->scope->constant_pool.length-1;
}

u64 add_constant_float(Interp *interp, f64 f) {
    Object o = (Object){
        .floating = f,
        .tag = OBJECT_FLOATING,
    };
    array_add(interp->scope->constant_pool, o);
    return interp->scope->constant_pool.length-1;
}

u64 add_scope_object(Interp *interp, Scope *scope) {
    Object o = (Object){
        .scope = scope,
        .tag = OBJECT_SCOPE,
    };
    array_add(interp->scope->constant_pool, o);
    return interp->scope->constant_pool.length-1;
}

u64 reserve_constant(Interp *interp) {
    Object o = (Object){0};
    array_add(interp->scope->constant_pool, o);
    return interp->scope->constant_pool.length-1;
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
        AstNode *maybe_decl = find_decl(interp->scope->ast, expr->identifier);
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

    case NODE_LAMBDA: {
        u64 index = reserve_constant(interp);
        compile_block(interp, expr->lambda.block);
        return index;
    } break;

    case NODE_CALL: {
        u64 index = reserve_constant(interp);
        compile_call(interp, expr);
        instr(interp, STORERET, index, expr->line);
        return index;
    } break;

    default: {
        assert(false);
    } break;
    }
}

void compile_let(Interp *interp, AstNode *node) {
    // For now at least, a variable is just a named reference to a slot in the constants table.
    u64 variable_index = reserve_constant(interp);
    node->let.constant_pool_index = variable_index; // for name lookup

    AstLet let = node->let;

    // Store null by default, then compile the expression if there is one.
    // Basically, this code implements null-initialization-by-default.
    u64 value_index = NULL_OBJECT_INDEX;
    if (let.expr) {
        value_index = compile_expr(interp, let.expr);
    }

    instr(interp, LOAD, value_index, node->line);
    instr(interp, STORE, variable_index, node->line);
}

void compile_assignment(Interp *interp, AstNode *node) {
    AstBinary ass = node->binary;

    u64 target_index = compile_expr(interp, ass.left);
    u64 value_index  = compile_expr(interp, ass.right);

    instr(interp, LOAD, value_index, node->line);
    instr(interp, STORE, target_index, node->line);
}

// Compile each expression in a list, and emit loads for each one.
// Returns the number of expressions in the list.
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
        // Multiple, comma-separated arguments.
        if (call->call.args->tag == NODE_EXPRESSION_LIST) {
            num_args = compile_loads_for_expression_list(interp, call->call.args);
        } else { // Single argument
            num_args = 1;
            u64 value_index = compile_expr(interp, call->call.args);
            instr(interp, LOAD, value_index, call->line);
        }
    }

    AstNode *name = call->call.name;
    if (name->tag == NODE_IDENTIFIER) {
        char *name_ident = name->identifier;

        // Temporary hard-coded lookup.
        if (strcmp(name_ident, "print") == 0) {
            instr(interp, PRINT, num_args, name->line);
            return;
        }

        for (int i = 0; i < interp->scope->ast.length; i++) {
            AstNode *n = interp->scope->ast.data[i];
            if (n->tag != NODE_LAMBDA) continue;
            AstLambda f = n->lambda;

            if (strcmp(name_ident, f.name) == 0) {
                instr(interp, LOAD, f.constant_pool_index, call->line);
                instr(interp, JUMP, 0, call->line);
                return;
            }
        }

        compile_error(interp, call, "undeclared identifier '%s' (attempted invocation)", name_ident);
    }
}

void compile_named_lambda(Interp *interp, AstNode *node) {
    u64 lambda_index = reserve_constant(interp);
    node->lambda.constant_pool_index = lambda_index;

    AstLambda f = node->lambda;

    instr(interp, BEGINFUNC, lambda_index, node->line);

    u64 scope_index = push_scope(interp, f.block->block.statements);
    instr(interp, ENTERSCOPE, scope_index, node->line);

    // if (f.args) compile_loads_for_expression_list(interp, f.args);

    compile_block(interp, f.block);
    pop_scope(interp);
    
    instr(interp, RET, 0, 0); // TODO: add line number

    // TODO: add line number
    instr(interp, ENDFUNC, 0, 0);
}

void compile_return(Interp *interp, AstNode *node) {
    AstReturn r = node->ret;
    if (r.value) {
        u64 value_index = compile_expr(interp, node->ret.value);
        instr(interp, RET, value_index, node->line);
        return;
    }

    instr(interp, RET, 0, node->line);
}

void compile_statement(Interp *interp, AstNode *stmt) {
    switch (stmt->tag) {
    case NODE_LET: {
        compile_let(interp, stmt);
    } break;

    case NODE_LAMBDA: {
        compile_named_lambda(interp, stmt);
    } break;

    case NODE_CALL: {
        compile_call(interp, stmt);
    } break;

    case NODE_BINARY: {
        if (stmt->binary.op == Token_EQUAL) {
            compile_assignment(interp, stmt);
        }
    } break;

    case NODE_RETURN: {
        compile_return(interp, stmt);
    } break;

    default: {
        assert(false);
    } break;
    }
}

void compile_block(Interp *interp, AstNode *block) {
    if (!block->block.statements.data) return;
    for (u64 i = 0; i < block->block.statements.length; i++) {
        AstNode *stmt = block->block.statements.data[i];
        compile_statement(interp, stmt);
    }
}

Interp compile(Ast ast, char *file_name) {
    Interp interp = {0};

    interp.pc = 0;
    interp.last_jump_loc = 0;
    interp.file_name = file_name;
    string_allocator_init(&interp.strings);
    array_init(interp.instructions, Instruction);
    stack_init(&interp.return_stack);

    Scope *root_scope = malloc(sizeof(Scope));

    root_scope->ast = ast;
    root_scope->parent = NULL;
    array_init(root_scope->constant_pool, Object);
    add_primitive_objects(root_scope);
    stack_init(&root_scope->stack);

    interp.scope = root_scope;
    interp.root_scope = root_scope;

    for (u64 i = 0; i < ast.length; i++) {
        AstNode *node = ast.data[i];
        if (!node) break;
        compile_statement(&interp, node);
    }

    instr(&interp, HALT, 0, 0);

    return interp;
}
