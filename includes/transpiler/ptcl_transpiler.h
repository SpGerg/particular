#ifndef PTCL_TRANSPILER_H
#define PTCL_TRANSPILER_H

#include <ptcl_parser.h>

typedef struct ptcl_transpiler ptcl_transpiler;

typedef struct ptcl_transpiler_variable
{
    char *name;
    ptcl_type type;
    bool is_inner;
    ptcl_statement_func_body *root;
} ptcl_transpiler_variable;

typedef struct ptcl_transpiler_function
{
    char *name;
    ptcl_statement_func_body *root;
} ptcl_transpiler_function;

static ptcl_transpiler_variable ptcl_transpiler_variable_create(char *name, ptcl_type type, bool is_inner, ptcl_statement_func_body *root)
{
    return (ptcl_transpiler_variable){
        .name = name,
        .type = type,
        .is_inner = is_inner,
        .root = root};
}

static ptcl_transpiler_function ptcl_transpiler_function_create(char *name, ptcl_statement_func_body *root)
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

bool ptcl_transpiler_is_inner(ptcl_transpiler *transpiler, char *name);

bool ptcl_transpiler_is_inner_function(ptcl_transpiler *transpiler, char *name);

bool ptcl_transpiler_add_variable(ptcl_transpiler *transpiler, char *name, ptcl_type type, bool is_inner, ptcl_statement_func_body *root);

bool ptcl_transpiler_add_inner_func(ptcl_transpiler *transpiler, char *name, ptcl_statement_func_body *root);

void ptcl_transpiler_clear_scope(ptcl_transpiler *transpiler);

void ptcl_transpiler_add_func_body(ptcl_transpiler *transpiler, ptcl_statement_func_body func_body, bool with_brackets, bool is_func_body);

void ptcl_transpiler_add_statement(ptcl_transpiler *transpiler, ptcl_statement *statement, bool is_func_body);

void ptcl_transpiler_add_func_decl(ptcl_transpiler *transpiler, ptcl_statement_func_decl func_decl, bool is_prototype);

void ptcl_transpiler_add_func_call(ptcl_transpiler *transpiler, ptcl_statement_func_call func_call);

void ptcl_transpiler_add_expression(ptcl_transpiler *transpiler, ptcl_expression expression, bool specify_type);

void ptcl_transpiler_add_identifier(ptcl_transpiler *transpiler, ptcl_identifier identifier);

void ptcl_transpiler_add_type(ptcl_transpiler *transpiler, ptcl_type type, bool with_array);

void ptcl_transpiler_add_binary_type(ptcl_transpiler *transpiler, ptcl_binary_operator_type type);

void ptcl_transpiler_destroy(ptcl_transpiler *transpiler);

#endif // PTCL_TRANSPILER_H