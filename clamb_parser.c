#include <stdio.h>
#include "clamb_lexer.h"

static Token lookahead;
static Token lookahead2;
static int failed;

static void advance(void) {
    lookahead = lookahead2;
    lookahead2 = clamb_lexer_next_token();
}

static void initialize_lookahead(void) {
    lookahead = clamb_lexer_next_token();
    lookahead2 = clamb_lexer_next_token();
}

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

static int accept(TokenType type) {
    if (lookahead.type != type) return 0;
    advance();
    return 1;
}

static int expect(TokenType type, const char *description) {
    if (accept(type)) return 1;
    syntax_error(description);
    return 0;
}

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
        case TOK_L_PAREN:
        case TOK_ADD_OP:
        case TOK_NOT_OP:
            return 1;
        default:
            return 0;
    }
}

static void parse_program_start(void);
static void parse_decl(void);
static void parse_decl_tail(void);
static void parse_var_init(void);
static void parse_param_list(void);
static void parse_param_tail(void);
static void parse_statement(void);
static void parse_var_decl(void);
static void parse_expr_stmt(void);
static void parse_if(void);
static void parse_else_opt(void);
static void parse_while(void);
static void parse_for(void);
static void parse_return(void);
static void parse_print(void);
static void parse_expr_opt(void);
static void parse_expr(void);
static void parse_assign_expr(void);
static void parse_assign_tail(void);
static void parse_or_expr(void);
static void parse_or_tail(void);
static void parse_and_expr(void);
static void parse_and_tail(void);
static void parse_eq_expr(void);
static void parse_eq_tail(void);
static void parse_relational_expr(void);
static void parse_relational_tail(void);
static void parse_additive_expr(void);
static void parse_additive_tail(void);
static void parse_mul_expr(void);
static void parse_mul_tail(void);
static void parse_unary_expr(void);
static void parse_postfix_expr(void);
static void parse_postfix_tail(void);
static void parse_primary_expr(void);
static void parse_expr_list(void);
static void parse_nonempty_expr_list(void);
static void parse_expr_list_tail(void);

// <program_start> ::= <decl>
static void parse_program_start(void) {
    parse_decl();
}

// <decl> ::= data_type identifier <decl_tail> <decl> | ε
static void parse_decl(void) {
    if (lookahead.type != TOK_DATA_TYPE) return;

    advance();
    if (!expect(TOK_IDENTIFIER, "an identifier")) return;
    parse_decl_tail();
    if (!failed) parse_decl();
}

// <decl_tail> ::= <var_init> semicolon
//               | l_paren <param_list> r_paren l_brace <statement> r_brace
static void parse_decl_tail(void) {
    if (accept(TOK_L_PAREN)) {
        parse_param_list();
        if (!expect(TOK_R_PAREN, "')' after the parameter list")) return;
        if (!expect(TOK_L_BRACE, "'{' to begin the function body")) return;
        parse_statement();
        if (!failed) expect(TOK_R_BRACE, "'}' to close the function body");
        return;
    }

    parse_var_init();
    if (!failed) expect(TOK_SEMICOLON, "';' after the declaration");
}

// <var_init> ::= assignment_op <expr>
//               | ε
static void parse_var_init(void) {
    if (accept(TOK_ASSIGNMENT_OP)) {
        parse_expr();
    }
}

// <param_list> ::= data_type identifier <param_tail> | ε
static void parse_param_list(void) {
    if (lookahead.type != TOK_DATA_TYPE) return;

    advance();
    if (!expect(TOK_IDENTIFIER, "a parameter name")) return;
    parse_param_tail();
}

// <param_tail> ::= comma data_type identifier <param_tail> | ε
static void parse_param_tail(void) {
    if (!accept(TOK_COMMA)) return;
    if (!expect(TOK_DATA_TYPE, "a data type after ','")) return;
    if (!expect(TOK_IDENTIFIER, "a parameter name")) return;
    parse_param_tail();
}

// <statement> ::= <var_decl> <statement> | l_brace <statement> r_brace <statement>
//               | <expr_stmt> <statement> | <if> <statement>
//               | <while> <statement> | <for> <statement>
//               | <return> <statement> | <print> <statement>
//               | ε
static void parse_statement(void) {
    switch (lookahead.type) {
        case TOK_DATA_TYPE:
            parse_var_decl();
            break;
        case TOK_L_BRACE:
            advance();
            parse_statement();
            if (!failed) expect(TOK_R_BRACE, "'}' to close the block");
            break;
        case TOK_KW_IF:
            parse_if();
            break;
        case TOK_KW_WHILE:
            parse_while();
            break;
        case TOK_KW_FOR:
            parse_for();
            break;
        case TOK_KW_RETURN:
            parse_return();
            break;
        case TOK_KW_PRINT:
            parse_print();
            break;
        case TOK_SEMICOLON:
            advance();
            break;
        case TOK_R_BRACE:
        case TOK_EOF:
            return;
        default:
            if (is_expression_start()) {
                parse_expr_stmt();
            } else {
                syntax_error("a statement or '}'");
                return;
            }
    }

    if (!failed) parse_statement();
}

// <var_decl> ::= data_type identifier <var_init> semicolon
static void parse_var_decl(void) {
    advance();
    if (!expect(TOK_IDENTIFIER, "a variable name")) return;
    parse_var_init();
    if (!failed) expect(TOK_SEMICOLON, "';' after the declaration");
}

// <expr_stmt> ::= <expr> semicolon | semicolon
static void parse_expr_stmt(void) {
    if (accept(TOK_SEMICOLON)) return;
    parse_expr();
    if (!failed) expect(TOK_SEMICOLON, "';' after the expression");
}

// <if> ::= kw_if l_paren <expr> r_paren l_brace <statement> r_brace <else_opt>
static void parse_if(void) {
    advance();
    if (!expect(TOK_L_PAREN, "'(' after 'if'")) return;
    parse_expr();
    if (!expect(TOK_R_PAREN, "')' after the condition")) return;
    if (!expect(TOK_L_BRACE, "'{' to begin the if body")) return;
    parse_statement();
    if (!expect(TOK_R_BRACE, "'}' to close the if body")) return;
    parse_else_opt();
}

// <else_opt> ::= kw_else l_brace <statement> r_brace
//              | kw_else kw_if <if>
//              | ε
// This production selects alternatives using the pair (lookahead, lookahead2).
static void parse_else_opt(void) {
    if (lookahead.type != TOK_KW_ELSE) return;

    if (lookahead2.type == TOK_L_BRACE) {
        advance();
        advance();
        parse_statement();
        if (!failed) expect(TOK_R_BRACE, "'}' to close the else body");
    } else if (lookahead2.type == TOK_KW_IF) {
        advance();
        parse_if();
    } else {
        advance();
        syntax_error("'{' or 'if' after 'else'");
    }
}

// <while> ::= kw_while l_paren <expr> r_paren l_brace <statement> r_brace
static void parse_while(void) {
    advance();
    if (!expect(TOK_L_PAREN, "'(' after 'while'")) return;
    parse_expr();
    if (!expect(TOK_R_PAREN, "')' after the condition")) return;
    if (!expect(TOK_L_BRACE, "'{' to begin the while body")) return;
    parse_statement();
    if (!failed) expect(TOK_R_BRACE, "'}' to close the while body");
}

// <for> ::= kw_for l_paren <expr_opt> semicolon <expr_opt> semicolon
//           <expr_opt> r_paren l_brace <statement> r_brace
static void parse_for(void) {
    advance();
    if (!expect(TOK_L_PAREN, "'(' after 'for'")) return;
    parse_expr_opt();
    if (!expect(TOK_SEMICOLON, "first ';' in the for header")) return;
    parse_expr_opt();
    if (!expect(TOK_SEMICOLON, "second ';' in the for header")) return;
    parse_expr_opt();
    if (!expect(TOK_R_PAREN, "')' after the for header")) return;
    if (!expect(TOK_L_BRACE, "'{' to begin the for body")) return;
    parse_statement();
    if (!failed) expect(TOK_R_BRACE, "'}' to close the for body");
}

// <return> ::= kw_return <expr_opt> semicolon
static void parse_return(void) {
    advance();
    parse_expr_opt();
    if (!failed) expect(TOK_SEMICOLON, "';' after return");
}

// <print> ::= kw_print l_paren <expr_list> r_paren semicolon
static void parse_print(void) {
    advance();
    if (!expect(TOK_L_PAREN, "'(' after 'print'")) return;
    parse_expr_list();
    if (!expect(TOK_R_PAREN, "')' after print arguments")) return;
    expect(TOK_SEMICOLON, "';' after print");
}

// <expr_opt> ::= <expr> | ε
static void parse_expr_opt(void) {
    if (is_expression_start()) parse_expr();
}

// <expr> ::= <assign_expr>
static void parse_expr(void) {
    parse_assign_expr();
}

// <assign_expr> ::= <or_expr> <assign_tail>
static void parse_assign_expr(void) {
    parse_or_expr();
    if (!failed) parse_assign_tail();
}

// <assign_tail> ::= assignment_op <assign_expr> | ε
static void parse_assign_tail(void) {
    if (accept(TOK_ASSIGNMENT_OP)) parse_assign_expr();
}

// <or_expr> ::= <and_expr> <or_tail>
static void parse_or_expr(void) {
    parse_and_expr();
    if (!failed) parse_or_tail();
}

// <or_tail> ::= logical_or <and_expr> <or_tail> | ε
static void parse_or_tail(void) {
    if (!accept(TOK_OR_OP)) return;
    parse_and_expr();
    if (!failed) parse_or_tail();
}

// <and_expr> ::= <eq_expr> <and_tail>
static void parse_and_expr(void) {
    parse_eq_expr();
    if (!failed) parse_and_tail();
}

// <and_tail> ::= logical_and <eq_expr> <and_tail> | ε
static void parse_and_tail(void) {
    if (!accept(TOK_AND_OP)) return;
    parse_eq_expr();
    if (!failed) parse_and_tail();
}

// <eq_expr> ::= <relational_expr> <eq_tail>
static void parse_eq_expr(void) {
    parse_relational_expr();
    if (!failed) parse_eq_tail();
}

// <eq_tail> ::= relational_eq <relational_expr> <eq_tail>
//             | relational_noteq <relational_expr> <eq_tail> | ε
static void parse_eq_tail(void) {
    if (!accept(TOK_EQ_OP) && !accept(TOK_NE_OP)) return;
    parse_relational_expr();
    if (!failed) parse_eq_tail();
}

// <relational_expr> ::= <additive_expr> <relational_tail>
static void parse_relational_expr(void) {
    parse_additive_expr();
    if (!failed) parse_relational_tail();
}

// <relational_tail> ::= relational_less <additive_expr> <relational_tail>
//                     | relational_greater <additive_expr> <relational_tail>
//                     | relational_lesseq <additive_expr> <relational_tail>
//                     | relational_greatereq <additive_expr> <relational_tail>
//                     | ε
static void parse_relational_tail(void) {
    switch (lookahead.type) {
        case TOK_LT_OP:
        case TOK_LE_OP:
        case TOK_GT_OP:
        case TOK_GE_OP:
            advance();
            parse_additive_expr();
            if (!failed) parse_relational_tail();
            break;
        default:
            break;
    }
}

// <additive_expr> ::= <mul_expr> <additive_tail>
static void parse_additive_expr(void) {
    parse_mul_expr();
    if (!failed) parse_additive_tail();
}

// <additive_tail> ::= add_op <mul_expr> <additive_tail> | ε
static void parse_additive_tail(void) {
    if (!accept(TOK_ADD_OP)) return;
    parse_mul_expr();
    if (!failed) parse_additive_tail();
}

// <mul_expr> ::= <unary_expr> <mul_tail>
static void parse_mul_expr(void) {
    parse_unary_expr();
    if (!failed) parse_mul_tail();
}

// <mul_tail> ::= mul_op <unary_expr> <mul_tail>
//              | asterisk <unary_expr> <mul_tail> | ε
static void parse_mul_tail(void) {
    if (!accept(TOK_MUL_OP) && !accept(TOK_ASTERISK)) return;
    parse_unary_expr();
    if (!failed) parse_mul_tail();
}

// <unary_expr> ::= logical_not <unary_expr> | add_op <unary_expr>
//                  | <postfix_expr>
static void parse_unary_expr(void) {
    if (lookahead.type == TOK_NOT_OP ||
        lookahead.type == TOK_ADD_OP) {
        advance();
        parse_unary_expr();
    } else {
        parse_postfix_expr();
    }
}

// <postfix_expr> ::= <primary_expr> <postfix_tail>
static void parse_postfix_expr(void) {
    parse_primary_expr();
    if (!failed) parse_postfix_tail();
}

// <postfix_tail> ::= l_paren <expr_list> r_paren <postfix_tail> | ε
static void parse_postfix_tail(void) {
    if (accept(TOK_L_PAREN)) {
        parse_expr_list();
        if (!expect(TOK_R_PAREN, "')' after call arguments")) return;
        parse_postfix_tail();
    }
}

// <primary_expr> ::= identifier | integer_literal | float_literal
//                  | char_literal | string_literal | boolean_literal
//                  | kw_null | l_paren <expr> r_paren
//                  | kw_lamb l_paren <param_list> r_paren l_brace <statement> r_brace
static void parse_primary_expr(void) {
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
            parse_expr();
            if (!expect(TOK_R_PAREN, "')' after the parenthesized expression")) return;
            break;
        case TOK_KW_LAMB:
            advance();
            if (!expect(TOK_L_PAREN, "'(' after 'lamb'")) return;
            parse_param_list();
            if (!expect(TOK_R_PAREN, "')' after lambda parameters")) return;
            if (!expect(TOK_L_BRACE, "'{' to begin the lambda body")) return;
            parse_statement();
            if (!failed) expect(TOK_R_BRACE, "'}' to close the lambda body");
            break;
        default:
            syntax_error("an expression operand");
            break;
    }
}

// <expr_list> ::= <nonempty_expr_list> | ε
static void parse_expr_list(void) {
    if (lookahead.type != TOK_R_PAREN) parse_nonempty_expr_list();
}

// <nonempty_expr_list> ::= <expr> <expr_list_tail>
static void parse_nonempty_expr_list(void) {
    parse_expr();
    if (!failed) parse_expr_list_tail();
}

// <expr_list_tail> ::= comma <expr> <expr_list_tail> | ε
static void parse_expr_list_tail(void) {
    if (!accept(TOK_COMMA)) return;
    parse_expr();
    if (!failed) parse_expr_list_tail();
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
    initialize_lookahead();
    parse_program_start();
    if (!failed) expect(TOK_EOF, "a top-level declaration or end of file");
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