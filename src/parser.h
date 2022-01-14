#ifndef PARSER_h
#define PARSER_h

#include "token.h"
#include "ast.h"

typedef struct Parser {
    Token *curr;
    Token *prev;
    char *file_name;
    u64 error_count;
} Parser;

void parser_init(Parser *, const TokenList, char *file_name);
Ast  run_parser(Parser *p);

#endif