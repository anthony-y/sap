// The lexer transforms a chunk of bytes (typically a loaded file) into an array of Tokens, ready to be handed off to the parser.
#include "lexer.h"
#include "context.h"
#include "array.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Initializes a Lexer */
void lexer_init(Lexer *tz, const char *path, char *data) {
    tz->file_name = path;

    tz->line = 1;
    tz->column = 1;

    tz->last = Token_EOF;

    tz->curr = data;
    tz->start = data;

    string_allocator_init(&tz->string_allocator);
}

/*
    Helpers
*/
static inline bool is_end(Lexer *tz) {
    return *tz->curr == '\0';
}

static inline char next_character(Lexer *tz) {
    if (*tz->curr == '\n') {
        tz->line++;
        tz->column = 1;

    } else tz->column++;

    return *(tz->curr++);
}

static inline bool is_whitespace(char what) {
    return (what == ' '     ||
            what == '\t'    ||
            what == '\v'    ||
            what == '\f'    ||
            what == '\r');
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool is_numeric(char c) {
    return (c >= '0' && c <= '9');
}

static inline bool is_alpha_numeric(char c) {
    return ( is_alpha(c) ) || ( is_numeric(c) );
}

/* Creates a token with the given type */
Token token_new(Lexer *tz, TokenType type) {
    Token t;

    t.type   = type;
    t.line   = tz->line;
    t.column = tz->column;
    t.length = (s32)(tz->curr - tz->start);
    t.file = tz->file_name;

    t.text = (char *)string_allocator(&tz->string_allocator, t.length+1);
    strncpy(t.text, tz->start, t.length);

    if (type == Token_STRING_LIT) {
        t.text[t.length-1] = 0;
    } else {
        t.text[t.length] = 0;
    }

    tz->last = type;
    return t;
}

/*
Util functions
**************
*/
static bool next_if_match(Lexer *tz, char c) {
    if (*tz->curr != c || is_end(tz)) return false;

    tz->curr++;
    return true;
}

static inline void skip_whitespace(Lexer *tz) {
    /* Skip blank characters */
    while (is_whitespace(*tz->curr) && !is_end(tz))
        next_character(tz);

    /* Skip comments */
    if (*tz->curr == '/' && tz->curr[1] == '/') {
        tz->curr += 2;
        while (*tz->curr != '\n' && !is_end(tz))
            next_character(tz);
    }
}

static Token tokenize_string(Lexer *tz) {
    u64 start_line = tz->line;
    tz->start++;
    while (*tz->curr != '"' && !is_end(tz)) {
        if (*tz->curr == '\n') {
            fprintf(stderr, "%s:%lu: Error: unterminated string.\n", tz->file_name, start_line);
            return token_new(tz, Token_ERROR);
        }
        next_character(tz);
    }
    next_character(tz);
    return token_new(tz, Token_STRING_LIT);
}

static Token tokenize_number(Lexer *tz) {
    TokenType t = Token_INT_LIT;

    while (true) {
        // This is just to reduce line length on the if
        bool first = (is_numeric(*tz->curr) || *tz->curr == '.');
        bool second = (*tz->curr != '\n' && !is_end(tz));

        if (!(first && second)) break;

        if (*tz->curr == '.') t = Token_FLOAT_LIT;

        next_character(tz);
    }

    return token_new(tz, t);
}

static Token tokenize_ident_or_keyword(Lexer *tz) {
    while ((is_alpha_numeric(*tz->curr) || *tz->curr == '_')
        && (!is_end(tz) && *tz->curr != '\n')) {
        
        next_character(tz);
    }

    int len = (tz->curr - tz->start);

    if (len == 4) {
        if (strncmp(tz->start, "true", 4) == 0) return token_new(tz, Token_TRUE);
        if (strncmp(tz->start, "then", 4) == 0) return token_new(tz, Token_THEN);
        if (strncmp(tz->start, "else", 4) == 0) return token_new(tz, Token_ELSE);
        if (strncmp(tz->start, "func", 4) == 0) return token_new(tz, Token_FUNC);
        if (strncmp(tz->start, "null", 4) == 0) return token_new(tz, Token_NULL);
        if (strncmp(tz->start, "func", 4) == 0) return token_new(tz, Token_FUNC);

        return token_new(tz, Token_IDENT);
    }

    if (len == 5) {
        if (strncmp(tz->start, "false", 5)  == 0) return token_new(tz, Token_FALSE);
        if (strncmp(tz->start, "while", 5)  == 0) return token_new(tz, Token_WHILE);
        if (strncmp(tz->start, "const", 5)  == 0) return token_new(tz, Token_CONST);
        if (strncmp(tz->start, "break", 5)  == 0) return token_new(tz, Token_BREAK);

        return token_new(tz, Token_IDENT);
    }

    if (len == 6) {
        if (strncmp(tz->start, "struct", 6) == 0) return token_new(tz, Token_STRUCT);
        if (strncmp(tz->start, "return", 6) == 0) return token_new(tz, Token_RETURN);
        if (strncmp(tz->start, "import", 6) == 0) return token_new(tz, Token_IMPORT);

        return token_new(tz, Token_IDENT);
    }

    if (len == 3) {
        if (strncmp(tz->start, "let", 3)    == 0) return token_new(tz, Token_LET);
        if (strncmp(tz->start, "for", 3)    == 0) return token_new(tz, Token_FOR);

        return token_new(tz, Token_IDENT);
    }

    if (len == 8 && strncmp(tz->start, "continue", 8) == 0) return token_new(tz, Token_CONTINUE);
    if (len == 2 && strncmp(tz->start, "if", 2)     == 0) return token_new(tz, Token_IF);

    return token_new(tz, Token_IDENT);
}

Token next_token(Lexer *tz) {
    skip_whitespace(tz);

    tz->start = tz->curr;

    // next_character returns the current character and then increments the pointer.
    char c = next_character(tz);

    if (c == '\n') {
        // Automatic semi-colon insertion based on the rules described here:
        //    https://medium.com/golangspec/automatic-semicolon-insertion-in-go-1990338f2649
        switch (tz->last) {
        case Token_SEMI_COLON: return next_token(tz);

        case Token_RESERVED_TYPE:
        case Token_CLOSE_PAREN:
        case Token_IDENT:
            // In these cases, if we were to insert a semi-colon, it would cause issues
            // for scope delimeters. For example:
            //     proc example(): int[;] <--- semi-colon would be inserted here and would give an error
            //     {
            //         return 10
            //     }
            //
            if (*tz->curr == '{') return next_token(tz);

        case Token_STRING_LIT:
        case Token_FLOAT_LIT:
        case Token_TRUE:
        case Token_FALSE:
        case Token_NULL:
        case Token_INT_LIT:

        case Token_RETURN:
        case Token_BREAK:
        case Token_CONTINUE:

        case Token_CLOSE_BRACE:
        case Token_CLOSE_BRACKET: {
            // The above call to next_character incremented the line already
            // but if there was a real semi-colon it would of course be on the same line.
            Token tmp = token_new(tz, Token_SEMI_COLON);
            tmp.line--;
            tmp.text = ";";
            return tmp;
        }
        
        // We increment the lexers line variable in next_character so that it happens inside of
        // specific lexing functions (e.g skip_whitespace) and not just at the root level.
        //
        // next_character doesn't skip over new lines though so we still might encounter it when
        // looking for the next token, however producing tokens from new lines creates verbosity
        // later on in the parser, so we just recurse until we get a token that isn't a newline.
        default:
            return next_token(tz);
        }
    }

    if (c == '\0') return token_new(tz, Token_EOF);

    if (is_numeric(c))           return tokenize_number(tz);
    if (is_alpha(c) || c == '_') return tokenize_ident_or_keyword(tz);

    switch(c) {
        case '(': return token_new(tz, Token_OPEN_PAREN);
        case ')': return token_new(tz, Token_CLOSE_PAREN);
        case '{': return token_new(tz, Token_OPEN_BRACE);
        case '}': return token_new(tz, Token_CLOSE_BRACE);
        case '[': return token_new(tz, Token_OPEN_BRACKET);
        case ']': return token_new(tz, Token_CLOSE_BRACKET);
        case ',': return token_new(tz, Token_COMMA);
        case ':': return token_new(tz, Token_COLON);
        case ';': return token_new(tz, Token_SEMI_COLON);
        case '^': return token_new(tz, Token_CARAT);
        case '#': return token_new(tz, Token_HASH);

        // Binary operators: +, *=, <, /=, !=, ==, etc.
        case '*': return token_new(tz, next_if_match(tz, '=') ? Token_STAR_EQUAL    : Token_STAR);
        case '/': return token_new(tz, next_if_match(tz, '=') ? Token_SLASH_EQUAL   : Token_SLASH);
        case '!': return token_new(tz, next_if_match(tz, '=') ? Token_BANG_EQUAL    : Token_BANG);
        case '=': return token_new(tz, next_if_match(tz, '=') ? Token_EQUAL_EQUAL   : next_if_match(tz, '>') ? Token_BIG_ARROW : Token_EQUAL);
        case '+': return token_new(tz, next_if_match(tz, '=') ? Token_PLUS_EQUAL    : Token_PLUS);
        case '>': return token_new(tz, next_if_match(tz, '=') ? Token_GREATER_EQUAL : Token_GREATER);
        case '<': return token_new(tz, next_if_match(tz, '=') ? Token_LESS_EQUAL    : Token_LESS);
        case '-': return token_new(tz, next_if_match(tz, '=') ? Token_MINUS_EQUAL   : next_if_match(tz, '>') ? Token_ARROW : Token_MINUS);

        case '&': return token_new(tz, next_if_match(tz, '&') ? Token_AMP_AMP       : Token_AMPERSAN);
        case '|': return token_new(tz, next_if_match(tz, '|') ? Token_BAR_BAR       : Token_BAR);

        case '.': return token_new(tz, next_if_match(tz, '.') ? Token_DOT_DOT       : Token_DOT);

        case '"': return tokenize_string(tz);
    }

    fprintf(stderr, "%s:%lu: Error: unknown character '%c'\n", tz->file_name, tz->line, c);
    return token_new(tz, Token_UNKNOWN);
}

bool lexer_lex(Lexer *l, TokenList *out) {
    array_init(*out, Token);
    while (true) {
        Token t = next_token(l);
        if (t.type == Token_ERROR) return false;
        array_add(*out, t);
        if (t.type == Token_EOF) break;
    }
    return true;
}

void token_list_print(const TokenList list) {
    printf("\nThere are %ld tokens, here they are:\n", list.length);
    for (u64 i = 0; i < list.length; i++) token_print(list.data[i]);
}

void token_print(Token t) {
    static u64 c = 1;

    printf("%lu. ", c++);
    if (t.type == Token_SEMI_COLON)
        printf("; (%d) on line %lu\n", t.type, t.line);
    else
        printf("%s (%d) on line %lu\n", t.text, t.type, t.line);
}
