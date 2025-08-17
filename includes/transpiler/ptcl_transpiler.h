#ifndef PTCL_TRANSPILER_H
#define PTCL_TRANSPILER_H

#include <ptcl_parser.h>

typedef struct ptcl_transpiler ptcl_transpiler;

typedef struct ptcl_inner_function
{
    char *original;
    char *name;
    char **arguments;
    size_t count;
    ptcl_statement_func_decl *func;
    ptcl_statement_func_body *root;
} ptcl_inner_function;

static ptcl_inner_function ptcl_inner_function_create(char *original, char *name, ptcl_statement_func_body *root)
{
    return (ptcl_inner_function){
        .original = original,
        .name = name,
        .arguments = NULL,
        .count = 0,
        .root = root};
}

ptcl_transpiler *ptcl_transpiler_create(ptcl_parser_result result);

char *ptcl_transpiler_transpile(ptcl_transpiler *transpiler);

bool ptcl_transpiler_append_word_s(ptcl_transpiler *transpiler, char *word);

bool ptcl_transpiler_append_word(ptcl_transpiler *transpiler, char *word);

bool ptcl_transpiler_append_character(ptcl_transpiler *transpiler, char character);

void ptcl_transpiler_add_func_body(ptcl_transpiler *transpiler, ptcl_statement_func_body func_body, bool with_brackets);

void ptcl_transpiler_add_statement(ptcl_transpiler *transpiler, ptcl_statement *statement);

void ptcl_transpiler_add_func_decl(ptcl_transpiler *transpiler, char *name, ptcl_statement_func_decl func_decl, bool is_prototype);

void ptcl_transpiler_add_func_call(ptcl_transpiler *transpiler, ptcl_statement_func_call func_call);

void ptcl_transpiler_add_expression(ptcl_transpiler *transpiler, ptcl_expression expression, bool specify_type);

void ptcl_transpiler_add_identifier(ptcl_transpiler *transpiler, ptcl_identifier identifier);

void ptcl_transpiler_add_type(ptcl_transpiler *transpiler, ptcl_type type, bool with_array);

void ptcl_transpiler_add_binary_type(ptcl_transpiler *transpiler, ptcl_binary_operator_type type);

void ptcl_transpiler_destroy(ptcl_transpiler *transpiler);

#endif // PTCL_TRANSPILER_H