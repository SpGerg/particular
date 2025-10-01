#ifndef PTCL_PARSER_H
#define PTCL_PARSER_H

#include <ptcl_node.h>
#include <ptcl_parser_error.h>
#include <ptcl_lexer_configuration.h>

#define PTCL_PARSER_MAX_DEPTH 256
#define PTCL_PARSER_STATEMENT_TYPEDATA_TYPE_I 0
#define PTCL_PARSER_STATEMENT_TYPEDATA_COUNT 1
#define PTCL_PARSER_STATEMENT_TYPE_NAME "ptcl_statement_t"

static ptcl_name const ptcl_statement_t_name = {
    .value = PTCL_PARSER_STATEMENT_TYPE_NAME,
    .location = {0},
    .is_free = false,
    .is_anonymous = false};

static ptcl_type_comp_type const ptcl_statement_comp_type = {
    .identifier = ptcl_statement_t_name,
    .types = NULL,
    .count = 0,
    .functions = NULL,
    .is_optional = false,
    .is_any = false};

#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
static ptcl_type ptcl_statement_t_type = {
    .type = ptcl_value_type_type,
    .is_primitive = true,
    .is_static = true,
    .comp_type = &ptcl_statement_comp_type};

typedef struct ptcl_parser_function ptcl_parser_function;

typedef struct ptcl_parser_variable
{
    ptcl_type type;
    ptcl_expression *built_in;
    bool is_built_in;
    bool is_syntax_word;
    bool is_function_pointer;
    bool is_syntax_variable;
} ptcl_parser_variable;

typedef struct ptcl_parser_typedata
{
    ptcl_typedata_member *members;
    size_t count;
} ptcl_parser_typedata;

typedef struct ptcl_parser_comp_type
{
    ptcl_type_comp_type *comp_type;
} ptcl_parser_comp_type;

typedef enum ptcl_parser_syntax_node_type
{
    ptcl_parser_syntax_node_word_type,
    ptcl_parser_syntax_node_variable_type,
    ptcl_parser_syntax_node_object_type_type,
    ptcl_parser_syntax_node_value_type
} ptcl_parser_syntax_node_type;

typedef struct ptcl_parser_syntax_word_node
{
    ptcl_token_type type;
    ptcl_name name;
} ptcl_parser_syntax_word_node;

typedef struct ptcl_parser_syntax_word_variable
{
    ptcl_type type;
    char *name;
} ptcl_parser_syntax_word_variable;

typedef struct ptcl_parser_syntax_node
{
    ptcl_parser_syntax_node_type type;

    union
    {
        ptcl_parser_syntax_word_node word;
        ptcl_parser_syntax_word_variable variable;
        ptcl_type object_type;
        ptcl_expression *value;
    };
} ptcl_parser_syntax_node;

typedef struct ptcl_parser_syntax
{
    ptcl_parser_syntax_node *nodes;
    size_t count;
    size_t start;
    size_t tokens_count;
} ptcl_parser_syntax;

typedef struct ptcl_parser ptcl_parser;
typedef ptcl_expression *(*ptcl_built_in_function_t)(
    ptcl_parser *parser,
    ptcl_expression **arguments,
    size_t count,
    ptcl_location location,
    bool is_expression);

typedef struct ptcl_parser_function
{
    bool is_built_in;
    ptcl_built_in_function_t bind;
    ptcl_statement_func_decl func;
} ptcl_parser_function;

typedef enum ptcl_parser_instance_type
{
    ptcl_parser_instance_typedata_type,
    ptcl_parser_instance_comp_type_type,
    ptcl_parser_instance_function_type,
    ptcl_parser_instance_variable_type,
    ptcl_parser_instance_syntax_type
} ptcl_parser_instance_type;

typedef struct ptcl_parser_instance
{
    ptcl_statement_func_body *root;
    ptcl_parser_instance_type type;
    ptcl_name name;
    bool is_out_of_scope;

    union
    {
        ptcl_parser_typedata typedata;
        ptcl_parser_function function;
        ptcl_parser_variable variable;
        ptcl_parser_comp_type comp_type;
        ptcl_parser_syntax syntax;
    };
} ptcl_parser_instance;

typedef struct ptcl_lated_body
{
    ptcl_token *tokens;
    size_t count;
    size_t start;
} ptcl_lated_body;

typedef struct ptcl_parser_result
{
    ptcl_lexer_configuration *configuration;
    ptcl_statement_func_body body;
    ptcl_parser_error *errors;
    size_t count;
    ptcl_parser_instance *instances;
    size_t instances_count;
    ptcl_lated_body *lated_bodies;
    size_t lated_bodies_count;
    bool is_critical;
} ptcl_parser_result;

static ptcl_parser_instance ptcl_parser_comp_type_create(ptcl_name name, ptcl_statement_func_body *root, ptcl_type_comp_type *type)
{
    return (ptcl_parser_instance){
        .type = ptcl_parser_instance_comp_type_type,
        .root = root,
        .name = name,
        .is_out_of_scope = false,
        .comp_type = (ptcl_parser_comp_type){
            .comp_type = type}};
}

static ptcl_parser_instance ptcl_parser_variable_create(ptcl_name name, ptcl_type type, ptcl_expression *built_in, bool is_built_in, ptcl_statement_func_body *root)
{
    return (ptcl_parser_instance){
        .type = ptcl_parser_instance_variable_type,
        .root = root,
        .name = name,
        .is_out_of_scope = false,
        .variable = (ptcl_parser_variable){
            .type = type,
            .built_in = built_in,
            .is_built_in = is_built_in,
            .is_syntax_word = false,
            .is_function_pointer = false,
            .is_syntax_variable = false}};
}

static ptcl_parser_instance ptcl_parser_func_variable_create(ptcl_name name, ptcl_type type, ptcl_statement_func_body *root)
{
    return (ptcl_parser_instance){
        .type = ptcl_parser_instance_variable_type,
        .root = root,
        .name = name,
        .is_out_of_scope = false,
        .variable = (ptcl_parser_variable){
            .type = type,
            .is_built_in = false,
            .is_syntax_word = false,
            .is_function_pointer = true,
            .is_syntax_variable = false}};
}

static ptcl_parser_instance ptcl_parser_function_create(ptcl_statement_func_body *root, ptcl_statement_func_decl func)
{
    return (ptcl_parser_instance){
        .type = ptcl_parser_instance_function_type,
        .root = root,
        .name = func.name,
        .is_out_of_scope = false,
        .function = (ptcl_parser_function){
            .is_built_in = false,
            .func = func,
        }};
}

static ptcl_parser_instance ptcl_parser_built_in_create(ptcl_statement_func_body *root, ptcl_name name, ptcl_built_in_function_t bind, ptcl_argument *arguments, size_t count, ptcl_type return_type)
{
    return (ptcl_parser_instance){
        .type = ptcl_parser_instance_function_type,
        .name = name,
        .root = root,
        .is_out_of_scope = false,
        .function = (ptcl_parser_function){
            .is_built_in = true,
            .bind = bind,
            .func = (ptcl_statement_func_decl){
                .name = name,
                .arguments = arguments,
                .count = count,
                .return_type = return_type}}};
}

static ptcl_parser_instance ptcl_parser_typedata_create(ptcl_statement_func_body *root, ptcl_name name, ptcl_typedata_member *members, size_t count)
{
    return (ptcl_parser_instance){
        .type = ptcl_parser_instance_typedata_type,
        .root = NULL,
        .name = name,
        .is_out_of_scope = false,
        .typedata = (ptcl_parser_typedata){
            .members = members,
            .count = count}};
}

static ptcl_parser_instance ptcl_parser_syntax_create(ptcl_name name, ptcl_statement_func_body *root, ptcl_parser_syntax_node *nodes, size_t count)
{
    return (ptcl_parser_instance){
        .type = ptcl_parser_instance_syntax_type,
        .root = root,
        .name = name,
        .is_out_of_scope = false,
        .syntax = (ptcl_parser_syntax){
            .nodes = nodes,
            .count = count}};
}

static ptcl_parser_syntax_node ptcl_parser_syntax_node_create_word(ptcl_token_type type, ptcl_name name)
{
    return (ptcl_parser_syntax_node){
        .type = ptcl_parser_syntax_node_word_type,
        .word = (ptcl_parser_syntax_word_node){
            .type = type,
            .name = name}};
}

static ptcl_parser_syntax_node ptcl_parser_syntax_node_create_variable(ptcl_type type, char *name)
{
    return (ptcl_parser_syntax_node){
        .type = ptcl_parser_syntax_node_variable_type,
        .variable = (ptcl_parser_syntax_word_variable){
            .type = type,
            .name = name}};
}

static ptcl_parser_syntax_node ptcl_parser_syntax_node_create_object_type(ptcl_type type)
{
    return (ptcl_parser_syntax_node){
        .type = ptcl_parser_syntax_node_object_type_type,
        .object_type = type};
}

static ptcl_parser_syntax_node ptcl_parser_syntax_node_create_value(ptcl_expression *value)
{
    return (ptcl_parser_syntax_node){
        .type = ptcl_parser_syntax_node_value_type,
        .value = value};
}

static void ptcl_parser_syntax_node_destroy(ptcl_parser_syntax_node node)
{
    switch (node.type)
    {
    case ptcl_parser_syntax_node_variable_type:
        ptcl_type_destroy(node.variable.type);
        break;
    case ptcl_parser_syntax_node_object_type_type:
        ptcl_type_destroy(node.object_type);
        break;
    case ptcl_parser_syntax_node_value_type:
        ptcl_expression_destroy(node.value);
        break;
    }
}

static void ptcl_parser_syntax_destroy(ptcl_parser_syntax syntax)
{
    if (syntax.count > 0)
    {
        for (size_t i = 0; i < syntax.count; i++)
        {
            ptcl_parser_syntax_node_destroy(syntax.nodes[i]);
        }

        free(syntax.nodes);
    }
}

static void ptcl_parser_instance_destroy(ptcl_parser_instance *instance)
{
    switch (instance->type)
    {
    case ptcl_parser_instance_variable_type:
        if (instance->variable.is_syntax_word)
        {
            free(instance->variable.built_in->word.value);
            free(instance->variable.built_in);
        }
        else if (instance->variable.is_function_pointer)
        {
            free(instance->variable.type.function_pointer.return_type);
        }
        else
        {
            if (instance->variable.is_built_in || instance->variable.is_syntax_variable)
            {
                ptcl_expression_destroy(instance->variable.built_in);
            }
        }

        break;
    case ptcl_parser_instance_syntax_type:
        ptcl_parser_syntax_destroy(instance->syntax);
        break;
    case ptcl_parser_instance_comp_type_type:
        free(instance->comp_type.comp_type);
        break;
    case ptcl_parser_instance_function_type:
        break;
    }
}

ptcl_parser *ptcl_parser_create(ptcl_tokens_list *input, ptcl_lexer_configuration *configuration);

ptcl_parser_result ptcl_parser_parse(ptcl_parser *parser);

ptcl_statement_type ptcl_parser_parse_get_statement(ptcl_parser *parser, bool *is_finded);

ptcl_statement *ptcl_parser_parse_statement(ptcl_parser *parser);

ptcl_attributes ptcl_parser_parse_attributes(ptcl_parser *parser);

bool ptcl_parser_parse_try_parse_syntax_usage_here(ptcl_parser *parser, bool is_statement);

bool ptcl_parser_parse_try_parse_syntax_usage(
    ptcl_parser *parser, ptcl_parser_syntax_node **nodes, size_t count, int down_start, bool skip_first, bool is_statement);

void ptcl_parser_leave_from_syntax(ptcl_parser *parser);

ptcl_statement_func_call ptcl_parser_parse_func_call(ptcl_parser *parser, ptcl_parser_function *function, bool is_expression);

ptcl_statement_func_decl ptcl_parser_parse_func_decl(ptcl_parser *parser, bool is_prototype);

ptcl_statement_typedata_decl ptcl_parser_parse_typedata_decl(ptcl_parser *parser, bool is_prototype);

ptcl_statement_type_decl ptcl_parser_parse_type_decl(ptcl_parser *parser, bool is_prototype);

ptcl_statement_assign ptcl_parser_parse_assign(ptcl_parser *parser);

ptcl_statement_return ptcl_parser_parse_return(ptcl_parser *parser);

ptcl_statement *ptcl_parser_parse_if(ptcl_parser *parser, bool is_static);

ptcl_expression *ptcl_parser_parse_if_expression(ptcl_parser *parser, bool is_static);

ptcl_statement_func_body ptcl_parser_parse_unsyntax(ptcl_parser *parser);

void ptcl_parser_parse_syntax(ptcl_parser *parser);

void ptcl_parser_parse_each(ptcl_parser *parser);

void ptcl_parser_parse_undefine(ptcl_parser *parser);

ptcl_statement_func_body ptcl_parser_parse_func_body(ptcl_parser *parser, bool with_brackets, bool change_root);

void ptcl_parser_parse_func_body_by_pointer(ptcl_parser *parser, ptcl_statement_func_body *func_body_pointer, bool with_brackets, bool change_root);

void ptcl_parser_parse_extra_body(ptcl_parser *parser, bool is_syntax);

ptcl_type ptcl_parser_parse_type(ptcl_parser *parser, bool with_word, bool with_any);

ptcl_expression *ptcl_parser_parse_cast(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_parse_binary(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_parse_additive(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_parse_multiplicative(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_parse_unary(ptcl_parser *parser, bool only_value, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_parse_dot(ptcl_parser *parser, ptcl_type *except, ptcl_expression *left, bool with_word);

ptcl_expression *ptcl_parser_parse_array_element(ptcl_parser *parser, ptcl_type *except, ptcl_expression *left, bool with_word);

ptcl_expression *ptcl_parser_parse_value(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression_ctor ptcl_parser_parse_ctor(ptcl_parser *parser, ptcl_name name, ptcl_parser_typedata typedata);

ptcl_name ptcl_parser_parse_name_word(ptcl_parser *parser);

ptcl_name ptcl_parser_parse_name_word_or_type(ptcl_parser *parser);

ptcl_name ptcl_parser_parse_name(ptcl_parser *parser, bool with_type);

ptcl_expression *ptcl_parser_get_default(ptcl_parser *parser, ptcl_type type, ptcl_location location);

ptcl_token ptcl_parser_except(ptcl_parser *parser, ptcl_token_type token_type);

bool ptcl_parser_match(ptcl_parser *parser, ptcl_token_type token_type);

ptcl_token ptcl_parser_peek(ptcl_parser *parser, size_t offset);

ptcl_token ptcl_parser_current(ptcl_parser *parser);

void ptcl_parser_skip(ptcl_parser *parser);

bool ptcl_parser_ended(ptcl_parser *parser);

bool ptcl_parser_add_instance(ptcl_parser *parser, ptcl_parser_instance instance);

bool ptcl_parser_try_get_instance(ptcl_parser *parser, ptcl_name name, ptcl_parser_instance_type type, ptcl_parser_instance **instance);

bool ptcl_parser_try_get_instance_any(ptcl_parser *parser, ptcl_name name, ptcl_parser_instance **instance);

bool ptcl_parser_try_get_instance_in_root(ptcl_parser *parser, ptcl_name name, ptcl_parser_instance_type type, ptcl_parser_instance **instance);

bool ptcl_parser_try_get_function(ptcl_parser *parser, ptcl_name name, ptcl_parser_function **function);

bool ptcl_parser_is_defined(ptcl_parser *parser, ptcl_name name);

bool ptcl_parser_check_arguments(ptcl_parser *parser, ptcl_parser_function *function, ptcl_expression **arguments, size_t count);

bool ptcl_parser_syntax_try_find(ptcl_parser *parser, ptcl_parser_syntax_node *nodes, size_t count, ptcl_parser_syntax **syntax, bool *any_found_or_continue);

bool ptcl_parser_try_get_typedata_member(ptcl_parser *parser, ptcl_name name, char *member_name, ptcl_typedata_member **member);

void ptcl_parser_clear_scope(ptcl_parser *parser);

void ptcl_parser_throw_out_of_memory(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_except_token(ptcl_parser *parser, char *value, ptcl_location location);

void ptcl_parser_throw_except_type_specifier(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_incorrect_type(ptcl_parser *parser, char *excepted, char *received, ptcl_location location);

void ptcl_parser_throw_fast_incorrect_type(ptcl_parser *parser, ptcl_type excepted, ptcl_type received, ptcl_location location);

void ptcl_parser_throw_variable_redefination(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_none_type(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_must_be_variable(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_must_be_static(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_unknown_member(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_unknown_statement(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_unknown_expression(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_unknown_variable(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_unknown_syntax(ptcl_parser *parser, ptcl_parser_syntax syntax, ptcl_location location);

void ptcl_parser_throw_wrong_arguments(ptcl_parser *parser, char *name, ptcl_expression **values, size_t count, ptcl_location location);

void ptcl_parser_throw_max_depth(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_unknown_function(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_unknown_variable_or_type(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_unknown_type(ptcl_parser *parser, char *value, ptcl_location location);

void ptcl_parser_throw_redefination(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_add_error(ptcl_parser *parser, ptcl_parser_error error);

void ptcl_parser_result_destroy(ptcl_parser_result result);

void ptcl_parser_destroy(ptcl_parser *parser);

#endif // PTCL_PARSER_H