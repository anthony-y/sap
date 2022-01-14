#include "lexer.h"
#include "token.h"
#include "common.h"
#include "context.h"
#include "parser.h"

#include <stdio.h>
#include <string.h>

int main(int arg_count, char *args[]) {
    if (arg_count < 2) return -1;
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
    if (verbose) token_list_print(&tokens);

    parser_init(&parser, tokens, args[1]);
    ast = run_parser(&parser);
    if (parser.error_count > 0) {
        printf("\nThere were errors, exiting.\n");
        return -1; // TODO lots of leaks here
    }

    if (verbose) printf("\nThere are %ld nodes in the AST.\n", ast.length);

    interp = compile(ast);
    if (interp.error_count > 0) {
        printf("\nThere were errors, exiting.\n");
        return -1; // TODO lots of leaks here
    }

    if (verbose) {
        printf("\nInstruction dump:\n");
        for (u64 i = 0; i < interp.instructions.length; i++) {
            Instruction instr = interp.instructions.data[i];
            printf("%s %d\n", instruction_strings[instr.op], instr.arg);
        }
        printf("\n");
    }

    run_interpreter(&interp);
    if (interp.error_count > 0) {
        printf("\nThere were errors, exiting.\n");
        return -1; // TODO lots of leaks here
    }

    arena_free(&lexer.string_allocator);
    array_free(ast);

    return 0;
}

static void test_stack() {
    Stack stack;
    stack_init(&stack);
    
    for (int i = 0; i < 5; i++) {
        Object o = (Object){.integer = i, .tag = OBJECT_INTEGER};
        stack_push(&stack, o);
    }

    for (int i = 0; i < 5; i++) {
        Object o = stack_pop(&stack);
        printf("%ld\n", o.integer);
    }
}
