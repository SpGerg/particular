#ifndef PTCL_PARSER_H
#define PTCL_PARSER_H

#include <ptcl_node.h>
#include <ptcl_parser_error.h>
#include <ptcl_lexer_configuration.h>

#define PTCL_PARSER_MAX_DEPTH 256
#define PTCL_PARSER_DEFAULT_INSTANCE_CAPACITY 16
#define PTCL_PARSER_DECL_INSERT_DEPTH 16
#define PTCL_PARSER_STATEMENT_TYPE_NAME "ptcl_statement_t"
#define PTCL_PARSER_EXPRESSION_TYPE_NAME "ptcl_expression_t"
#define PTCL_PARSER_TOKEN_TYPE_NAME "ptcl_token_t"
#define PTCL_PARSER_ERROR_FUNC_NAME "ptcl_error"

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

static ptcl_name const ptcl_expression_t_name = {
    .value = PTCL_PARSER_EXPRESSION_TYPE_NAME,
    .location = {0},
    .is_free = false,
    .is_anonymous = false};

static ptcl_name const ptcl_token_t_name = {
    .value = PTCL_PARSER_TOKEN_TYPE_NAME,
    .location = {0},
    .is_free = false,
    .is_anonymous = false};

static ptcl_type_comp_type const ptcl_token_comp_type = {
    .identifier = ptcl_token_t_name,
    .types = NULL,
    .count = 0,
    .functions = NULL,
    .is_optional = false,
    .is_any = false};

static ptcl_name const ptcl_insert_name = {
    .value = "ptcl_insert",
    .location = {0},
    .is_free = false,
    .is_anonymous = false};

#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
static ptcl_type ptcl_statement_t_type = {
    .type = ptcl_value_type_type,
    .is_primitive = true,
    .is_static = true,
    .comp_type = &ptcl_statement_comp_type};

static ptcl_type ptcl_token_t_type = {
    .type = ptcl_value_type_type,
    .is_primitive = true,
    .is_static = true,
    .comp_type = &ptcl_token_comp_type};

typedef struct ptcl_parser_tokens_state
{
    size_t position;
    ptcl_token *tokens;
    size_t count;
} ptcl_parser_tokens_state;

typedef struct ptcl_parser_this_s_pair
{
    ptcl_statement_func_body *body;
    ptcl_parser_tokens_state state;
} ptcl_parser_this_s_pair;

typedef struct ptcl_parser_function ptcl_parser_function;

typedef struct ptcl_parser_variable
{
    ptcl_name name;
    ptcl_statement_func_body *root;
    ptcl_type type;
    ptcl_expression *built_in;
    bool is_built_in;
    bool is_syntax_word;
    bool is_function_pointer;
    bool is_syntax_variable;
    bool is_out_of_scope;
} ptcl_parser_variable;

typedef struct ptcl_parser_typedata
{
    ptcl_statement_func_body *root;
    ptcl_name name;
    ptcl_argument *members;
    size_t count;
    bool is_out_of_scope;
} ptcl_parser_typedata;

typedef struct ptcl_parser_comp_type
{
    ptcl_name identifier;
    ptcl_statement_func_body *root;
    ptcl_type_comp_type *comp_type;
    ptcl_type_comp_type *static_type;
    bool is_out_of_scope;
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
    bool is_variadic;
} ptcl_parser_syntax_word_variable;

typedef struct ptcl_parser_tokens_node
{
    ptcl_token *tokens;
    size_t count;
} ptcl_parser_tokens_node;

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
    ptcl_name name;
    ptcl_statement_func_body *root;
    ptcl_parser_syntax_node *nodes;
    size_t count;
    size_t index;
    bool is_out_of_scope;
} ptcl_parser_syntax;

typedef struct ptcl_parser_syntax_pair
{
    ptcl_parser_syntax syntax;
    ptcl_parser_tokens_state state;
} ptcl_parser_syntax_pair;

typedef struct ptcl_parser ptcl_parser;
typedef ptcl_expression *(*ptcl_built_in_function_t)(
    ptcl_parser *parser,
    ptcl_expression **arguments,
    size_t count,
    ptcl_location location,
    bool is_expression);

typedef struct ptcl_parser_function
{
    ptcl_name name;
    ptcl_statement_func_body *root;
    bool is_built_in;
    ptcl_built_in_function_t bind;
    ptcl_statement_func_decl func;
    bool is_out_of_scope;
} ptcl_parser_function;

typedef enum ptcl_parser_instance_type
{
    ptcl_parser_instance_typedata_type,
    ptcl_parser_instance_comp_type_type,
    ptcl_parser_instance_function_type,
    ptcl_parser_instance_variable_type,
    ptcl_parser_instance_syntax_type
} ptcl_parser_instance_type;

typedef struct ptcl_parser_result
{
    ptcl_lexer_configuration *configuration;
    ptcl_statement_func_body body;
    ptcl_parser_error *errors;
    size_t errors_count;
    ptcl_parser_syntax *syntaxes;
    size_t syntaxes_count;
    ptcl_parser_typedata *typedatas;
    size_t typedatas_count;
    ptcl_parser_comp_type *comp_types;
    size_t comp_types_count;
    ptcl_parser_function *functions;
    size_t functions_count;
    ptcl_parser_variable *variables;
    size_t variables_count;
    ptcl_parser_tokens_state *lated_states;
    size_t lated_states_count;
    ptcl_parser_this_s_pair *this_pairs;
    size_t this_pairs_count;
    bool is_critical;
} ptcl_parser_result;

static ptcl_parser_syntax_pair ptcl_parser_syntax_pair_create(ptcl_parser_syntax syntax, ptcl_parser_tokens_state state)
{
    return (ptcl_parser_syntax_pair){
        .syntax = syntax,
        .state = state};
}

static ptcl_parser_this_s_pair ptcl_parser_this_s_pair_create(ptcl_statement_func_body *body, ptcl_parser_tokens_state state)
{
    return (ptcl_parser_this_s_pair){
        .body = body,
        .state = state};
}

static ptcl_parser_comp_type ptcl_parser_comp_type_create(ptcl_statement_func_body *root, ptcl_name identifier, ptcl_type_comp_type *type, bool is_static)
{
    return (ptcl_parser_comp_type){
        .root = root,
        .identifier = identifier,
        .static_type = is_static ? type : NULL,
        .comp_type = is_static ? NULL : type,
        .is_out_of_scope = false};
}

static ptcl_parser_variable ptcl_parser_variable_create(ptcl_name name, ptcl_type type, ptcl_expression *built_in, bool is_built_in, ptcl_statement_func_body *root)
{
    return (ptcl_parser_variable){
        .name = name,
        .root = root,
        .is_out_of_scope = false,
        .type = type,
        .built_in = built_in,
        .is_built_in = is_built_in,
        .is_syntax_word = false,
        .is_function_pointer = false,
        .is_syntax_variable = false};
}

static ptcl_parser_variable ptcl_parser_func_variable_create(ptcl_name name, ptcl_type type, ptcl_statement_func_body *root)
{
    return (ptcl_parser_variable){
        .name = name,
        .root = root,
        .is_out_of_scope = false,
        .type = type,
        .is_built_in = false,
        .is_syntax_word = false,
        .is_function_pointer = true,
        .is_syntax_variable = false};
}

static ptcl_parser_function ptcl_parser_function_create(ptcl_statement_func_body *root, ptcl_statement_func_decl func)
{
    return (ptcl_parser_function){
        .name = func.name,
        .root = root,
        .is_out_of_scope = false,
        .is_built_in = false,
        .func = func};
};

static ptcl_parser_function ptcl_parser_built_in_create(ptcl_statement_func_body *root, ptcl_name name, ptcl_built_in_function_t bind, ptcl_argument *arguments, size_t count, ptcl_type return_type)
{
    return (ptcl_parser_function){
        .name = name,
        .root = root,
        .is_out_of_scope = false,
        .is_built_in = true,
        .bind = bind,
        .func = (ptcl_statement_func_decl){
            .name = name,
            .arguments = arguments,
            .count = count,
            .return_type = return_type}};
}

static ptcl_parser_typedata ptcl_parser_typedata_create(ptcl_statement_func_body *root, ptcl_name name, ptcl_argument *members, size_t count)
{
    return (ptcl_parser_typedata){
        .root = root,
        .name = name,
        .is_out_of_scope = false,
        .members = members,
        .count = count};
}

static ptcl_parser_syntax ptcl_parser_syntax_create(ptcl_name name, ptcl_statement_func_body *root, ptcl_parser_syntax_node *nodes, size_t count, size_t index)
{
    return (ptcl_parser_syntax){
        .root = root,
        .name = name,
        .is_out_of_scope = false,
        .nodes = nodes,
        .count = count,
        .index = index};
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
            .is_variadic = false,
            .name = name}};
}

static ptcl_parser_syntax_node ptcl_parser_syntax_node_create_end_token(char *name)
{
    return (ptcl_parser_syntax_node){
        .type = ptcl_parser_syntax_node_variable_type,
        .variable = (ptcl_parser_syntax_word_variable){
            .name = name,
            .type = ptcl_type_create_array(&ptcl_token_t_type, -1),
            .is_variadic = true}};
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
    case ptcl_parser_syntax_node_word_type:
        ptcl_name_destroy(node.word.name);
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

static void ptcl_parser_variable_destroy(ptcl_parser_variable variable)
{
    if (variable.is_syntax_word)
    {
        free(variable.built_in);
    }
    else if (variable.is_function_pointer)
    {
        free(variable.type.function_pointer.return_type);
    }
    else
    {
        if (variable.is_built_in || variable.is_syntax_variable)
        {
            ptcl_expression_destroy(variable.built_in);
        }
    }
}

static void ptcl_parser_comp_type_destroy(ptcl_parser_comp_type comp_type)
{
    free(comp_type.comp_type);
    free(comp_type.static_type);
}

ptcl_parser *ptcl_parser_create(ptcl_tokens_list *input, ptcl_lexer_configuration *configuration);

ptcl_parser_result ptcl_parser_parse(ptcl_parser *parser);

ptcl_statement_type ptcl_parser_parse_get_statement(ptcl_parser *parser, bool *is_finded);

ptcl_statement *ptcl_parser_parse_statement(ptcl_parser *parser);

ptcl_attributes ptcl_parser_parse_attributes(ptcl_parser *parser);

bool ptcl_parser_parse_try_syntax_usage_here(ptcl_parser *parser, bool is_statement);

bool ptcl_parser_parse_try_syntax_usage(
    ptcl_parser *parser, ptcl_parser_syntax_node **nodes, size_t count, int down_start, bool skip_first, bool is_statement);

void ptcl_parser_leave_from_syntax(ptcl_parser *parser);

ptcl_statement_func_call ptcl_parser_func_call(ptcl_parser *parser, ptcl_statement_func_decl *function, ptcl_expression *self, bool is_expression);

ptcl_statement_func_decl ptcl_parser_func_decl(ptcl_parser *parser, bool is_prototype, bool is_global, bool is_static);

ptcl_statement_typedata_decl ptcl_parser_typedata_decl(ptcl_parser *parser, bool is_prototype, bool is_global);

ptcl_statement_type_decl ptcl_parser_type_decl(ptcl_parser *parser, bool is_prototype, bool is_global);

ptcl_statement_assign ptcl_parser_assign(ptcl_parser *parser, bool is_global);

ptcl_statement_return ptcl_parser_return(ptcl_parser *parser);

ptcl_statement *ptcl_parser_if(ptcl_parser *parser, bool is_static);

ptcl_expression *ptcl_parser_if_expression(ptcl_parser *parser, bool is_static);

ptcl_statement_func_body ptcl_parser_unsyntax(ptcl_parser *parser);

void ptcl_parser_syntax_decl(ptcl_parser *parser);

void ptcl_parser_each(ptcl_parser *parser);

void ptcl_parser_undefine(ptcl_parser *parser);

ptcl_statement_func_body ptcl_parser_func_body(ptcl_parser *parser, bool with_brackets, bool change_root, bool is_ignore_error);

void ptcl_parser_func_body_by_pointer(ptcl_parser *parser, ptcl_statement_func_body *func_body_pointer, bool with_brackets, bool change_root, bool is_static);

void ptcl_parser_extra_body(ptcl_parser *parser, bool is_syntax);

ptcl_type ptcl_parser_type(ptcl_parser *parser, bool with_word, bool with_any);

ptcl_expression *ptcl_parser_cast(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_binary(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_additive(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_multiplicative(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_unary(ptcl_parser *parser, bool only_value, ptcl_type *except, bool with_word);

ptcl_expression *ptcl_parser_dot(ptcl_parser *parser, ptcl_type *except, ptcl_expression *left, bool is_expression, bool with_word);

ptcl_expression *ptcl_parser_array_element(ptcl_parser *parser, ptcl_type *except, ptcl_expression *left, bool with_word);

ptcl_expression *ptcl_parser_value(ptcl_parser *parser, ptcl_type *except, bool with_word);

ptcl_expression_ctor ptcl_parser_ctor(ptcl_parser *parser, ptcl_name name, ptcl_parser_typedata typedata);

ptcl_name ptcl_parser_name_word(ptcl_parser *parser);

ptcl_name ptcl_parser_name_word_or_type(ptcl_parser *parser);

ptcl_name ptcl_parser_name(ptcl_parser *parser, bool with_type);

ptcl_expression *ptcl_parser_get_default(ptcl_parser *parser, ptcl_type type, ptcl_location location);

ptcl_token ptcl_parser_except(ptcl_parser *parser, ptcl_token_type token_type);

bool ptcl_parser_match(ptcl_parser *parser, ptcl_token_type token_type);

ptcl_token ptcl_parser_peek(ptcl_parser *parser, size_t offset);

ptcl_token ptcl_parser_current(ptcl_parser *parser);

ptcl_parser_tokens_state ptcl_parser_state(ptcl_parser *parser);

void ptcl_parser_set_state(ptcl_parser *parser, ptcl_parser_tokens_state state);

ptcl_token *ptcl_parser_tokens(ptcl_parser *parser);

void ptcl_parser_set_tokens(ptcl_parser *parser, ptcl_token *tokens);

size_t ptcl_parser_count(ptcl_parser *parser);

void ptcl_parser_set_count(ptcl_parser *parser, size_t count);

size_t ptcl_parser_position(ptcl_parser *parser);

void ptcl_parser_set_position(ptcl_parser *parser, size_t position);

void ptcl_parser_skip(ptcl_parser *parser);

void ptcl_parser_back(ptcl_parser *parser);

bool ptcl_parser_ended(ptcl_parser *parser);

bool ptcl_parser_critical(ptcl_parser *parser);

ptcl_statement_func_body *ptcl_parser_root(ptcl_parser *parser);

size_t ptcl_parser_variables_count(ptcl_parser *parser);

ptcl_parser_variable *ptcl_parser_variables(ptcl_parser *parser);

bool ptcl_parser_add_this_pair(ptcl_parser *parser, ptcl_parser_this_s_pair instance);

ptcl_statement *ptcl_parser_insert_pairs(ptcl_parser *parser, ptcl_statement *statement, ptcl_statement_func_body *body);

bool ptcl_parser_add_instance_syntax(ptcl_parser *parser, ptcl_parser_syntax instance);

bool ptcl_parser_add_instance_comp_type(ptcl_parser *parser, ptcl_parser_comp_type instance);

bool ptcl_parser_add_instance_typedata(ptcl_parser *parser, ptcl_parser_typedata instance);

bool ptcl_parser_add_instance_function(ptcl_parser *parser, ptcl_parser_function instance);

bool ptcl_parser_add_instance_variable(ptcl_parser *parser, ptcl_parser_variable instance);

bool ptcl_parser_try_get_syntax(ptcl_parser *parser, ptcl_name name, ptcl_parser_syntax **instance);

bool ptcl_parser_try_get_comp_type(ptcl_parser *parser, ptcl_name name, bool is_static, ptcl_parser_comp_type **instance);

bool ptcl_parser_try_get_any_comp_type(ptcl_parser *parser, ptcl_name name, ptcl_parser_comp_type **instance);

bool ptcl_parser_try_get_typedata(ptcl_parser *parser, ptcl_name name, ptcl_parser_typedata **instance);

bool ptcl_parser_try_get_function(ptcl_parser *parser, ptcl_name name, ptcl_parser_function **instance);

bool ptcl_parser_try_get_variable(ptcl_parser *parser, ptcl_name name, ptcl_parser_variable **instance);

int ptcl_parser_add_lated_body(ptcl_parser *parser, size_t start, size_t tokens_count, ptcl_location location);

bool ptcl_parser_is_syntax_defined(ptcl_parser *parser, ptcl_name name);

bool ptcl_parser_is_comp_type_defined(ptcl_parser *parser, ptcl_name name, bool is_static);

bool ptcl_parser_is_typedata_defined(ptcl_parser *parser, ptcl_name name);

bool ptcl_parser_is_function_defined(ptcl_parser *parser, ptcl_name name);

bool ptcl_parser_is_variable_defined(ptcl_parser *parser, ptcl_name name);

bool ptcl_parser_is_defined(ptcl_parser *parser, ptcl_name name);

bool ptcl_parser_check_arguments(ptcl_parser *parser, ptcl_parser_function *function, ptcl_expression **arguments, size_t count);

bool ptcl_parser_syntax_try_find(ptcl_parser *parser, ptcl_parser_syntax_node *nodes, size_t count, ptcl_parser_syntax **syntax, bool *can_continue, char **end_token);

bool ptcl_parser_try_get_typedata_member(ptcl_parser *parser, ptcl_name name, char *member_name, ptcl_argument **member, size_t *index);

void ptcl_parser_clear_scope(ptcl_parser *parser);

ptcl_expression *ptcl_parser_get_ctor_from_dot(ptcl_parser *parser, ptcl_expression *dot, bool is_root);

void ptcl_parser_throw_out_of_memory(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_not_allowed_token(ptcl_parser *parser, char *token, ptcl_location location);

void ptcl_parser_throw_except_token(ptcl_parser *parser, char *value, ptcl_location location);

void ptcl_parser_throw_except_type_specifier(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_incorrect_type(ptcl_parser *parser, char *excepted, char *received, ptcl_location location);

void ptcl_parser_throw_fast_incorrect_type(ptcl_parser *parser, ptcl_type excepted, ptcl_type received, ptcl_location location);

void ptcl_parser_throw_variable_redefinition(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_none_type(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_must_be_variable(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_must_be_static(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_unknown_member(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_unknown_statement(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_unknown_expression(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_unknown_variable(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_unknown_syntax(ptcl_parser *parser, ptcl_parser_syntax syntax, ptcl_location location);

void ptcl_parser_throw_wrong_arguments(ptcl_parser *parser, char *name, ptcl_expression **values, size_t count, ptcl_argument *arguments, size_t arguments_count, ptcl_location location);

void ptcl_parser_throw_max_depth(ptcl_parser *parser, ptcl_location location);

void ptcl_parser_throw_unknown_function(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_unknown_variable_or_type(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_unknown_type(ptcl_parser *parser, char *value, ptcl_location location);

void ptcl_parser_throw_redefination(ptcl_parser *parser, char *name, ptcl_location location);

void ptcl_parser_throw_user(ptcl_parser *parser, char *message, ptcl_location location);

void ptcl_parser_add_error(ptcl_parser *parser, ptcl_parser_error error);

void ptcl_parser_result_destroy(ptcl_parser_result result);

void ptcl_parser_destroy(ptcl_parser *parser);

#endif // PTCL_PARSER_H
