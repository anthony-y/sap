#ifndef LEXER_h
#define LEXER_h

#include "token.h"
#include "arena.h"

typedef struct Lexer {
    const char *file_name;

    char *start;
    char *curr;
    
    u64 line;
    u32 column;

    TokenType last;
    Arena string_allocator;
} Lexer;

void lexer_init(Lexer *, const char *path, char *data);

bool lexer_lex(Lexer *, struct TokenList *list);
Token next_token(Lexer *);

#endif
