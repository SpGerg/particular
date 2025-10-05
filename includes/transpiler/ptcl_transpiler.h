#ifndef PTCL_TRANSPILER_H
#define PTCL_TRANSPILER_H

#include <ptcl_parser.h>

typedef struct ptcl_transpiler ptcl_transpiler;

typedef struct ptcl_transpiler_variable
{
    ptcl_name name;
    ptcl_type type;
    bool is_inner;
    bool is_self;
    ptcl_statement_func_body *root;
} ptcl_transpiler_variable;

typedef struct ptcl_transpiler_function
{
    ptcl_name name;
    ptcl_statement_func_body *root;
} ptcl_transpiler_function;

typedef struct ptcl_transpiler_anonymous
{
    char *original_name;
    char *alias;
    ptcl_statement_func_body *root;
} ptcl_transpiler_anonymous;

static ptcl_transpiler_variable ptcl_transpiler_variable_create(ptcl_name name, ptcl_type type, bool is_inner, ptcl_statement_func_body *root)
{
    return (ptcl_transpiler_variable){
        .name = name,
        .type = type,
        .is_inner = is_inner,
        .is_self = false,
        .root = root};
}

static ptcl_transpiler_variable ptcl_transpiler_variable_create_this(ptcl_type type, ptcl_statement_func_body *root)
{
    return (ptcl_transpiler_variable){
        .name = ptcl_name_create_fast_w("this", false),
        .type = type,
        .is_inner = true,
        .is_self = true,
        .root = root};
}

static ptcl_transpiler_anonymous ptcl_transpiler_anonymous_create(char *original_name, char *alias, ptcl_statement_func_body *root)
{
    return (ptcl_transpiler_anonymous){
        .original_name = original_name,
        .alias = alias,
        .root = root};
}

static ptcl_transpiler_function ptcl_transpiler_function_create(ptcl_name name, ptcl_statement_func_body *root)
{
    return (ptcl_transpiler_function){
        .name = name,
        .root = root};
}

ptcl_transpiler *ptcl_transpiler_create(ptcl_parser_result result);

char *ptcl_transpiler_transpile(ptcl_transpiler *transpiler);

bool ptcl_transpiler_append_word_s(ptcl_transpiler *transpiler, char *word);

bool ptcl_transpiler_append_word(ptcl_transpiler *transpiler, char *word);

bool ptcl_transpiler_append_character(ptcl_transpiler *transpiler, char character);

bool ptcl_transpiler_try_get_variable(ptcl_transpiler *transpiler, ptcl_name name, ptcl_transpiler_variable **variable);

bool ptcl_transpiler_is_inner(ptcl_transpiler *transpiler, ptcl_name name);

bool ptcl_transpiler_is_inner_function(ptcl_transpiler *transpiler, ptcl_name name);

bool ptcl_transpiler_add_variable(ptcl_transpiler *transpiler, ptcl_transpiler_variable variable);

bool ptcl_transpiler_add_variable_f(ptcl_transpiler *transpiler, ptcl_name name, ptcl_type type, bool is_inner, ptcl_statement_func_body *root);

bool ptcl_transpiler_add_inner_func(ptcl_transpiler *transpiler, ptcl_name name, ptcl_statement_func_body *root);

bool ptcl_transpiler_add_anonymous(ptcl_transpiler *transpiler, char *original_name, char *alias, ptcl_statement_func_body *root);

void ptcl_transpiler_add_func_body(ptcl_transpiler *transpiler, ptcl_statement_func_body func_body, bool with_brackets, bool is_func_body);

void ptcl_transpiler_add_statement(ptcl_transpiler *transpiler, ptcl_statement *statement, bool is_func_body);

void ptcl_transpiler_add_func_decl(ptcl_transpiler *transpiler, ptcl_statement_func_decl func_decl, ptcl_name name, ptcl_type *self);

void ptcl_transpiler_add_func_call(ptcl_transpiler *transpiler, ptcl_statement_func_call func_call);

void ptcl_transpiler_add_expression(ptcl_transpiler *transpiler, ptcl_expression *expression, bool specify_type);

void ptcl_transpiler_add_identifier(ptcl_transpiler *transpiler, ptcl_identifier identifier, bool is_declare);

void ptcl_transpiler_add_name(ptcl_transpiler *transpiler, ptcl_name name, bool is_declare);

void ptcl_transpiler_add_func_type_args(ptcl_transpiler *transpiler, ptcl_type_functon_pointer_type type);

bool ptcl_transpiler_add_type_and_name(ptcl_transpiler *transpiler, ptcl_type type, ptcl_name name, ptcl_statement_func_decl *func_decl, bool with_array, bool is_define);

void ptcl_transpiler_add_binary_type(ptcl_transpiler *transpiler, ptcl_binary_operator_type type);

char *ptcl_transpiler_generate_anonymous(ptcl_transpiler *transpiler);

char *ptcl_transpiler_get_func_name_in_type(char *type, char *function);

void ptcl_transpiler_clear_scope(ptcl_transpiler *transpiler);

void ptcl_transpiler_destroy(ptcl_transpiler *transpiler);

#endif // PTCL_TRANSPILER_H
