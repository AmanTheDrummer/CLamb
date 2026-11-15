#include "clamb_semantic.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SemScope {
    int id;
    int isolated;
    SemScope *parent;
    SemSymbol *symbols;
};

static SemType primitive_types[] = {
    { SEM_TYPE_ERROR, 0, NULL, NULL },
    { SEM_TYPE_VOID, 0, NULL, NULL },
    { SEM_TYPE_AUTO, 0, NULL, NULL },
    { SEM_TYPE_INT, 0, NULL, NULL },
    { SEM_TYPE_FLOAT, 0, NULL, NULL },
    { SEM_TYPE_CHAR, 0, NULL, NULL },
    { SEM_TYPE_STRING, 0, NULL, NULL },
    { SEM_TYPE_BOOL, 0, NULL, NULL },
    { SEM_TYPE_NULL, 0, NULL, NULL }
};

static SemScope *global_scope;
static SemScope *current_scope;
static int next_scope_id;
static int semantic_errors;
static char **semantic_messages;
static size_t semantic_message_count;

void sem_initialize(void) {
    global_scope = calloc(1, sizeof(*global_scope));
    if (!global_scope) {
        fputs("fatal: unable to allocate global scope\n", stderr);
        exit(2);
    }
    global_scope->id = 0;
    current_scope = global_scope;
    next_scope_id = 1;
    semantic_errors = 0;
    semantic_messages = NULL;
    semantic_message_count = 0;
}

SemType *sem_type_from_name(const char *name) {
    if (strcmp(name, "void") == 0) return &primitive_types[SEM_TYPE_VOID];
    if (strcmp(name, "auto") == 0) return &primitive_types[SEM_TYPE_AUTO];
    if (strcmp(name, "int") == 0) return &primitive_types[SEM_TYPE_INT];
    if (strcmp(name, "float") == 0) return &primitive_types[SEM_TYPE_FLOAT];
    if (strcmp(name, "char") == 0) return &primitive_types[SEM_TYPE_CHAR];
    if (strcmp(name, "string") == 0) return &primitive_types[SEM_TYPE_STRING];
    if (strcmp(name, "bool") == 0) return &primitive_types[SEM_TYPE_BOOL];
    if (strcmp(name, "null") == 0) return &primitive_types[SEM_TYPE_NULL];
    return &primitive_types[SEM_TYPE_ERROR];
}

SemType *sem_function_type(SemType **params, int param_count, SemType *return_type) {
    SemType *type = calloc(1, sizeof(*type));
    if (!type) {
        fputs("fatal: unable to allocate function type\n", stderr);
        exit(2);
    }
    type->kind = SEM_TYPE_FUNCTION;
    type->param_count = param_count;
    type->return_type = return_type;
    if (param_count > 0) {
        type->params = malloc((size_t)param_count * sizeof(*type->params));
        if (!type->params) {
            fputs("fatal: unable to allocate function parameters\n", stderr);
            exit(2);
        }
        memcpy(type->params, params, (size_t)param_count * sizeof(*type->params));
    }
    return type;
}

int sem_type_is_error(const SemType *type) {
    return !type || type->kind == SEM_TYPE_ERROR;
}

int sem_type_equal(const SemType *left, const SemType *right) {
    if (sem_type_is_error(left) || sem_type_is_error(right)) return 0;
    if (left->kind != right->kind) return 0;
    if (left->kind != SEM_TYPE_FUNCTION) return 1;
    if (left->param_count != right->param_count ||
        !sem_type_equal(left->return_type, right->return_type)) return 0;
    for (int i = 0; i < left->param_count; i++) {
        if (!sem_type_equal(left->params[i], right->params[i])) return 0;
    }
    return 1;
}

int sem_type_assignable(const SemType *source, const SemType *target) {
    if (sem_type_is_error(source) || sem_type_is_error(target)) return 1;
    if (sem_type_equal(source, target)) return 1;
    if (source->kind == SEM_TYPE_INT && target->kind == SEM_TYPE_FLOAT) return 1;
    if (source->kind == SEM_TYPE_CHAR && target->kind == SEM_TYPE_STRING) return 1;
    return 0;
}

const char *sem_type_name(const SemType *type) {
    static const char *names[] = {
        "error", "void", "auto", "int", "float", "char", "string", "bool", "null", "function"
    };
    if (!type || type->kind < SEM_TYPE_ERROR || type->kind > SEM_TYPE_FUNCTION) return "unknown";
    return names[type->kind];
}

SemScope *sem_scope_enter(int isolated) {
    SemScope *scope = calloc(1, sizeof(*scope));
    if (!scope) {
        fputs("fatal: unable to allocate scope\n", stderr);
        exit(2);
    }
    scope->id = next_scope_id++;
    scope->isolated = isolated;
    scope->parent = isolated ? NULL : current_scope;
    current_scope = scope;
    return scope;
}

void sem_scope_exit(void) {
    if (current_scope && current_scope != global_scope) current_scope = current_scope->parent;
    if (!current_scope) current_scope = global_scope;
}

SemSymbol *sem_lookup(const char *name) {
    for (SemScope *scope = current_scope; scope; scope = scope->parent) {
        for (SemSymbol *symbol = scope->symbols; symbol; symbol = symbol->next) {
            if (strcmp(symbol->name, name) == 0) return symbol;
        }
    }
    if (current_scope != global_scope) {
        for (SemSymbol *symbol = global_scope->symbols; symbol; symbol = symbol->next) {
            if (strcmp(symbol->name, name) == 0) return symbol;
        }
    }
    return NULL;
}

void sem_error_at(int line, int column, const char *format, ...) {
    semantic_errors++;
    char detail[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(detail, sizeof(detail), format, args);
    va_end(args);
    int length = snprintf(NULL, 0, "[Semantic Error] at line %d, column %d: %s\n",
                          line, column, detail);
    char *message = malloc((size_t)length + 1);
    if (!message) {
        fputs("fatal: unable to allocate diagnostic\n", stderr);
        exit(2);
    }
    snprintf(message, (size_t)length + 1, "[Semantic Error] at line %d, column %d: %s\n",
             line, column, detail);
    char **messages = realloc(semantic_messages,
                              (semantic_message_count + 1) * sizeof(*semantic_messages));
    if (!messages) {
        fputs("fatal: unable to allocate diagnostic list\n", stderr);
        exit(2);
    }
    semantic_messages = messages;
    semantic_messages[semantic_message_count++] = message;
}

int sem_error_count(void) {
    return semantic_errors;
}

void sem_finish(int parse_succeeded) {
    if (parse_succeeded) {
        for (size_t i = 0; i < semantic_message_count; i++)
            fputs(semantic_messages[i], stderr);
    }
}

SemSymbol *sem_declare(const char *name, SemSymbolKind kind, SemType *type,
                       int line, int column) {
    for (SemSymbol *existing = current_scope->symbols; existing; existing = existing->next) {
        if (strcmp(existing->name, name) == 0) {
            sem_error_at(line, column, "duplicate declaration of '%s' in this scope", name);
            return NULL;
        }
    }
    SemSymbol *symbol = calloc(1, sizeof(*symbol));
    if (!symbol) {
        fputs("fatal: unable to allocate symbol\n", stderr);
        exit(2);
    }
    symbol->name = malloc(strlen(name) + 1);
    if (!symbol->name) {
        fputs("fatal: unable to allocate symbol name\n", stderr);
        exit(2);
    }
    strcpy(symbol->name, name);
    symbol->kind = kind;
    symbol->type = type;
    symbol->scope_id = current_scope->id;
    symbol->line = line;
    symbol->column = column;
    symbol->next = current_scope->symbols;
    current_scope->symbols = symbol;
    return symbol;
}

int sem_current_scope_id(void) {
    return current_scope ? current_scope->id : 0;
}

static int is_numeric(const SemType *type) {
    return type && (type->kind == SEM_TYPE_INT || type->kind == SEM_TYPE_FLOAT);
}

static SemType *promote_numeric(const SemType *left, const SemType *right) {
    if (left->kind == SEM_TYPE_FLOAT || right->kind == SEM_TYPE_FLOAT)
        return &primitive_types[SEM_TYPE_FLOAT];
    return &primitive_types[SEM_TYPE_INT];
}

SemType *sem_check_binary(Token op, SemType *left, SemType *right,
                          int line, int column) {
    if (sem_type_is_error(left) || sem_type_is_error(right))
        return &primitive_types[SEM_TYPE_ERROR];
    if (op.type == TOK_AND_OP || op.type == TOK_OR_OP) {
        if (left->kind == SEM_TYPE_BOOL && right->kind == SEM_TYPE_BOOL)
            return &primitive_types[SEM_TYPE_BOOL];
    } else if (op.type == TOK_EQ_OP || op.type == TOK_NE_OP) {
        if (sem_type_assignable(left, right) || sem_type_assignable(right, left))
            return &primitive_types[SEM_TYPE_BOOL];
    } else if (op.type == TOK_LT_OP || op.type == TOK_LE_OP || op.type == TOK_GT_OP || op.type == TOK_GE_OP) {
        if (is_numeric(left) && is_numeric(right))
            return &primitive_types[SEM_TYPE_BOOL];
    } else if (op.type == TOK_ADD_OP && strcmp(op.lexeme, "+") == 0) {
        if (is_numeric(left) && is_numeric(right)) return promote_numeric(left, right);
        if ((left->kind == SEM_TYPE_CHAR || left->kind == SEM_TYPE_STRING) &&
            (right->kind == SEM_TYPE_CHAR || right->kind == SEM_TYPE_STRING))
            return &primitive_types[SEM_TYPE_STRING];
    } else if (op.type == TOK_ADD_OP && strcmp(op.lexeme, "-") == 0) {
        if (is_numeric(left) && is_numeric(right)) return promote_numeric(left, right);
    } else if (op.type == TOK_MUL_OP && strcmp(op.lexeme, "%") == 0) {
        if (left->kind == SEM_TYPE_INT && right->kind == SEM_TYPE_INT)
            return &primitive_types[SEM_TYPE_INT];
    } else if ((op.type == TOK_MUL_OP || op.type == TOK_ASTERISK) && is_numeric(left) && is_numeric(right)) {
        return promote_numeric(left, right);
    }
    sem_error_at(line, column, "operator '%s' is not defined for %s and %s",
                 op.lexeme,
                 sem_type_name(left), sem_type_name(right));
    return &primitive_types[SEM_TYPE_ERROR];
}

SemType *sem_check_unary(Token op, SemType *operand, int line, int column) {
    if (sem_type_is_error(operand)) return &primitive_types[SEM_TYPE_ERROR];
    if (op.type == TOK_NOT_OP && operand->kind == SEM_TYPE_BOOL)
        return &primitive_types[SEM_TYPE_BOOL];
    if (op.type == TOK_ADD_OP && is_numeric(operand)) return operand;
    sem_error_at(line, column, "unary operator '%s' is not defined for %s",
                 op.lexeme, sem_type_name(operand));
    return &primitive_types[SEM_TYPE_ERROR];
}

SemType *sem_check_assignment(SemType *target, int is_lvalue, SemType *value,
                              int line, int column) {
    if (sem_type_is_error(target) || sem_type_is_error(value))
        return &primitive_types[SEM_TYPE_ERROR];
    if (!is_lvalue) {
        sem_error_at(line, column, "assignment target is not a variable");
        return &primitive_types[SEM_TYPE_ERROR];
    }
    if (!sem_type_assignable(value, target)) {
        sem_error_at(line, column, "cannot assign %s to %s", sem_type_name(value), sem_type_name(target));
        return &primitive_types[SEM_TYPE_ERROR];
    }
    return target;
}

SemType *sem_check_call(SemType *callee, SemType **args, int arg_count,
                        int line, int column) {
    if (sem_type_is_error(callee)) return &primitive_types[SEM_TYPE_ERROR];
    if (callee->kind != SEM_TYPE_FUNCTION) {
        sem_error_at(line, column, "called expression is not a function");
        return &primitive_types[SEM_TYPE_ERROR];
    }
    if (arg_count != callee->param_count) {
        sem_error_at(line, column, "function expects %d argument(s), but received %d",
                     callee->param_count, arg_count);
    }
    int count = arg_count < callee->param_count ? arg_count : callee->param_count;
    for (int i = 0; i < count; i++) {
        if (!sem_type_is_error(args[i]) && !sem_type_assignable(args[i], callee->params[i])) {
            sem_error_at(line, column, "argument %d has type %s; expected %s", i + 1,
                         sem_type_name(args[i]), sem_type_name(callee->params[i]));
        }
    }
    return callee->return_type;
}

void sem_check_condition(SemType *type, const char *construct, int line, int column) {
    if (!sem_type_is_error(type) && type->kind != SEM_TYPE_BOOL)
        sem_error_at(line, column, "%s condition must have type bool, not %s",
                     construct, sem_type_name(type));
}

void sem_check_print_arg(SemType *type, int line, int column) {
    if (sem_type_is_error(type)) return;
    if (type->kind == SEM_TYPE_VOID || type->kind == SEM_TYPE_AUTO ||
        type->kind == SEM_TYPE_NULL || type->kind == SEM_TYPE_FUNCTION) {
        sem_error_at(line, column, "cannot print a value of type %s", sem_type_name(type));
    }
}
