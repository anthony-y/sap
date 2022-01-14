// Mostly utility functions for find declarations in scopes, error logging, and initialising the Context struct.
#include "context.h"

#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//
// First-in-last-out stacks for procedures and blocks.
//
void stack_push(Stack *s, Object obj) {
    s->data[++s->top] = obj;
    assert(s->top <= CONTEXT_STACK_SIZE);
}

Object stack_pop(Stack *s) {
    Object obj = s->data[s->top];
    s->data[s->top--] = (Object){0, 0};
    return obj;
}

Object stack_top(Stack s) {
    return s.data[s.top];
}

AstNode *find_decl(Ast ast, char *name) {
    for (u64 i = 0; i < ast.length; i++) {
        AstNode *node = ast.data[i];
        if (node->tag != NODE_LET) continue;
        char *node_name = node->let.name;
        if (strcmp(name, node_name) == 0) {
            return node;
        }
    }
    return NULL;
}

char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: failed to open file %s.\n", path);
        exit(0);
    }

    fseek(f, 0L, SEEK_END);
    u64 file_length = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(file_length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: not enough memory to read \"%s\".\n", path);
        fclose(f);
        exit(0);
    }

    u64 bytes_read = fread(buffer, sizeof(char), file_length, f);
    if (bytes_read < file_length) {
        fprintf(stderr, "Error: failed to read file \"%s\".\n", path);
        fclose(f);
        exit(0);
    }

    buffer[bytes_read] = '\0';

    fclose(f);

    return buffer;
}

/*
void compile_error(Context *ctx, Token t, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // The weird looking escape characters are to set the text color
    // to red, print "Error", and then reset the colour.
    fprintf(stderr, "%s:%lu: \033[0;31mError\033[0m: ", t.file, t.line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, ".\n");
    va_end(args);

    ctx->error_count++;
}

void compile_error_start(Context *ctx, Token t, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "%s:%lu: \033[0;31mError\033[0m: ", t.file, t.line);
    vfprintf(stderr, fmt, args);
    va_end(args);

    ctx->error_count++;
}

void compile_error_add_line(Context *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void compile_error_end() {
    fprintf(stderr, ".\n");
}

void compile_warning(Context *ctx, Token t, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "%s:%lu: \033[0;33mWarning\033[0m: ", t.file, t.line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, ".\n");
    va_end(args);
}
*/
