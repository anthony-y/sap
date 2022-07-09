#include "context.h"
#include "common.h"
#include "ast.h"
#include "array.h"

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

static BlockStack block_stack;

void compile_statement(Interp *interp, AstNode *stmt);
void compile_if(Interp *interp, AstNode *cf);
void compile_block(Interp *interp, AstNode *block);
void compile_call(Interp *interp, AstNode *call);
void compile_break_or_continue(Interp *interp, AstNode *bc);
void instr(Interp *interp, Op op, s32 arg, u64 line_number);
u64 compile_loads_for_expression_list(Interp *interp, AstNode *list, bool args);
u64 add_scope_object(Interp *interp, StackFrame *scope);

void add_primitive_objects(StackFrame *scope) {
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

u64 push_frame(Interp *interp, Ast ast) {
    StackFrame *new_scope = malloc(sizeof(StackFrame));

    new_scope->ast = ast;
    new_scope->parent = interp->scope;

    array_init(new_scope->constant_pool, Object);
    add_primitive_objects(new_scope);
    new_scope->stack.top = 0;

    u64 index = add_scope_object(interp, new_scope);
    interp->scope = new_scope;

    return index;
}

void pop_frame(Interp *interp) {
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

u64 add_scope_object(Interp *interp, StackFrame *scope) {
    Object o = (Object){
        .scope = scope,
        .tag = OBJECT_SCOPE,
    };
    array_add(interp->scope->constant_pool, o);
    return interp->scope->constant_pool.length-1;
}

u64 add_array_object(Interp *interp) {
    Object o = (Object){0};
    o.tag = OBJECT_ARRAY;
    array_init(o.array, Object);
    array_add(interp->scope->constant_pool, o);
    return interp->scope->constant_pool.length-1;
}

u64 reserve_constant(Interp *interp) {
    Object o = (Object){0};
    array_add(interp->scope->constant_pool, o);
    return interp->scope->constant_pool.length-1;
}

u64 reserve_non_mutable(Interp *interp) {
    Object o = (Object){0};
    o.non_mutable = true;
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

    case NODE_ARRAY_LITERAL: {
        u64 array_index = add_array_object(interp);

        if (!expr->array_literal) {
            return array_index;
        }
        
        Object *array = &interp->scope->constant_pool.data[array_index];

        if (expr->array_literal->tag != NODE_EXPRESSION_LIST) {
            u64 index = compile_expr(interp, expr->array_literal);
            array_add(array->array, interp->scope->constant_pool.data[index]);
            return array_index;
        }

        int length = expr->array_literal->expression_list.expressions.length;

        for (int i = 0; i < length; i++) {
            AstNode *node = expr->array_literal->expression_list.expressions.data[i];
            u64 index = compile_expr(interp, node);
            array_add(array->array, interp->scope->constant_pool.data[index]);
        }
        
        return array_index;
    } break;

    case NODE_SUBSCRIPT: {
        u64 target = compile_expr(interp, expr->subscript.array);
        u64 index = compile_expr(interp, expr->subscript.inner_expr);
        assert(false);
    } break;

    case NODE_IDENTIFIER: {
        AstNode *maybe_decl = find_decl(current_block(block_stack), interp->root_scope, expr->identifier);
        if (!maybe_decl) {
            compile_error(interp, expr, "undeclared identifier '%s'", expr->identifier);
            return 0;
        }
        return maybe_decl->let.constant_pool_index;
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
        case Token_GREATER: {
            instr(interp, GREATER_THAN, 0, expr->line);
        } break;
        case Token_LESS: {
            instr(interp, LESS_THAN, 0, expr->line);
        } break;
        case Token_GREATER_EQUAL: {
            instr(interp, GREATER_THAN_EQUALS, 0, expr->line);
        } break;
        case Token_LESS_EQUAL: {
            instr(interp, LESS_THAN_EQUALS, 0, expr->line);
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

    case NODE_CALL: {
        u64 index = reserve_constant(interp);
        compile_call(interp, expr);
        instr(interp, STORE_ARG_OR_RETVAL, index, expr->line);
        return index;
    } break;

    default: {
        assert(false);
    } break;
    }
}

void compile_let(Interp *interp, AstNode *node) {
    AstLet let = node->let;

    // For now at least, a variable is just a named reference to a slot in the constants table.
    u64 variable_index;

    if (let.flags & DECL_NON_MUTABLE) {
        variable_index = reserve_non_mutable(interp);
    } else {
        variable_index = reserve_constant(interp);
    }
    
    node->let.constant_pool_index = variable_index; // for name lookup

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
    if (interp->scope->constant_pool.data[target_index].non_mutable) {
        compile_error(interp, node, "attempt to change value of const symbol");
        return;
    }

    u64 value_index  = compile_expr(interp, ass.right);

    switch (ass.op) {
    case Token_EQUAL: {
        instr(interp, LOAD, value_index, node->line);
    } break;

    case Token_PLUS_EQUAL: {
        instr(interp, LOAD, target_index, node->line);
        instr(interp, LOAD, value_index, node->line);
        instr(interp, ADD, 0, node->line);
    } break;

    case Token_MINUS_EQUAL: {
        instr(interp, LOAD, target_index, node->line);
        instr(interp, LOAD, value_index, node->line);
        instr(interp, SUB, 0, node->line);
    } break;

    case Token_STAR_EQUAL: {
        instr(interp, LOAD, target_index, node->line);
        instr(interp, LOAD, value_index, node->line);
        instr(interp, MUL, 0, node->line);
    } break;

    case Token_SLASH_EQUAL: {
        instr(interp, LOAD, target_index, node->line);
        instr(interp, LOAD, value_index, node->line);
        instr(interp, DIV, 0, node->line);
    } break;

    default: {
        assert(false);
    } break;
    }

    instr(interp, STORE, target_index, node->line);
}

// Compile each expression in a list, and emit loads for each one.
// Returns the number of expressions in the list.
u64 compile_loads_for_expression_list(Interp *interp, AstNode *list, bool args) {
    u64 i = list->expression_list.expressions.length;
    u64 j = i;
    for (; j > 0; j--) {
        AstNode *expr = list->expression_list.expressions.data[j-1];
        u64 value_index = compile_expr(interp, expr);
        instr(interp, (args ? LOAD_ARG : LOAD), value_index, expr->line);
    }
    return i;
}

void compile_call(Interp *interp, AstNode *call) {
    AstNode *name = call->call.name;
    if (name->tag == NODE_IDENTIFIER) {
        char *name_ident = name->identifier;

        // Temporary hard-coded built-ins lookup.
        if (strcmp(name_ident, "print") == 0) {
            s32 num_args = compile_loads_for_expression_list(interp, call->call.args, true);
            instr(interp, PRINT, num_args, name->line);
            return;
        }

        if (strcmp(name_ident, "append") == 0) {
            Ast args = call->call.args->expression_list.expressions;
            if (args.length != 2) {
                compile_error(interp, call, "'append' takes 2 arguments");
                return;
            }
            u64 value_loc = compile_expr(interp, args.data[1]);
            u64 array_loc = compile_expr(interp, args.data[0]);
            instr(interp, LOAD_ARG, value_loc, call->line);
            instr(interp, APPEND, array_loc, call->line);
            return;
        }

        s32 num_args = compile_loads_for_expression_list(interp, call->call.args, true);

        for (int i = 0; i < interp->root_scope->ast.length; i++) {
            AstNode *n = interp->root_scope->ast.data[i];
            if (n->tag != NODE_LAMBDA) continue;

            AstLambda f = n->lambda;

            if (strcmp(name_ident, f.name) == 0) {
                int expected_num_args = f.args->expression_list.expressions.length;
            
                if (expected_num_args < num_args) {
                    compile_error(interp, call, "too many arguments provided at call to '%s'", name_ident);
                    return;
                }

                if (expected_num_args > num_args) {
                    compile_error(interp, call, "too few arguments provided at call to '%s'", name_ident);
                    return;
                }

                instr(interp, LOAD_PC, 0, call->line);
                instr(interp, CALL_FUNC, f.constant_pool_index, call->line);
                return;
            }
        }

        compile_error(interp, call, "undeclared identifier '%s'", name_ident);
    }
}

void compile_func(Interp *interp, AstNode *node) {
    u64 lambda_index = reserve_constant(interp);
    node->lambda.constant_pool_index = lambda_index;
    instr(interp, BEGIN_BLOCK, lambda_index, node->line);

    AstLambda f = node->lambda;
    AstBlock  b = f.block->block;

    Ast args = f.args->expression_list.expressions;

    u64 scope_index = push_frame(interp, b.statements);

    // compile each as lets
    for (int i = 0; i < args.length; i++) {
        assert(args.data[i]->tag == NODE_LET);
        args.data[i]->let.constant_pool_index = reserve_constant(interp);
    }
    
    instr(interp, LOAD_SCOPE, scope_index, node->line);

    for (int i = 0; i < args.length; i++) {
        instr(interp, STORE_ARG_OR_RETVAL, args.data[i]->let.constant_pool_index, node->line);
    }

    compile_block(interp, f.block);

    pop_frame(interp);
    instr(interp, POP_SCOPE_RETURN, 0, 0);

    instr(interp, END_BLOCK, lambda_index, 0); // TODO: line numbers
}

void compile_return(Interp *interp, AstNode *node) {
    AstReturn r = node->ret;
    if (r.value) {
        u64 value_index = compile_expr(interp, node->ret.value);
        instr(interp, POP_SCOPE_RETURN, value_index, node->line);
        return;
    }
    instr(interp, POP_SCOPE_RETURN, 0, node->line);
}

void compile_if(Interp *interp, AstNode *cf) {
    u64 condition_index = compile_expr(interp, cf->cf.condition);
    instr(interp, LOAD, condition_index, cf->line);

    instr(interp, JUMP_FALSE, 0, cf->line);
    u64 count = interp->instructions.length-1;

    u64 block_id = reserve_constant(interp);

    instr(interp, BEGIN_BLOCK, block_id, cf->cf.block->line);
    compile_block(interp, cf->cf.block);
    instr(interp, END_BLOCK, block_id, 0); // TODO line number

    Instruction *to_patch = (interp->instructions.data + count);
    to_patch->arg = interp->instructions.length-1;
}

typedef Array(u64) PatchLocations;
static PatchLocations breaks_to_patch;
static u64 continue_loc = 0;

void compile_loop(Interp *interp, AstNode *cf) {
    u64 condition_jump = interp->instructions.length-1;

    u64 condition_index = compile_expr(interp, cf->cf.condition);
    instr(interp, LOAD, condition_index, cf->line);

    // Emit incomplete JUMP_FALSE
    // We will use `patch_location` to patch this instruction once the block has been compiled.
    // NOTE: we only do this in case compile_block causes the instructions array to be reallocated.
    //       In such an instance, a pointer to the instruction made here could be invalidated.
    instr(interp, JUMP_FALSE, 0, cf->line);
    u64 patch_location = interp->instructions.length-1;

    u64 block_id = reserve_constant(interp);

    instr(interp, BEGIN_BLOCK, block_id, cf->cf.block->line);

    continue_loc = condition_jump;
    compile_block(interp, cf->cf.block);

    instr(interp, JUMP, condition_jump, 0);
    instr(interp, END_BLOCK, block_id, 0); // TODO line number

    // Do the aforementioned patching.
    u64 exit_loc = interp->instructions.length-1;
    Instruction *to_patch = (interp->instructions.data + patch_location);
    to_patch->arg = exit_loc;

    for (int i = 0; i < breaks_to_patch.length; i++) {
        u64 loc = breaks_to_patch.data[i];
        Instruction *instr = interp->instructions.data + loc;
        instr->arg = exit_loc;
    }

    // TODO maybe clear patches array here
}

void compile_break_continue(Interp *interp, AstNode *stmt) {
    AstBreakCont bc = stmt->break_cont;
    if (bc.which == Token_CONTINUE) {
        instr(interp, JUMP, continue_loc, stmt->line);
        return;
    }
    instr(interp, JUMP, 0, stmt->line);
    array_add(breaks_to_patch, interp->instructions.length-1);
}

void compile_statement(Interp *interp, AstNode *stmt) {
    switch (stmt->tag) {
    case NODE_LET: {
        compile_let(interp, stmt);
    } break;

    case NODE_LAMBDA: {
        compile_func(interp, stmt);
    } break;

    case NODE_CALL: {
        compile_call(interp, stmt);
    } break;

    case NODE_BINARY: {
        if (stmt->binary.op > Token_ASSIGNMENTS_START && stmt->binary.op < Token_ASSIGNMENTS_END) {
            compile_assignment(interp, stmt);
        }
    } break;

    case NODE_RETURN: {
        compile_return(interp, stmt);
    } break;

    case NODE_CONTROL_FLOW_IF: {
        compile_if(interp, stmt);
    } break;

    case NODE_CONTROL_FLOW_LOOP: {
        compile_loop(interp, stmt);
    } break;

    case NODE_BREAK_OR_CONTINUE: {
        compile_break_continue(interp, stmt);
    } break;

    default: {
        assert(false);
    } break;
    }
}

void compile_block(Interp *interp, AstNode *block) {
    if (!block->block.statements.data) return;

    push_block(&block_stack, block);

    for (u64 i = 0; i < block->block.statements.length; i++) {
        AstNode *stmt = block->block.statements.data[i];
        compile_statement(interp, stmt);
    }

    pop_block(&block_stack);
}

Interp compile(Ast ast, char *file_name) {
    Interp interp = {0};

    interp.pc = 0;
    interp.file_name = file_name;
    string_allocator_init(&interp.strings);
    array_init(interp.instructions, Instruction);
    interp.call_storage.top = 0;
    interp.call_stack.top = 0;
    interp.jump_stack.top = 0;

    StackFrame *root_scope = malloc(sizeof(StackFrame));

    root_scope->ast = ast;
    root_scope->parent = NULL;
    array_init(root_scope->constant_pool, Object);
    add_primitive_objects(root_scope);
    root_scope->stack.top = 0;

    interp.scope = root_scope;
    interp.root_scope = root_scope;

    init_blocks(&block_stack);
    array_init(breaks_to_patch, u64);

    for (u64 i = 0; i < ast.length; i++) {
        AstNode *node = ast.data[i];
        if (!node) break;
        if (node->tag != NODE_LAMBDA) continue;
        compile_func(&interp, node);
    }

    for (u64 i = 0; i < ast.length; i++) {
        AstNode *node = ast.data[i];
        if (!node) break;
        if (node->tag == NODE_LAMBDA) continue;
        compile_statement(&interp, node);
    }

    array_free(breaks_to_patch);

    instr(&interp, HALT, 0, 0);

    return interp;
}
