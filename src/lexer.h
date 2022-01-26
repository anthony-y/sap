#ifndef LEXER_h
#define LEXER_h

#include "array.h"

typedef enum TokenType {
    Token_EOF,
    Token_END_OF_CHUNK,
    Token_ERROR,
    Token_UNKNOWN,
    Token_COMMENT,

    Token_VALUE_START,
        Token_IDENT,

        Token_INT_LIT,
        Token_STRING_LIT,
        Token_FLOAT_LIT,
        Token_TRUE,
        Token_FALSE,
        Token_NULL,

        Token_RESERVED_TYPE,
    Token_VALUE_END,

    /* Keywords */
    Token_IMPORT,
    Token_RETURN,
    Token_IF,
    Token_ELSE,
    Token_FOR,
    Token_WHILE,
    Token_TO,
    Token_STRUCT,
    Token_ENUM,
    Token_TYPEDEF,
    Token_CAST,
    Token_DEFER,
    Token_THEN,
    Token_INLINE,
    Token_USING,
    Token_LET,
    Token_FUNC,

    Token_SIZE_OF,

    Token_SYMBOL_START,

    Token_CLOSE_PAREN,
    Token_OPEN_BRACE,
    Token_CLOSE_BRACE,
    Token_CLOSE_BRACKET,
    Token_COMMA,
    Token_COLON,
    Token_SEMI_COLON,
    Token_BANG,
    Token_AMPERSAN,
    Token_BAR,
    Token_DOT_DOT,
    Token_CARAT,
    Token_HASH,
    Token_ARROW,
    Token_PERCENT,

    Token_BINOP_START,

        // Postfix operators
        Token_OPEN_PAREN,
        Token_OPEN_BRACKET,
        //

        Token_ASSIGNMENTS_START,
            Token_EQUAL,
            Token_MINUS_EQUAL,
            Token_PLUS_EQUAL,
            Token_STAR_EQUAL,
            Token_SLASH_EQUAL,
        Token_ASSIGNMENTS_END,

        Token_BINARY_COMPARE_START,
            Token_AMP_AMP,
            Token_BAR_BAR,
            Token_LESS,
            Token_GREATER,
            Token_BANG_EQUAL,
            Token_GREATER_EQUAL,
            Token_LESS_EQUAL,
            Token_EQUAL_EQUAL,
        Token_BINARY_COMPARE_END,

        Token_PLUS,
        Token_MINUS,
        Token_SLASH,
        Token_STAR,

        Token_DOT,

    Token_BINOP_END,
    Token_SYMBOL_END,

    Token_COUNT
} TokenType;

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

typedef struct Token {
    TokenType type;

    u32 length;
    s64 line;
    u32 column;

    char *text;
    const char *file;
} Token;

typedef Array(Token) TokenList;

typedef struct Lexer {
    const char *file_name;

    char *start;
    char *curr;
    
    u64 line;
    u32 column;

    TokenType last;
    StringAllocator string_allocator;
} Lexer;

void lexer_init(Lexer *, const char *path, char *data);
bool lexer_lex(Lexer *l, TokenList *out);
Token next_token(Lexer *);
Token token_new(struct Lexer *, TokenType);
void token_list_print(const TokenList list);
void token_print(Token t);

#endif
