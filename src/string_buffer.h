#ifndef STRING_BUFFER_h
#define STRING_BUFFER_h

#include "common.h"

#define STRING_BUFFER_LENGTH 1024

typedef struct StringBuffer {
    u8 data[STRING_BUFFER_LENGTH];
    u32 used;
    struct StringBuffer *next;
} StringBuffer;

typedef struct StringAllocator {
    StringBuffer *first;
    StringBuffer *current;
    u64 num_buffers;
} StringAllocator;

bool string_allocator_init(StringAllocator *sa);
u8  *string_allocator(StringAllocator *sa, u32 length);
void string_allocator_free(StringAllocator *sa);

#endif
