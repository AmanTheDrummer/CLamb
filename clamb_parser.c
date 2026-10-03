#include <stdio.h>
#include <string.h>
#include "clamb_lexer.h"

static Token lookahead; // token holding the next token to be processed by the parser
static int failed;

// advances the next token from the lexer into the lookahead variable, 
// so that the parser can process it.
static void advance(void) {
    lookahead = clamb_lexer_next_token();
}

// prints error message for syntax errors
// including the line number, expected token type, and actual token type
static void syntax_error(const char *expected) {
    if (failed) return;
    if (lookahead.type == TOK_EOF) {
        fprintf(stderr, "Syntax Error at line %d: expected %s but found end of file\n",
                lookahead.line, expected);
    } else {
        fprintf(stderr, "Syntax Error at line %d: expected %s but found %s ('%s')\n",
                lookahead.line, expected,
                clamb_token_type_name(lookahead.type), lookahead.lexeme);
    }
    failed = 1;
}

// accepts the next token if it matches the expected token type
// advances the lookahead token, otherwise returns 0
// used for optional syntax
static int accept(TokenType type) {
    if (lookahead.type != type) return 0;
    advance();
    return 1;
}

// stricter syntax checking: expects the next token to match the expected type,
// advances the lookahead token if it does, otherwise reports a syntax error
static int expect(TokenType type, const char *description) {
    if (accept(type)) return 1;
    syntax_error(description);
    return 0;
}

// checks if token is a valid start of an expression
// returns 1 if it is, 0 otherwise
static int is_expression_start(void) {
    switch (lookahead.type) {
        case TOK_IDENTIFIER:
        case TOK_INTEGER_LITERAL:
        case TOK_FLOAT_LITERAL:
        case TOK_CHAR_LITERAL:
        case TOK_STRING_LITERAL:
        case TOK_BOOLEAN_LITERAL:
        case TOK_KW_NULL:
        case TOK_KW_LAMB:
        case TOK_KW_MALLOC:
        case TOK_L_PAREN:
        case TOK_ADDRESS_OP:
        case TOK_ASTERISK:
        case TOK_ADD_OP:
        case TOK_NOT_OP:
            return 1;
        default:
            return 0;
    }
}

static void parse_expression(void);
static void parse_statement(void);
static void parse_block(void);

static void parse_parameter_list(void) {
    if (lookahead.type == TOK_R_PAREN) return;
    if (lookahead.type != TOK_DATA_TYPE) {
        syntax_error("a parameter declaration or ')'");
        return;
    }

    do {
        expect(TOK_DATA_TYPE, "a data type");
        if (failed) return;
        accept(TOK_ASTERISK);
        if (!expect(TOK_IDENTIFIER, "a parameter name")) return;
    } while (accept(TOK_COMMA));
}

static void parse_declaration(void) {
    expect(TOK_DATA_TYPE, "a data type");
    if (failed) return;
    accept(TOK_ASTERISK);
    if (!expect(TOK_IDENTIFIER, "an identifier")) return;

    if (accept(TOK_L_PAREN)) {
        parse_parameter_list();
        if (!expect(TOK_R_PAREN, "')' after the parameter list")) return;
        parse_block();
        return;
    }

    if (accept(TOK_ASSIGNMENT_OP)) {
        parse_expression();
    } else if (accept(TOK_L_BRACKET)) {
        if (!expect(TOK_INTEGER_LITERAL, "an integer array size")) return;
        if (!expect(TOK_R_BRACKET, "']' after the array size")) return;
    }
    if (!failed) expect(TOK_SEMICOLON, "';' after the declaration");
}

static void parse_block(void) {
    if (!expect(TOK_L_BRACE, "'{' to begin a block")) return;
    while (!failed && lookahead.type != TOK_R_BRACE && lookahead.type != TOK_EOF)
        parse_statement();
    if (!failed) expect(TOK_R_BRACE, "'}' to close the block");
}

static void parse_expression_list(void) {
    if (lookahead.type == TOK_R_PAREN) return;
    parse_expression();
    while (!failed && accept(TOK_COMMA)) parse_expression();
}

static void parse_optional_expression(void) {
    if (is_expression_start()) parse_expression();
}

static void parse_if_statement(void) {
    expect(TOK_KW_IF, "'if'");
    if (!expect(TOK_L_PAREN, "'(' after 'if'")) return;
    parse_expression();
    if (!expect(TOK_R_PAREN, "')' after the condition")) return;
    parse_block();
    if (failed || !accept(TOK_KW_ELSE)) return;
    if (lookahead.type == TOK_L_BRACE) {
        parse_block();
    } else if (lookahead.type == TOK_KW_IF) {
        parse_if_statement();
    } else {
        syntax_error("a block or 'if' after 'else'");
    }
}

static void parse_statement(void) {
    switch (lookahead.type) {
        case TOK_DATA_TYPE:
            parse_declaration();
            return;
        case TOK_L_BRACE:
            parse_block();
            return;
        case TOK_KW_IF:
            parse_if_statement();
            return;
        case TOK_KW_WHILE:
            advance();
            if (!expect(TOK_L_PAREN, "'(' after 'while'")) return;
            parse_expression();
            if (!expect(TOK_R_PAREN, "')' after the condition")) return;
            parse_block();
            return;
        case TOK_KW_FOR:
            advance();
            if (!expect(TOK_L_PAREN, "'(' after 'for'")) return;
            parse_optional_expression();
            if (!expect(TOK_SEMICOLON, "first ';' in the for header")) return;
            parse_optional_expression();
            if (!expect(TOK_SEMICOLON, "second ';' in the for header")) return;
            parse_optional_expression();
            if (!expect(TOK_R_PAREN, "')' after the for header")) return;
            parse_block();
            return;
        case TOK_KW_RETURN:
            advance();
            parse_optional_expression();
            if (!failed) expect(TOK_SEMICOLON, "';' after return");
            return;
        case TOK_KW_PRINT:
            advance();
            if (!expect(TOK_L_PAREN, "'(' after 'print'")) return;
            parse_expression_list();
            if (!expect(TOK_R_PAREN, "')' after print arguments")) return;
            expect(TOK_SEMICOLON, "';' after print");
            return;
        case TOK_KW_FREE:
            advance();
            if (!expect(TOK_L_PAREN, "'(' after 'free'")) return;
            parse_expression();
            if (!expect(TOK_R_PAREN, "')' after free argument")) return;
            expect(TOK_SEMICOLON, "';' after free");
            return;
        case TOK_SEMICOLON:
            advance();
            return;
        default:
            if (is_expression_start()) {
                parse_expression();
                if (!failed) expect(TOK_SEMICOLON, "';' after the expression");
                return;
            }
            syntax_error("a statement");
    }
}

static void parse_primary_expression(void) {
    switch (lookahead.type) {
        case TOK_IDENTIFIER:
        case TOK_INTEGER_LITERAL:
        case TOK_FLOAT_LITERAL:
        case TOK_CHAR_LITERAL:
        case TOK_STRING_LITERAL:
        case TOK_BOOLEAN_LITERAL:
        case TOK_KW_NULL:
            advance();
            break;
        case TOK_L_PAREN:
            advance();
            parse_expression();
            if (!expect(TOK_R_PAREN, "')' after the parenthesized expression")) return;
            break;
        case TOK_KW_LAMB:
            advance();
            if (!expect(TOK_L_PAREN, "'(' after 'lamb'")) return;
            parse_parameter_list();
            if (!expect(TOK_R_PAREN, "')' after lambda parameters")) return;
            parse_block();
            break;
        case TOK_KW_MALLOC:
            advance();
            if (!expect(TOK_L_PAREN, "'(' after 'malloc'")) return;
            parse_expression();
            if (!expect(TOK_R_PAREN, "')' after malloc argument")) return;
            break;
        default:
            syntax_error("an expression operand");
            return;
    }

    if (failed) return;
    while (!failed) {
        if (accept(TOK_L_BRACKET)) {
            parse_expression();
            if (!expect(TOK_R_BRACKET, "']' after the index")) return;
        } else if (accept(TOK_L_PAREN)) {
            parse_expression_list();
            if (!expect(TOK_R_PAREN, "')' after call arguments")) return;
        } else {
            break;
        }
    }
}

static void parse_unary_expression(void) {
    if (lookahead.type == TOK_NOT_OP ||
        lookahead.type == TOK_ADDRESS_OP || lookahead.type == TOK_ASTERISK ||
        lookahead.type == TOK_ADD_OP) {
        advance();
        parse_unary_expression();
        return;
    }
    parse_primary_expression();
}

static void parse_multiplicative_expression(void) {
    parse_unary_expression();
    while (!failed && (lookahead.type == TOK_MUL_OP || lookahead.type == TOK_ASTERISK)) {
        advance();
        parse_unary_expression();
    }
}

static void parse_additive_expression(void) {
    parse_multiplicative_expression();
    while (!failed && lookahead.type == TOK_ADD_OP) {
        advance();
        parse_multiplicative_expression();
    }
}

static void parse_relational_expression(void) {
    parse_additive_expression();
    while (!failed && (lookahead.type == TOK_LT_OP ||
                      lookahead.type == TOK_LE_OP ||
                      lookahead.type == TOK_GT_OP ||
                      lookahead.type == TOK_GE_OP)) {
        advance();
        parse_additive_expression();
    }
}

static void parse_equality_expression(void) {
    parse_relational_expression();
    while (!failed && (lookahead.type == TOK_EQ_OP ||
                      lookahead.type == TOK_NE_OP)) {
        advance();
        parse_relational_expression();
    }
}

static void parse_logical_and_expression(void) {
    parse_equality_expression();
    while (!failed && lookahead.type == TOK_AND_OP) {
        advance();
        parse_equality_expression();
    }
}

static void parse_logical_or_expression(void) {
    parse_logical_and_expression();
    while (!failed && lookahead.type == TOK_OR_OP) {
        advance();
        parse_logical_and_expression();
    }
}

static void parse_assignment_expression(void) {
    parse_logical_or_expression();
    if (!failed && accept(TOK_ASSIGNMENT_OP)) parse_assignment_expression();
}

static void parse_expression(void) {
    parse_assignment_expression();
}

static void parse_program(void) {
    advance();
    while (!failed && lookahead.type == TOK_DATA_TYPE) parse_declaration();
    if (!failed) expect(TOK_EOF, "a top-level declaration or end of file");
}

int main(int argc, char **argv) {
    FILE *input = stdin;
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [source.clamb]\n", argv[0]);
        return 2;
    }
    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
            return 1;
        }
    }

    clamb_lexer_init(input);
    parse_program();
    if (!failed && clamb_lexer_error_count() != 0) {
        fprintf(stderr, "Parse rejected: lexer reported %d lexical error(s)\n",
                clamb_lexer_error_count());
        failed = 1;
    }

    if (input != stdin) fclose(input);
    if (failed) return 1;
    puts("Parse successful.");
    return 0;
}




