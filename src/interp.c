#include "context.h"
#include "array.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

static void runtime_error(Interp *interp, Instruction instr, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // The weird looking escape characters are to: set the text color to red, print "Error", and then reset the colour.
    fprintf(stderr, "%s:%lu: \033[0;31mRuntime error\033[0m: ", interp->file_name, instr.line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, ".\n");
    va_end(args);

    interp->error_count++;
}

static void runtime_print(Object value) {
    switch (value.tag) {
    case OBJECT_BOOLEAN:   printf("%s", (value.boolean ? "true" : "false")); break;
    case OBJECT_INTEGER:   printf("%ld", value.integer); break;
    case OBJECT_FLOATING:  printf("%f", value.floating); break;
    case OBJECT_STRING:    printf("%s", value.pointer); break;
    case OBJECT_NULL:      printf("null"); break;
    case OBJECT_UNDEFINED: printf("undefined"); break;

    case OBJECT_ARRAY: {
        printf("[%d]{", value.array.length);
        for (int i = 0; i < value.array.length; i++) {
            runtime_print(value.array.data[i]);
            if (i < value.array.length-1) {
                printf(", ");
            }
        }
        printf("}");
    } break;

    default: assert(false); break;
    }
}

static u8 runtime_equals1(Object a, Object b) {
    if (a.tag == OBJECT_FLOATING) {
        if (b.tag == OBJECT_INTEGER) {
            return ((u64)a.floating == b.integer);
        }
    }

    if (a.tag != b.tag) return 0;

    switch (a.tag) {
    case OBJECT_FLOATING:  return a.floating == b.floating;            break;
    case OBJECT_STRING:    return (strcmp(a.pointer, b.pointer) == 0); break;
    case OBJECT_INTEGER:   return a.integer == b.integer;              break;
    case OBJECT_BOOLEAN:   return a.boolean == b.boolean;              break;
    case OBJECT_NULL:      return (b.tag == OBJECT_NULL);              break;
    case OBJECT_UNDEFINED: return (b.tag == OBJECT_UNDEFINED);         break;
    
    default: assert(false); break;
    }
}

static u8 runtime_equals(Object a, Object b) {
    return (runtime_equals1(a, b) || runtime_equals1(b, a));
}

static char *runtime_string_concat(Interp *interp, Object a, Object b) {
    u64 a_len = strlen(a.pointer);
    u64 b_len = strlen(b.pointer);
    u64 new_length = a_len + b_len + 1;

    char *result = string_allocator(&interp->strings, new_length);
    strcpy(result, a.pointer);
    strcat(result, b.pointer);

    result[new_length] = 0;
    return result;
}

void run_interpreter(Interp *interp) {
    frame_push(interp, interp->root_scope);
    StackFrame *scope = frame_top(interp);

    while (true) {
        if (interp->pc >= interp->instructions.length || interp->pc < 0) {
            break;
        }

        Instruction instr = interp->instructions.data[interp->pc];
        scope = frame_top(interp);

        switch (instr.op) {

        case HALT: {
            break;
        } break;

        case CONST: {
            assert(false);
        } break;

        case LOAD: {
            stack_push(&scope->stack, scope->constant_pool.data[instr.arg]);
        } break;

        case LOAD_ARG: {
            stack_push(&interp->call_storage, scope->constant_pool.data[instr.arg]);
        } break;

        case STORE_ARG_OR_RETVAL: {
            Object arg = stack_pop(&interp->call_storage);
            scope->constant_pool.data[instr.arg] = arg;
        } break;

        case STORE: {
            Object arg = stack_pop(&scope->stack);
            scope->constant_pool.data[instr.arg] = arg;
        } break;

        case EQUALS: {
            Object left = stack_pop(&scope->stack);
            Object right = stack_pop(&scope->stack);

            Object result = (Object){
                .tag=OBJECT_BOOLEAN,
                .boolean=runtime_equals(left, right),
            };
            
            stack_push(&scope->stack, result);
        } break;

        case PRINT: {
            for (int i = 0; i < instr.arg; i++) {
                runtime_print(stack_pop(&interp->call_storage));
                printf("\n");
            }
        } break;

        case APPEND: {
            Object value  = stack_pop(&interp->call_storage);
            Object target = scope->constant_pool.data[instr.arg];

            if (target.tag != OBJECT_ARRAY) {
                runtime_error(interp, instr, "attempt to append to non-array");
                return;
            }

            array_add(target.array, value);
            stack_push(&interp->call_storage, target);
        } break;

        case BEGIN_BLOCK: {
            s32 block_id = instr.arg;
            interp->root_scope->constant_pool.data[instr.arg].integer = interp->pc+1;
            while (true) {
                instr = interp->instructions.data[interp->pc];
                if (instr.op == END_BLOCK && instr.arg == block_id) break;
                interp->pc++;
            }
        } break;

        case END_BLOCK: {
        } break;

        case LOAD_SCOPE: {
            StackFrame *new_scope = interp->root_scope->constant_pool.data[instr.arg].scope;
            frame_push(interp, new_scope);
        } break;

        case POP_SCOPE: {
            frame_pop(interp);
        } break;

        case POP_SCOPE_RETURN: {
            // Return to caller instruction
            interp->pc = stack_pop(&interp->jump_stack).integer;

            // Push return value
            stack_push(&interp->call_storage, scope->constant_pool.data[instr.arg]);

            // Return to last scope
            frame_pop(interp);
        } break;

        case LOAD_PC: {
            Object pc = (Object){0};
            pc.integer = interp->pc+1;
            pc.tag = OBJECT_INTEGER;
            stack_push(&interp->jump_stack, pc);
        } break;

        case CALL_FUNC: {
            // Jump to the new place
            interp->pc = interp->root_scope->constant_pool.data[instr.arg].integer;
            continue;
        } break;

        case JUMP_TRUE: {
            Object what = stack_pop(&scope->stack);
            assert(what.tag == OBJECT_BOOLEAN);

            // It's zero so we need to jump to where the argument says
            if (what.boolean == 1) {
                // TODO may need to add to jump stack before modifying
                interp->pc = instr.arg;
            } else {
                interp->pc++;
            }
        } break;

        case JUMP_FALSE: {
            Object what = stack_pop(&scope->stack);
            assert(what.tag == OBJECT_BOOLEAN);

            if (what.boolean == 0) {
                interp->pc = instr.arg;
            } else {
                interp->pc++;
            }
        } break;

        case JUMP: {
            interp->pc = instr.arg;
        } break;

        case NEG: {
            Object negate = stack_pop(&scope->stack);
            Object result = (Object){0};
            result.tag = negate.tag;
            if (negate.tag == OBJECT_INTEGER) {
                result.integer = -negate.integer;
            } else if (negate.tag == OBJECT_FLOATING) {
                result.floating = -negate.floating;
            } else {
                runtime_error(interp, instr, "operand of unary negation must be numerical");
                return;
            }
            stack_push(&scope->stack, result);
        } break;

        case GREATER_THAN: {
            Object right = stack_pop(&scope->stack);
            Object left  = stack_pop(&scope->stack);

            if (left.tag != right.tag) {
                runtime_error(interp, instr, "type mismatch: cannot compare two different types");
                return;
            }

            Object result = (Object){0};
            result.tag = OBJECT_BOOLEAN;

            switch (left.tag) {
            case OBJECT_INTEGER: {
                result.boolean = (left.integer > right.integer);
            } break;
            case OBJECT_FLOATING: {
                result.boolean = (left.floating > right.floating);
            } break;

            default: {
                runtime_error(interp, instr, "operands of '>' must be integer or float");
                return;
            } break;
            }

            stack_push(&scope->stack, result);
        } break;

        case GREATER_THAN_EQUALS: {
            Object right = stack_pop(&scope->stack);
            Object left  = stack_pop(&scope->stack);

            if (left.tag != right.tag) {
                runtime_error(interp, instr, "type mismatch: cannot compare two different types");
                return;
            }

            Object result = (Object){0};
            result.tag = OBJECT_BOOLEAN;

            switch (left.tag) {
            case OBJECT_INTEGER: {
                result.boolean = (left.integer >= right.integer);
            } break;
            case OBJECT_FLOATING: {
                result.boolean = (left.floating >= right.floating);
            } break;

            default: {
                runtime_error(interp, instr, "operands of '>=' must be integer or float");
                return;
            } break;
            }

            stack_push(&scope->stack, result);
        } break;

        case LESS_THAN: {
            Object right = stack_pop(&scope->stack);
            Object left  = stack_pop(&scope->stack);

            if (left.tag != right.tag) {
                runtime_error(interp, instr, "type mismatch: cannot compare two different types");
                return;
            }

            Object result = (Object){0};
            result.tag = OBJECT_BOOLEAN;

            switch (left.tag) {
            case OBJECT_INTEGER: {
                result.boolean = (left.integer < right.integer);
            } break;
            case OBJECT_FLOATING: {
                result.boolean = (left.floating < right.floating);
            } break;

            default: {
                runtime_error(interp, instr, "operands of '<' must be integer or float");
                return;
            } break;
            }

            stack_push(&scope->stack, result);
        } break;

        case LESS_THAN_EQUALS: {
            Object right = stack_pop(&scope->stack);
            Object left  = stack_pop(&scope->stack);

            if (left.tag != right.tag) {
                runtime_error(interp, instr, "type mismatch: cannot compare two different types");
                return;
            }

            Object result = (Object){0};
            result.tag = OBJECT_BOOLEAN;

            switch (left.tag) {
            case OBJECT_INTEGER: {
                result.boolean = (left.integer <= right.integer);
            } break;
            case OBJECT_FLOATING: {
                result.boolean = (left.floating <= right.floating);
            } break;

            default: {
                runtime_error(interp, instr, "operands of '<=' must be integer or float");
                return;
            } break;
            }

            stack_push(&scope->stack, result);
        } break;

        case ADD: {
            Object right = stack_pop(&scope->stack);
            Object left  = stack_pop(&scope->stack);

            if (left.tag != right.tag) {
                runtime_error(interp, instr, "type mismatch: cannot add two different types");
                return;
            }

            Object result = (Object){0};
            assert(left.tag == right.tag);
            result.tag = left.tag;

            switch (left.tag) {
            case OBJECT_INTEGER: {
                result.integer = left.integer + right.integer;
            } break;
            case OBJECT_FLOATING: {
                result.floating = left.floating + right.floating;
            } break;
            case OBJECT_STRING: {
                result.pointer = runtime_string_concat(interp, left, right);
            } break;

            default: {
                runtime_error(interp, instr, "operands of addition must be integer, float or string");
                return;
            } break;
            }
            stack_push(&scope->stack, result);
        } break;

        case SUB: {
            Object right = stack_pop(&scope->stack);
            Object left  = stack_pop(&scope->stack);

            if (left.tag != right.tag) {
                runtime_error(interp, instr, "type mismatch: cannot subtract two different types");
                return;
            }

            Object result = (Object){0};
            assert(left.tag == right.tag);
            result.tag = left.tag;

            switch (left.tag) {
            case OBJECT_INTEGER: {
                result.integer = left.integer - right.integer;
            } break;
            case OBJECT_FLOATING: {
                result.floating = left.floating - right.floating;
            } break;

            default: {
                runtime_error(interp, instr, "operands of subtraction must be numerical");
                return;
            } break;
            }
            stack_push(&scope->stack, result);
        } break;

        case MUL: {
            Object right = stack_pop(&scope->stack);
            Object left  = stack_pop(&scope->stack);

            if (left.tag != right.tag) {
                runtime_error(interp, instr, "type mismatch: cannot multiply two different types");
                return;
            }

            Object result = (Object){0};
            assert(left.tag == right.tag);
            result.tag = left.tag;

            switch (left.tag) {
            case OBJECT_INTEGER: {
                result.integer = left.integer * right.integer;
            } break;
            case OBJECT_FLOATING: {
                result.floating = left.floating * right.floating;
            } break;

            default: {
                runtime_error(interp, instr, "operands of multiplication must be numerical");
                return;
            } break;
            }
            stack_push(&scope->stack, result);
        } break;

        case DIV: {
            Object right = stack_pop(&scope->stack);
            Object left  = stack_pop(&scope->stack);

            if (left.tag != right.tag) {
                runtime_error(interp, instr, "type mismatch: cannot multiply two different types");
                return;
            }

            Object result = (Object){0};
            assert(left.tag == right.tag);
            result.tag = left.tag;

            switch (left.tag) {
            case OBJECT_INTEGER: {
                result.integer = left.integer / right.integer;
            } break;
            case OBJECT_FLOATING: {
                result.floating = left.floating / right.floating;
            } break;

            default: {
                runtime_error(interp, instr, "operands of division must be numerical");
                return;
            } break;
            }
            stack_push(&scope->stack, result);
        } break;

        
        default: {
            assert(false);
        } break;
        }

        interp->pc++;
    }
}
