#ifndef CLAMB_SEMANTIC_H
#define CLAMB_SEMANTIC_H

#include "clamb_lexer.h"

#define SEM_MAX_PARAMS 64

typedef enum {
    SEM_TYPE_ERROR,
    SEM_TYPE_VOID,
    SEM_TYPE_AUTO,
    SEM_TYPE_INT,
    SEM_TYPE_FLOAT,
    SEM_TYPE_CHAR,
    SEM_TYPE_STRING,
    SEM_TYPE_BOOL,
    SEM_TYPE_NULL,
    SEM_TYPE_FUNCTION
} SemTypeKind;

typedef struct SemType SemType;
typedef struct SemSymbol SemSymbol;
typedef struct SemScope SemScope;

typedef enum {
    SEM_SYMBOL_VARIABLE,
    SEM_SYMBOL_PARAMETER,
    SEM_SYMBOL_FUNCTION
} SemSymbolKind;

struct SemType {
    SemTypeKind kind;
    int param_count;
    SemType **params;
    SemType *return_type;
};

struct SemSymbol {
    char *name;
    SemSymbolKind kind;
    SemType *type;
    int scope_id;
    int line;
    int column;
    SemSymbol *next;
};

void sem_initialize(void);
SemType *sem_type_from_name(const char *name);
SemType *sem_function_type(SemType **params, int param_count, SemType *return_type);
int sem_type_equal(const SemType *left, const SemType *right);
int sem_type_assignable(const SemType *source, const SemType *target);
int sem_type_is_error(const SemType *type);
const char *sem_type_name(const SemType *type);

SemScope *sem_scope_enter(int isolated);
void sem_scope_exit(void);
SemSymbol *sem_lookup(const char *name);
SemSymbol *sem_declare(const char *name, SemSymbolKind kind, SemType *type,
                       int line, int column);
int sem_current_scope_id(void);
void sem_error_at(int line, int column, const char *format, ...);
int sem_error_count(void);
void sem_finish(int parse_succeeded);

SemType *sem_check_binary(Token op, SemType *left, SemType *right,
                          int line, int column);
SemType *sem_check_unary(Token op, SemType *operand, int line, int column);
SemType *sem_check_assignment(SemType *target, int is_lvalue, SemType *value,
                              int line, int column);
SemType *sem_check_call(SemType *callee, SemType **args, int arg_count,
                        int line, int column);
void sem_check_condition(SemType *type, const char *construct, int line, int column);
void sem_check_print_arg(SemType *type, int line, int column);

#endif
