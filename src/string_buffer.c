#include "string_buffer.h"

#include <stdlib.h>
#include <stdio.h>

bool string_allocator_init(StringAllocator *sa) {
    StringBuffer *memory = (StringBuffer *)malloc(sizeof(StringBuffer));
    if (!memory) {
        return false;
    }
    *memory = (StringBuffer){0};
    sa->first   = memory;
    sa->current = memory;
    sa->num_buffers = 1;
    return true;
}

u8 *string_allocator(StringAllocator *sa, u32 length) {
    if ((sa->current->used+length) > STRING_BUFFER_LENGTH) {
        StringBuffer *next = (StringBuffer *)malloc(sizeof(StringBuffer));
        if (!next) {
            printf("bad news, out of memory");
            return NULL;
        }
        sa->current->next = next;
        sa->current = next;
        sa->num_buffers++;
    }
    u8 *out = sa->current->data+sa->current->used;
    sa->current->used += length+1;
    return out;
}

void string_allocator_free(StringAllocator *sa) {
    StringBuffer *buffer = sa->first;
    while (buffer) {
        StringBuffer *current = buffer;
        buffer = current->next;
        free(current);
    }
}
