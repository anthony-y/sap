#ifndef PARSER_h
#define PARSER_h

#include "lexer.h"
#include "ast.h"

#define NODE_BLOCK_LENGTH 64

typedef struct NodeBlock {
    AstNode data[NODE_BLOCK_LENGTH];
    u32 num_nodes;
    struct NodeBlock *next;
} NodeBlock;

typedef struct NodeAllocator {
    NodeBlock *first;
    NodeBlock *current;
    u64 num_blocks;
    u64 total_nodes;
} NodeAllocator;

bool     node_allocator_init(NodeAllocator *sa);
AstNode *node_allocator(NodeAllocator *sa);
void     node_allocator_free(NodeAllocator *sa);

typedef struct Parser {
    Token *token;
    Token *before;
    char *file_name;
    u64 error_count;
    NodeAllocator node_allocator;
} Parser;

void parser_init(Parser *, const TokenList, char *file_name);
Ast  run_parser(Parser *p);

#endif
