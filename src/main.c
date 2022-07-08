#include "lexer.h"
#include "common.h"
#include "context.h"
#include "parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_stack() {
    Stack stack;
    stack.top = 0;

    static const int NUMBER_ITEMS = 5;
    
    for (int i = 0; i < NUMBER_ITEMS; i++) {
        Object o = (Object){.integer = i, .tag = OBJECT_INTEGER};
        stack_push(&stack, o);
    }

    for (int i = 1; i < NUMBER_ITEMS+1; i++) {
        Object o = stack_pop(&stack);
        assert(o.integer == NUMBER_ITEMS-i); // they should come out in the opposite order.
    }
}

int main(int arg_count, char *args[]) {
    test_stack();

    if (arg_count < 2) {
        printf("Please supply the path of the main module.\n");
        return -1;
    }

    char *file_data = read_file(args[1]);

    bool verbose = false;
    if (arg_count > 2 && strcmp(args[2], "-v") == 0) {
        verbose = true;
    }

    Lexer     lexer;
    TokenList tokens;
    Parser    parser;
    Ast       ast;
    Interp    interp;

    lexer_init(&lexer, args[1], file_data);
    if (!lexer_lex(&lexer, &tokens)) {
        printf("\nThere were errors, exiting.\n");
        return -1; // TODO lots of leaks here
    }
    if (verbose) token_list_print(tokens);

    parser_init(&parser, tokens, args[1]);
    ast = run_parser(&parser);
    if (parser.error_count > 0) {
        printf("\nThere were errors, exiting.\n");
        return -1; // TODO lots of leaks here
    }

    if (verbose) {
        printf("\nsizeof(AstNode) is %ld bytes.", sizeof(AstNode));
        printf("\nsizeof(Object) is %ld bytes.\n", sizeof(Object));
        printf("\nThere are %ld nodes in the AST (%ld top-level).", parser.node_allocator.total_nodes, ast.length);
        printf("\nThere are %ld blocks in the node allocator.\n", parser.node_allocator.num_blocks);
    }

    interp = compile(ast, args[1]);
    if (interp.error_count > 0) {
        printf("\nThere were errors, exiting.\n");
        return -1; // TODO lots of leaks here
    }

    if (verbose && !PRINT_INSTRUCTIONS_DURING_COMPILE) {
        printf("\nThere are %ld instructions, here they are:\n", interp.instructions.length);
        for (u64 i = 0; i < interp.instructions.length; i++) {
            Instruction instr = interp.instructions.data[i];
            printf("(%s%ld) Line %s%ld : %s %d\n", (i < 10 ? "0" : ""), i, (instr.line_number < 10 ? "0" : ""), instr.line_number, instruction_strings[instr.op], instr.arg);
        }
        printf("\nRunning the bytecode:\n");
    }

    run_interpreter(&interp);
    if (interp.error_count > 0) {
        printf("\nThere were errors, exiting.\n");
        return -1; // TODO lots of leaks here
    }
    
    string_allocator_free(&lexer.string_allocator);
    node_allocator_free(&parser.node_allocator);
    array_free(ast);
    free_interpreter(&interp);

    return 0;
}
