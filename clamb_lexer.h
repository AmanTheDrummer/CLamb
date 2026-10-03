#ifndef CLAMB_LEXER_H
#define CLAMB_LEXER_H

#include <stdio.h>

#define MAX_LEXEME 256

typedef enum {
    TOK_NONE,
    TOK_DYNAMIC_IDENT,
    TOK_DATA_TYPE,
    TOK_KW_IF, TOK_KW_ELSE, TOK_KW_FOR, TOK_KW_WHILE, TOK_KW_RETURN,
    TOK_KW_LAMB, TOK_KW_NULL, TOK_KW_PRINT, TOK_KW_MALLOC, TOK_KW_FREE,
    TOK_IDENTIFIER, TOK_INTEGER_LITERAL,
    TOK_FLOAT_LITERAL, TOK_CHAR_LITERAL, TOK_STRING_LITERAL, TOK_BOOLEAN_LITERAL,
    TOK_ADD_OP, TOK_MUL_OP, TOK_ASTERISK,
    TOK_AND_OP, TOK_OR_OP, TOK_NOT_OP,
    TOK_EQ_OP, TOK_NE_OP, TOK_LT_OP, TOK_LE_OP, TOK_GT_OP, TOK_GE_OP,
    TOK_ASSIGNMENT_OP, TOK_ADDRESS_OP, TOK_L_PAREN, TOK_R_PAREN, TOK_L_BRACE,
    TOK_R_BRACE, TOK_L_BRACKET, TOK_R_BRACKET, TOK_SEMICOLON, TOK_COMMA,
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[MAX_LEXEME];
    int line;
} Token;

void clamb_lexer_init(FILE *input);
Token clamb_lexer_next_token(void);
int clamb_lexer_error_count(void);
const char *clamb_token_type_name(TokenType type);

#endif