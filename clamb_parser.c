#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clamb_lexer.h"
#include "clamb_semantic.h"

#define PARSER_MAX_PARAMS SEM_MAX_PARAMS

typedef struct {
    SemType *type;
    int is_lvalue;
    Token token;
} ExprResult;

typedef struct {
    char name[MAX_LEXEME];
    SemType *type;
    Token token;
} Parameter;

typedef struct {
    Parameter items[PARSER_MAX_PARAMS];
    int count;
} ParameterList;

typedef struct {
    SemType *expected_type;
    int infer;
    int saw_return;
    SemType *inferred_type;
} ReturnContext;

static Token lookahead;
static Token lookahead2;
static int failed;
static int semantic_enabled = 1;
static ExprResult expression;
static ReturnContext *return_context;

static SemType *type_error(void) { return sem_type_from_name("<error>"); }
static SemType *type_void(void) { return sem_type_from_name("void"); }
static SemType *type_auto(void) { return sem_type_from_name("auto"); }

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
        fprintf(stderr, "[Syntax Error] at line %d, column %d: expected %s but found end of file\n",
                lookahead.line, lookahead.column, expected);
    } else {
        fprintf(stderr, "[Syntax Error] at line %d, column %d: expected %s but found %s ('%s')\n",
                lookahead.line, lookahead.column, expected,
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

static void parse_decl(void);
static void parse_decl_tail(SemType *declared_type, Token name);
static void parse_statement(void);
static void parse_var_decl(void);
static void parse_if(void);
static void parse_else_opt(void);
static void parse_while(void);
static void parse_for(void);
static void parse_return(void);
static void parse_print(void);
static SemType *parse_expr_opt(void);
static void parse_expr(void);
static void parse_assign_expr(void);
static void parse_or_expr(void);
static void parse_and_expr(void);
static void parse_eq_expr(void);
static void parse_relational_expr(void);
static void parse_additive_expr(void);
static void parse_mul_expr(void);
static void parse_unary_expr(void);
static void parse_postfix_expr(void);
static void parse_primary_expr(void);
static ParameterList parse_param_list(void);

static void check_initializer(SemType *target, SemType *value, Token at) {
    if (!semantic_enabled || sem_type_is_error(value)) return;
    if (target->kind == SEM_TYPE_AUTO) {
        if (value->kind == SEM_TYPE_VOID || value->kind == SEM_TYPE_NULL) {
            sem_error_at(at.line, at.column,
                         "auto initializer must have a non-void, non-null value");
        }
        return;
    }
    if (!sem_type_assignable(value, target)) {
        sem_error_at(at.line, at.column, "cannot initialize %s with %s",
                     sem_type_name(target), sem_type_name(value));
    }
}

static void parse_variable(SemType *declared_type, Token name) {
    int is_auto = declared_type->kind == SEM_TYPE_AUTO;
    SemSymbol *symbol = NULL;
    if (semantic_enabled && !is_auto) {
        if (declared_type->kind == SEM_TYPE_VOID) {
            sem_error_at(name.line, name.column, "variable '%s' cannot have type void", name.lexeme);
            sem_declare(name.lexeme, SEM_SYMBOL_VARIABLE, type_error(), name.line, name.column);
        } else {
            symbol = sem_declare(name.lexeme, SEM_SYMBOL_VARIABLE, declared_type,
                                 name.line, name.column);
        }
    }

    Token initializer_token = lookahead;
    SemType *initializer_type = type_void();
    if (accept(TOK_ASSIGNMENT_OP)) {
        initializer_token = lookahead;
        parse_expr();
        initializer_type = expression.type;
        if (declared_type->kind != SEM_TYPE_VOID)
            check_initializer(declared_type, initializer_type, initializer_token);
    } else if (is_auto && semantic_enabled) {
        sem_error_at(name.line, name.column, "auto variable '%s' requires an initializer", name.lexeme);
        initializer_type = type_error();
    }

    if (semantic_enabled && is_auto) {
        SemType *inferred = initializer_type;
        if (sem_type_is_error(inferred) || inferred->kind == SEM_TYPE_VOID || inferred->kind == SEM_TYPE_NULL) {
            inferred = type_error();
        }
        sem_declare(name.lexeme, SEM_SYMBOL_VARIABLE, inferred, name.line, name.column);
    }
    (void)symbol;
    (void)initializer_token;
}

// <decl> ::= data_type identifier <decl_tail> <decl> | epsilon
static void parse_decl(void) {
    while (!failed && lookahead.type == TOK_DATA_TYPE) {
        Token type_token = lookahead;
        SemType *declared_type = sem_type_from_name(type_token.lexeme);
        advance();
        Token name = lookahead;
        if (!expect(TOK_IDENTIFIER, "an identifier")) return;
        parse_decl_tail(declared_type, name);
    }
}

static ParameterList parse_param_list(void) {
    ParameterList params = {0};
    while (!failed && lookahead.type == TOK_DATA_TYPE) {
        Token type_token = lookahead;
        SemType *type = sem_type_from_name(type_token.lexeme);
        advance();
        Token name = lookahead;
        if (!expect(TOK_IDENTIFIER, "a parameter name")) break;
        if (semantic_enabled && (type->kind == SEM_TYPE_VOID || type->kind == SEM_TYPE_AUTO)) {
            sem_error_at(type_token.line, type_token.column,
                         "parameter '%s' cannot have type %s", name.lexeme, sem_type_name(type));
            type = type_error();
        }
        if (params.count < PARSER_MAX_PARAMS) {
            Parameter *param = &params.items[params.count++];
            strncpy(param->name, name.lexeme, MAX_LEXEME - 1);
            param->name[MAX_LEXEME - 1] = '\0';
            param->type = type;
            param->token = name;
        } else if (semantic_enabled) {
            sem_error_at(name.line, name.column, "parameter limit (%d) exceeded", PARSER_MAX_PARAMS);
        }
        if (!accept(TOK_COMMA)) break;
        if (lookahead.type != TOK_DATA_TYPE) {
            syntax_error("a data type after ','");
            break;
        }
    }
    return params;
}

static void insert_parameters(ParameterList *params) {
    if (!semantic_enabled) return;
    for (int i = 0; i < params->count; i++) {
        Parameter *param = &params->items[i];
        sem_declare(param->name, SEM_SYMBOL_PARAMETER, param->type,
                    param->token.line, param->token.column);
    }
}

// <decl_tail> ::= <var_init> semicolon | l_paren <param_list> r_paren l_brace <statement> r_brace
static void parse_decl_tail(SemType *declared_type, Token name) {
    if (accept(TOK_L_PAREN)) {
        ParameterList params = parse_param_list();
        if (!expect(TOK_R_PAREN, "')' after the parameter list")) return;
        SemType *param_types[PARSER_MAX_PARAMS];
        for (int i = 0; i < params.count; i++) param_types[i] = params.items[i].type;
        SemType *body_return_type = declared_type;
        SemType *signature = sem_function_type(param_types, params.count, declared_type);
        if (semantic_enabled) {
            if (declared_type->kind == SEM_TYPE_AUTO) {
                sem_error_at(name.line, name.column, "function '%s' cannot have return type auto", name.lexeme);
                body_return_type = type_error();
                signature = sem_function_type(param_types, params.count, body_return_type);
            }
            sem_declare(name.lexeme, SEM_SYMBOL_FUNCTION, signature, name.line, name.column);
        }
        if (!expect(TOK_L_BRACE, "'{' to begin the function body")) return;
        sem_scope_enter(0);
        insert_parameters(&params);
        ReturnContext context = {body_return_type, 0, 0, NULL};
        ReturnContext *previous_context = return_context;
        return_context = &context;
        parse_statement();
        if (!expect(TOK_R_BRACE, "'}' to close the function body")) {
            return_context = previous_context;
            sem_scope_exit();
            return;
        }
        return_context = previous_context;
        sem_scope_exit();
        return;
    }

    parse_variable(declared_type, name);
    if (!failed) expect(TOK_SEMICOLON, "';' after the declaration");
}

static void parse_var_decl(void) {
    Token type_token = lookahead;
    SemType *type = sem_type_from_name(type_token.lexeme);
    advance();
    Token name = lookahead;
    if (!expect(TOK_IDENTIFIER, "a variable name")) return;
    parse_variable(type, name);
    if (!failed) expect(TOK_SEMICOLON, "';' after the declaration");
}

static void parse_scoped_body(const char *closing_description) {
    sem_scope_enter(0);
    parse_statement();
    if (!expect(TOK_R_BRACE, closing_description)) {
        sem_scope_exit();
        return;
    }
    sem_scope_exit();
}

static void parse_statement(void) {
    while (!failed && lookahead.type != TOK_R_BRACE && lookahead.type != TOK_EOF) {
        switch (lookahead.type) {
            case TOK_DATA_TYPE:
                parse_var_decl();
                break;
            case TOK_L_BRACE:
                advance();
                parse_scoped_body("'}' to close the block");
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
            default:
                if (is_expression_start()) {
                    parse_expr();
                    if (!failed) expect(TOK_SEMICOLON, "';' after the expression");
                } else {
                    syntax_error("a statement or '}'");
                }
                break;
        }
    }
}

static void parse_if(void) {
    Token keyword = lookahead;
    advance();
    if (!expect(TOK_L_PAREN, "'(' after 'if'")) return;
    parse_expr();
    if (semantic_enabled) sem_check_condition(expression.type, "if", keyword.line, keyword.column);
    if (!expect(TOK_R_PAREN, "')' after the condition")) return;
    if (!expect(TOK_L_BRACE, "'{' to begin the if body")) return;
    parse_scoped_body("'}' to close the if body");
    if (!failed) parse_else_opt();
}

static void parse_else_opt(void) {
    if (lookahead.type != TOK_KW_ELSE) return;
    if (lookahead2.type == TOK_L_BRACE) {
        advance();
        advance();
        parse_scoped_body("'}' to close the else body");
    } else if (lookahead2.type == TOK_KW_IF) {
        advance();
        parse_if();
    } else {
        advance();
        syntax_error("'{' or 'if' after 'else'");
    }
}

static void parse_while(void) {
    Token keyword = lookahead;
    advance();
    if (!expect(TOK_L_PAREN, "'(' after 'while'")) return;
    parse_expr();
    if (semantic_enabled) sem_check_condition(expression.type, "while", keyword.line, keyword.column);
    if (!expect(TOK_R_PAREN, "')' after the condition")) return;
    if (!expect(TOK_L_BRACE, "'{' to begin the while body")) return;
    parse_scoped_body("'}' to close the while body");
}

static void parse_for(void) {
    Token keyword = lookahead;
    advance();
    if (!expect(TOK_L_PAREN, "'(' after 'for'")) return;
    (void)parse_expr_opt();
    if (!expect(TOK_SEMICOLON, "first ';' in the for header")) return;
    int has_condition = is_expression_start();
    SemType *condition = parse_expr_opt();
    if (semantic_enabled && has_condition)
        sem_check_condition(condition, "for", keyword.line, keyword.column);
    if (!expect(TOK_SEMICOLON, "second ';' in the for header")) return;
    (void)parse_expr_opt();
    if (!expect(TOK_R_PAREN, "')' after the for header")) return;
    if (!expect(TOK_L_BRACE, "'{' to begin the for body")) return;
    parse_scoped_body("'}' to close the for body");
}

static void parse_return(void) {
    Token keyword = lookahead;
    advance();
    int has_expression = is_expression_start();
    SemType *value = parse_expr_opt();
    if (!failed) expect(TOK_SEMICOLON, "';' after return");
    if (!semantic_enabled || failed) return;
    if (!return_context) {
        sem_error_at(keyword.line, keyword.column, "return is only valid inside a function or lambda");
        return;
    }
    if (return_context->infer) {
        if (!return_context->saw_return) {
            return_context->inferred_type = value;
            return_context->saw_return = 1;
        } else if (sem_type_is_error(value) || sem_type_is_error(return_context->inferred_type)) {
            return_context->inferred_type = type_error();
        } else if (sem_type_assignable(value, return_context->inferred_type)) {
            if ((value->kind == SEM_TYPE_FLOAT && return_context->inferred_type->kind == SEM_TYPE_INT) ||
                (value->kind == SEM_TYPE_STRING && return_context->inferred_type->kind == SEM_TYPE_CHAR))
                return_context->inferred_type = value;
        } else if (sem_type_assignable(return_context->inferred_type, value)) {
            if ((value->kind == SEM_TYPE_FLOAT && return_context->inferred_type->kind == SEM_TYPE_INT) ||
                (value->kind == SEM_TYPE_STRING && return_context->inferred_type->kind == SEM_TYPE_CHAR))
                return_context->inferred_type = value;
        } else {
            sem_error_at(keyword.line, keyword.column,
                         "lambda return type %s conflicts with earlier return type %s",
                         sem_type_name(value), sem_type_name(return_context->inferred_type));
            return_context->inferred_type = type_error();
        }
        return;
    }
    if (return_context->expected_type->kind == SEM_TYPE_VOID) {
        if (has_expression && !sem_type_is_error(value))
            sem_error_at(keyword.line, keyword.column, "void function cannot return a value");
    } else if (value->kind == SEM_TYPE_VOID && !sem_type_is_error(value)) {
        sem_error_at(keyword.line, keyword.column, "non-void function must return a value of type %s",
                     sem_type_name(return_context->expected_type));
    } else if (!sem_type_assignable(value, return_context->expected_type)) {
        sem_error_at(keyword.line, keyword.column, "cannot return %s from a function returning %s",
                     sem_type_name(value), sem_type_name(return_context->expected_type));
    }
}

static void parse_print(void) {
    Token keyword = lookahead;
    advance();
    if (!expect(TOK_L_PAREN, "'(' after 'print'")) return;
    SemType *args[PARSER_MAX_PARAMS];
    int count = 0;
    if (lookahead.type != TOK_R_PAREN) {
        do {
            parse_expr();
            if (count < PARSER_MAX_PARAMS) args[count++] = expression.type;
            else if (semantic_enabled)
                sem_error_at(keyword.line, keyword.column, "print argument limit exceeded");
        } while (!failed && accept(TOK_COMMA));
    }
    if (!expect(TOK_R_PAREN, "')' after print arguments")) return;
    if (!expect(TOK_SEMICOLON, "';' after print")) return;
    if (semantic_enabled) {
        for (int i = 0; i < count; i++)
            sem_check_print_arg(args[i], keyword.line, keyword.column);
    }
}

static SemType *parse_expr_opt(void) {
    if (!is_expression_start()) return type_void();
    parse_expr();
    return expression.type;
}

static void parse_expr(void) {
    parse_assign_expr();
}

static void parse_assign_expr(void) {
    parse_or_expr();
    if (failed) return;
    ExprResult target = expression;
    if (lookahead.type == TOK_ASSIGNMENT_OP) {
        Token assignment = lookahead;
        advance();
        parse_assign_expr();
        if (!failed && semantic_enabled) {
            expression.type = sem_check_assignment(target.type, target.is_lvalue,
                                                   expression.type, assignment.line,
                                                   assignment.column);
            expression.is_lvalue = 0;
            expression.token = assignment;
        }
    }
}

static void parse_or_expr(void) {
    parse_and_expr();
    while (!failed && lookahead.type == TOK_OR_OP) {
        Token op = lookahead;
        SemType *left = expression.type;
        advance();
        parse_and_expr();
        if (!failed && semantic_enabled) {
            expression.type = sem_check_binary(op, left, expression.type, op.line, op.column);
            expression.is_lvalue = 0;
            expression.token = op;
        }
    }
}

static void parse_and_expr(void) {
    parse_eq_expr();
    while (!failed && lookahead.type == TOK_AND_OP) {
        Token op = lookahead;
        SemType *left = expression.type;
        advance();
        parse_eq_expr();
        if (!failed && semantic_enabled) {
            expression.type = sem_check_binary(op, left, expression.type, op.line, op.column);
            expression.is_lvalue = 0;
            expression.token = op;
        }
    }
}

static void parse_eq_expr(void) {
    parse_relational_expr();
    while (!failed && (lookahead.type == TOK_EQ_OP || lookahead.type == TOK_NE_OP)) {
        Token op = lookahead;
        SemType *left = expression.type;
        advance();
        parse_relational_expr();
        if (!failed && semantic_enabled) {
            expression.type = sem_check_binary(op, left, expression.type, op.line, op.column);
            expression.is_lvalue = 0;
            expression.token = op;
        }
    }
}

static void parse_relational_expr(void) {
    parse_additive_expr();
    while (!failed && (lookahead.type == TOK_LT_OP || lookahead.type == TOK_LE_OP ||
                       lookahead.type == TOK_GT_OP || lookahead.type == TOK_GE_OP)) {
        Token op = lookahead;
        SemType *left = expression.type;
        advance();
        parse_additive_expr();
        if (!failed && semantic_enabled) {
            expression.type = sem_check_binary(op, left, expression.type, op.line, op.column);
            expression.is_lvalue = 0;
            expression.token = op;
        }
    }
}

static void parse_additive_expr(void) {
    parse_mul_expr();
    while (!failed && lookahead.type == TOK_ADD_OP) {
        Token op = lookahead;
        SemType *left = expression.type;
        advance();
        parse_mul_expr();
        if (!failed && semantic_enabled) {
            expression.type = sem_check_binary(op, left, expression.type, op.line, op.column);
            expression.is_lvalue = 0;
            expression.token = op;
        }
    }
}

static void parse_mul_expr(void) {
    parse_unary_expr();
    while (!failed && (lookahead.type == TOK_MUL_OP || lookahead.type == TOK_ASTERISK)) {
        Token op = lookahead;
        SemType *left = expression.type;
        advance();
        parse_unary_expr();
        if (!failed && semantic_enabled) {
            expression.type = sem_check_binary(op, left, expression.type, op.line, op.column);
            expression.is_lvalue = 0;
            expression.token = op;
        }
    }
}

static void parse_unary_expr(void) {
    if (lookahead.type == TOK_NOT_OP || lookahead.type == TOK_ADD_OP) {
        Token op = lookahead;
        advance();
        parse_unary_expr();
        if (!failed && semantic_enabled) {
            expression.type = sem_check_unary(op, expression.type, op.line, op.column);
            expression.is_lvalue = 0;
            expression.token = op;
        }
        return;
    }
    parse_postfix_expr();
}

static void parse_postfix_expr(void) {
    parse_primary_expr();
    while (!failed && lookahead.type == TOK_L_PAREN) {
        Token open_paren = lookahead;
        SemType *callee = expression.type;
        advance();
        SemType *args[PARSER_MAX_PARAMS];
        int arg_count = 0;
        if (lookahead.type != TOK_R_PAREN) {
            do {
                parse_expr();
                if (arg_count < PARSER_MAX_PARAMS) args[arg_count++] = expression.type;
                else if (semantic_enabled)
                    sem_error_at(open_paren.line, open_paren.column, "call argument limit exceeded");
            } while (!failed && accept(TOK_COMMA));
        }
        if (!expect(TOK_R_PAREN, "')' after call arguments")) return;
        if (!semantic_enabled) {
            expression.type = type_error();
        } else {
            expression.type = sem_check_call(callee, args, arg_count,
                                             open_paren.line, open_paren.column);
        }
        expression.is_lvalue = 0;
        expression.token = open_paren;
    }
}

static SemType *literal_type(Token token) {
    switch (token.type) {
        case TOK_INTEGER_LITERAL: return sem_type_from_name("int");
        case TOK_FLOAT_LITERAL: return sem_type_from_name("float");
        case TOK_CHAR_LITERAL: return sem_type_from_name("char");
        case TOK_STRING_LITERAL: return sem_type_from_name("string");
        case TOK_BOOLEAN_LITERAL: return sem_type_from_name("bool");
        case TOK_KW_NULL: return sem_type_from_name("null");
        default: return type_error();
    }
}

static void parse_primary_expr(void) {
    Token token = lookahead;
    switch (lookahead.type) {
        case TOK_IDENTIFIER: {
            advance();
            expression.token = token;
            expression.is_lvalue = 0;
            if (!semantic_enabled) {
                expression.type = type_error();
                return;
            }
            SemSymbol *symbol = sem_lookup(token.lexeme);
            if (!symbol) {
                sem_error_at(token.line, token.column, "undeclared identifier '%s'", token.lexeme);
                expression.type = type_error();
                return;
            }
            expression.type = symbol->type;
            expression.is_lvalue = symbol->kind == SEM_SYMBOL_VARIABLE ||
                                   symbol->kind == SEM_SYMBOL_PARAMETER;
            return;
        }
        case TOK_INTEGER_LITERAL:
        case TOK_FLOAT_LITERAL:
        case TOK_CHAR_LITERAL:
        case TOK_STRING_LITERAL:
        case TOK_BOOLEAN_LITERAL:
        case TOK_KW_NULL:
            advance();
            expression.type = literal_type(token);
            expression.is_lvalue = 0;
            expression.token = token;
            return;
        case TOK_L_PAREN:
            advance();
            parse_expr();
            if (!expect(TOK_R_PAREN, "')' after the parenthesized expression")) return;
            expression.is_lvalue = 0;
            expression.token = token;
            return;
        case TOK_KW_LAMB: {
            advance();
            if (!expect(TOK_L_PAREN, "'(' after 'lamb'")) return;
            ParameterList params = parse_param_list();
            if (!expect(TOK_R_PAREN, "')' after lambda parameters")) return;
            if (!expect(TOK_L_BRACE, "'{' to begin the lambda body")) return;
            sem_scope_enter(1);
            insert_parameters(&params);
            ReturnContext context = {type_auto(), 1, 0, NULL};
            ReturnContext *previous_context = return_context;
            return_context = &context;
            parse_statement();
            if (!expect(TOK_R_BRACE, "'}' to close the lambda body")) {
                return_context = previous_context;
                sem_scope_exit();
                return;
            }
            return_context = previous_context;
            sem_scope_exit();
            SemType *return_type = context.saw_return ? context.inferred_type : type_void();
            SemType *param_types[PARSER_MAX_PARAMS];
            for (int i = 0; i < params.count; i++) param_types[i] = params.items[i].type;
            expression.type = sem_function_type(param_types, params.count, return_type);
            expression.is_lvalue = 0;
            expression.token = token;
            return;
        }
        default:
            syntax_error("an expression operand");
            return;
    }
}

static int parse_program_start(void) {
    parse_decl();
    if (!failed) expect(TOK_EOF, "a top-level declaration or end of file");
    return !failed;
}

int main(int argc, char **argv) {
    FILE *input = stdin;
    int arg_index = 1;
    if (arg_index < argc && strcmp(argv[arg_index], "--syntax-only") == 0) {
        semantic_enabled = 0;
        arg_index++;
    }
    if (argc - arg_index > 1) {
        fprintf(stderr, "Usage: %s [--syntax-only] [source.clamb]\n", argv[0]);
        return 2;
    }
    if (arg_index < argc) {
        input = fopen(argv[arg_index], "r");
        if (!input) {
            fprintf(stderr, "Error: cannot open file '%s'\n", argv[arg_index]);
            return 1;
        }
    }

    sem_initialize();
    clamb_lexer_init(input);
    initialize_lookahead();
    parse_program_start();
    if (clamb_lexer_error_count() != 0) failed = 1;
    sem_finish(!failed);

    if (input != stdin) fclose(input);
    if (failed) return 1;
    if (semantic_enabled && sem_error_count() != 0) return 1;
    puts(semantic_enabled ? "Semantic analysis successful." : "Parse successful.");
    return 0;
}
