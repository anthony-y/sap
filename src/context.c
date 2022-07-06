// Mostly utility functions for find declarations in scopes, error logging, and initialising the Context struct.
#include "context.h"

#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void free_interpreter(Interp *interp) {
    string_allocator_free(&interp->strings);

    // MEMORY LEAK
    // array_free(interp->constant_pool);
    // array_free(interp->instructions);
}

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

void frame_push(Interp *s, Scope *frame) {
    s->call_stack.data[++s->call_stack.top] = frame;
    assert(s->call_stack.top <= CONTEXT_STACK_SIZE);
}

Scope *frame_pop(Interp *s) {
    Scope *f = s->call_stack.data[s->call_stack.top];
    s->call_stack.data[s->call_stack.top--] = s->root_scope;
}

Scope *frame_top(Interp *s) {
    return s->call_stack.data[s->call_stack.top];
}

AstNode *find_decl(Scope *scope, char *name) {
    if (!scope) return NULL;

    for (u64 i = 0; i < scope->ast.length; i++) {
        AstNode *node = scope->ast.data[i];
        if (node->tag != NODE_LET) continue;
        char *node_name = node->let.name;
        if (strcmp(name, node_name) == 0) {
            return node;
        }
    }

    return find_decl(scope->parent, name);
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
