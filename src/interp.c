#include "context.h"
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
    default: assert(false); break;
    }
    printf("\n");
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

static char *runtime_string_concat(Object a, Object b) {
    u64 a_len = strlen(a.pointer);
    u64 b_len = strlen(b.pointer);
    u64 new_length = a_len + b_len + 1;

    char *result = malloc(new_length);
    strcpy(result, a.pointer);
    strcat(result, b.pointer);

    result[new_length] = 0;
    return result;
}

void run_interpreter(Interp *interp) {
    for (u64 i = 0; i < interp->instructions.length; i++) {
        Instruction instr = interp->instructions.data[i];
        switch (instr.op) {
        case LOAD: {
            stack_push(&interp->stack, interp->constant_pool.data[instr.arg]);
        } break;

        case STORE: { // TODO type checking
            Object new_value = stack_pop(&interp->stack);
            interp->constant_pool.data[instr.arg] = new_value;
        } break;

        case NEG: {
            Object negate = stack_pop(&interp->stack);
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
            stack_push(&interp->stack, result);
        } break;

        case ADD: {
            Object right = stack_pop(&interp->stack);
            Object left  = stack_pop(&interp->stack);

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
                result.pointer = runtime_string_concat(left, right);
            } break;

            default: {
                runtime_error(interp, instr, "operands of addition must be integer, float or string");
                return;
            } break;
            }
            stack_push(&interp->stack, result);
        } break;

        case SUB: {
            Object right = stack_pop(&interp->stack);
            Object left  = stack_pop(&interp->stack);

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
            stack_push(&interp->stack, result);
        } break;

        case MUL: {
            Object right = stack_pop(&interp->stack);
            Object left  = stack_pop(&interp->stack);

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
            stack_push(&interp->stack, result);
        } break;

        case DIV: {
            Object right = stack_pop(&interp->stack);
            Object left  = stack_pop(&interp->stack);

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
            stack_push(&interp->stack, result);
        } break;

        case EQUALS: {
            Object left = stack_pop(&interp->stack);
            Object right = stack_pop(&interp->stack);

            Object result = (Object){
                .tag=OBJECT_BOOLEAN,
                .boolean=runtime_equals(left, right),
            };
            
            stack_push(&interp->stack, result);
        } break;

        case PRINT: {
            for (int i = 0; i < instr.arg; i++) {
                runtime_print(stack_pop(&interp->stack));
            }
        } break;

        default: {
            assert(false);
        } break;
        }
    }
}