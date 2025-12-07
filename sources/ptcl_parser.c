#include <ptcl_parser.h>
#include <ptcl_parser_builder.h>
#include <ptcl_interpreter.h>

#define PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count) \
    for (size_t i = 0; i < count; i++)                  \
    {                                                   \
        ptcl_expression_destroy(arguments[i]);          \
    }                                                   \
    free(arguments);

#define SETUP_STATIC_ANY()                \
    ptcl_type static_any = ptcl_type_any; \
    static_any.is_static = true

#define CREATE_TOKEN_TYPE()                                                                            \
    ptcl_comp_type_builder token_builder = ptcl_comp_type_builder_create(PTCL_PARSER_TOKEN_TYPE_NAME); \
    ptcl_type_comp_type *token_comp_type = malloc(sizeof(ptcl_type_comp_type));                        \
    if (token_comp_type == NULL)                                                                       \
        goto out_of_memory;                                                                            \
    *token_comp_type = ptcl_comp_type_builder_build_type(&token_builder);                              \
    ptcl_type token_type = ptcl_type_create_comp_type_t(token_comp_type);                              \
    ptcl_parser_add_instance_comp_type(parser, ptcl_comp_type_builder_build(&token_builder, token_type.comp_type))

#define CREATE_STATEMENT_TYPE()                                                                                \
    ptcl_comp_type_builder statement_builder = ptcl_comp_type_builder_create(PTCL_PARSER_STATEMENT_TYPE_NAME); \
    ptcl_type_comp_type *statement_comp_type = malloc(sizeof(ptcl_type_comp_type));                            \
    if (statement_comp_type == NULL)                                                                           \
        goto out_of_memory;                                                                                    \
    *statement_comp_type = ptcl_comp_type_builder_build_type(&statement_builder);                              \
    ptcl_type statement_type = ptcl_type_create_comp_type_t(statement_comp_type);                              \
    ptcl_parser_add_instance_comp_type(parser, ptcl_comp_type_builder_build(&statement_builder, statement_type.comp_type))

#define CREATE_EXPRESSION_TYPE()                                                                                 \
    ptcl_comp_type_builder expression_builder = ptcl_comp_type_builder_create(PTCL_PARSER_EXPRESSION_TYPE_NAME); \
    ptcl_type_comp_type *expression_comp_type = malloc(sizeof(ptcl_type_comp_type));                             \
    if (expression_comp_type == NULL)                                                                            \
        goto out_of_memory;                                                                                      \
    *expression_comp_type = ptcl_comp_type_builder_build_type(&expression_builder);                              \
    ptcl_type expression_type = ptcl_type_create_comp_type_t(expression_comp_type);                              \
    ptcl_parser_add_instance_comp_type(parser, ptcl_comp_type_builder_build(&expression_builder, expression_type.comp_type))

#define CREATE_GET_STATEMENTS_FUNC()                                                                              \
    ptcl_func_built_in_builder get_statements_builder = ptcl_func_built_in_builder_create("ptcl_get_statements"); \
    ptcl_type array_statements = ptcl_type_create_array(&statement_type, -1);                                     \
    array_statements.array.target->is_static = false;                                                             \
    ptcl_func_built_in_builder_bind(&get_statements_builder, ptcl_get_statements_realization);                    \
    ptcl_func_built_in_builder_add_argument(&get_statements_builder, static_any);                                 \
    ptcl_func_built_in_builder_return(&get_statements_builder, array_statements);                                 \
    ptcl_parser_add_instance_function(parser, ptcl_func_built_in_builder_build(&get_statements_builder))

#define CREATE_ERROR_FUNC()                                                                                    \
    ptcl_func_built_in_builder error_builder = ptcl_func_built_in_builder_create(PTCL_PARSER_ERROR_FUNC_NAME); \
    ptcl_type string_type = ptcl_type_create_array(&ptcl_type_character, -1);                                  \
    string_type.is_static = true;                                                                              \
    ptcl_func_built_in_builder_bind(&error_builder, ptcl_error_realization);                                   \
    ptcl_func_built_in_builder_add_argument(&error_builder, string_type);                                      \
    ptcl_func_built_in_builder_return(&error_builder, ptcl_type_void);                                         \
    ptcl_parser_add_instance_function(parser, ptcl_func_built_in_builder_build(&error_builder))

#define CREATE_DEFINED_FUNC()                                                                                    \
    ptcl_func_built_in_builder defined_builder = ptcl_func_built_in_builder_create("ptcl_defined");              \
    ptcl_func_built_in_builder_bind(&defined_builder, ptcl_defined_realization);                                 \
    ptcl_func_built_in_builder_add_argument(&defined_builder, ptcl_type_create_array(&ptcl_type_character, -1)); \
    ptcl_func_built_in_builder_return(&defined_builder, ptcl_type_integer);                                      \
    ptcl_parser_add_instance_function(parser, ptcl_func_built_in_builder_build(&defined_builder))

#define CREATE_INSERT_FUNC()                                                                      \
    ptcl_func_built_in_builder insert_builder = ptcl_func_built_in_builder_create("ptcl_insert"); \
    ptcl_func_built_in_builder_bind(&insert_builder, ptcl_insert_realization);                    \
    ptcl_func_built_in_builder_add_argument(&insert_builder, static_any);                         \
    ptcl_func_built_in_builder_return(&insert_builder, ptcl_type_integer);                        \
    ptcl_parser_add_instance_function(parser, ptcl_func_built_in_builder_build(&insert_builder))

#define CLEANUP_BUILDERS()                                       \
    ptcl_func_built_in_builder_destroy(&get_statements_builder); \
    ptcl_func_built_in_builder_destroy(&defined_builder);        \
    ptcl_func_built_in_builder_destroy(&insert_builder);         \
    ptcl_func_built_in_builder_destroy(&error_builder);          \
    ptcl_comp_type_builder_destroy(&token_builder);              \
    ptcl_comp_type_builder_destroy(&statement_builder);          \
    ptcl_comp_type_builder_destroy(&expression_builder)

typedef struct ptcl_parser
{
    ptcl_interpreter *interpreter;
    ptcl_lexer_configuration *configuration;
    ptcl_tokens_list *input;
    ptcl_parser_tokens_state state;
    ptcl_parser_insert_state insert_states[PTCL_PARSER_DECL_INSERT_DEPTH];
    size_t insert_states_count;
    ptcl_parser_error *errors;
    size_t errors_count;
    ptcl_parser_syntax *syntaxes;
    size_t syntaxes_count;
    size_t syntaxes_capacity;
    ptcl_parser_typedata *typedatas;
    size_t typedatas_count;
    size_t typedatas_capacity;
    ptcl_parser_comp_type *comp_types;
    size_t comp_types_count;
    size_t comp_types_capacity;
    ptcl_parser_function *functions;
    size_t functions_count;
    size_t functions_capacity;
    ptcl_parser_variable *variables;
    size_t variables_count;
    size_t variables_capacity;
    ptcl_statement_func_body *root;
    ptcl_statement_func_body *main_root;
    ptcl_statement_func_body *inserted_body;
    ptcl_type *return_type;
    ptcl_parser_syntax_pair syntaxes_nodes[PTCL_PARSER_MAX_DEPTH];
    size_t syntax_depth;
    bool is_critical;
    bool add_errors;
    bool is_syntax_body;
    bool is_type_body;
    ptcl_parser_tokens_state *lated_states;
    size_t lated_states_count;
    size_t lated_states_capacity;
    ptcl_parser_this_s_pair *this_pairs;
    size_t this_pairs_count;
    bool is_ignore_error;
} ptcl_parser;

typedef struct ptcl_parse_arguments_result
{
    ptcl_argument *arguments;
    size_t count;
    bool has_variadic;
} ptcl_parse_arguments_result;

static ptcl_token *ptcl_parser_tokens_from_array(ptcl_expression *expression)
{
    ptcl_token *expression_tokens = malloc(expression->array.count * sizeof(ptcl_token));
    if (expression_tokens == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < expression->array.count; i++)
    {
        expression_tokens[i] = expression->array.expressions[i]->internal_token;
    }

    return expression_tokens;
}

static bool ptcl_parser_try_parse_insert(ptcl_parser *parser, ptcl_parser_tokens_state *state, ptcl_location location)
{
    ptcl_token name = ptcl_parser_current(parser);
    if ((name.value == ptcl_insert_name.value || strcmp(name.value, ptcl_insert_name.value)) != 0 || ptcl_parser_peek(parser, 1).type != ptcl_token_left_par_type)
    {
        return false;
    }

    ptcl_parser_skip(parser);
    ptcl_parser_skip(parser);
    ptcl_type array = ptcl_type_create_array(&ptcl_token_t_type, -1);
    ptcl_expression *expression = ptcl_parser_cast(parser, &array, false);
    if (ptcl_parser_critical(parser))
    {
        return false;
    }

    expression->is_original = false;
    ptcl_parser_except(parser, ptcl_token_right_par_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(expression);
        return false;
    }

    ptcl_token *expression_tokens = ptcl_parser_tokens_from_array(expression);
    if (ptcl_parser_critical(parser))
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    state->tokens = expression_tokens;
    state->count = expression->array.count;
    state->position = 0;
    expression->is_original = false;
    ptcl_expression_destroy(expression);
    return true;
}

static void ptcl_parse_arguments_result_destroy(ptcl_parse_arguments_result result)
{
    for (size_t i = 0; i < result.count; i++)
    {
        ptcl_argument_destroy(result.arguments[i]);
    }

    free(result.arguments);
}

static ptcl_parse_arguments_result ptcl_parser_parse_arguments(ptcl_parser *parser, ptcl_location location, bool with_names, bool with_variadic)
{
    ptcl_parse_arguments_result result = {
        .arguments = NULL,
        .count = 0,
        .has_variadic = false};
    bool is_tokens = false;
    size_t start = ptcl_parser_insert_states_count(parser);
    ptcl_parser_insert_state *start_state = ptcl_parser_insert_state_at(parser, start);
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            if (is_tokens)
            {
                ptcl_parser_leave_from_insert_state(parser);
                if (ptcl_parser_insert_states_count(parser) == 0)
                {
                    is_tokens = false;
                }

                continue;
            }

            ptcl_parse_arguments_result_destroy(result);
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            break;
        }

        ptcl_parser_tokens_state temp_state;
        bool is_toggle = ptcl_parser_try_parse_insert(parser, &temp_state, ptcl_parser_current(parser).location);
        if (ptcl_parser_critical(parser))
        {
            goto leave;
        }

        if (ptcl_parser_insert_states_count(parser) >= PTCL_PARSER_DECL_INSERT_DEPTH)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            goto leave;
        }

        if (is_toggle)
        {
            is_tokens = true;
            start_state->state = parser->state;
            start_state->syntax_depth = parser->syntax_depth;
            ptcl_parser_insert_state *state = ptcl_parser_insert_state_at(parser, ++parser->insert_states_count);
            state->state = temp_state;
            state->syntax_depth = parser->syntax_depth;
            ptcl_parser_set_state(parser, temp_state);
            continue;
        }

        ptcl_argument *buffer = realloc(result.arguments, (result.count + 1) * sizeof(ptcl_argument));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        leave:
            ptcl_parse_arguments_result_destroy(result);
            if (ptcl_parser_insert_states_count(parser) > 1)
            {
                for (size_t i = start + 1; i < ptcl_parser_insert_states_count(parser); i++)
                {
                    free(ptcl_parser_insert_state_at(parser, i)->state.tokens);
                }

                free(parser->state.tokens);
                ptcl_parser_set_state(parser, ptcl_parser_insert_state_at(parser, 0)->state);
            }

            break;
        }

        result.arguments = buffer;
        if (ptcl_parser_match(parser, ptcl_token_elipsis_type))
        {
            if (result.count == 0)
            {
                ptcl_parser_throw_except_type_specifier(parser, location);
                goto leave;
            }
            else if (!with_variadic)
            {
                ptcl_parser_throw_not_allowed_token(parser, "...", ptcl_parser_current(parser).location);
                goto leave;
            }

            result.arguments[result.count++] = ptcl_argument_create_variadic();
            result.has_variadic = true;

            ptcl_parser_except(parser, ptcl_token_right_par_type);
            if (ptcl_parser_critical(parser))
            {
                goto leave;
            }

            break;
        }
        else
        {
            ptcl_name argument_name = ptcl_name_create_fast_w(NULL, false);
            if (with_names)
            {
                argument_name = ptcl_parser_name_word(parser);
                if (ptcl_parser_critical(parser))
                {
                    goto leave;
                }

                ptcl_parser_except(parser, ptcl_token_colon_type);
                if (ptcl_parser_critical(parser))
                {
                    goto leave;
                }
            }

            ptcl_type type = ptcl_parser_type(parser, false, false);
            if (ptcl_parser_critical(parser))
            {
                goto leave;
            }

            result.arguments = buffer;
            result.arguments[result.count++] = ptcl_argument_create(type, argument_name);
        }

        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    return result;
}

static ptcl_type ptcl_parser_pointers(ptcl_parser *parser, ptcl_type type)
{
    if (ptcl_parser_ended(parser))
    {
        return type;
    }

    if (ptcl_parser_match(parser, ptcl_token_asterisk_type))
    {
        if (type.is_static)
        {
            ptcl_type excepted = type;
            excepted.is_static = false;
            ptcl_parser_throw_fast_incorrect_type(parser, excepted, type, ptcl_parser_current(parser).location);
            ptcl_type_destroy(type);
            return type;
        }

        ptcl_type *target = malloc(sizeof(ptcl_type));
        if (target == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_type_destroy(type);
            return type;
        }

        *target = type;
        target->is_primitive = false;
        target->is_static = false;
        type = ptcl_type_create_pointer(target);
        return ptcl_parser_pointers(parser, type);
    }
    else if (ptcl_parser_match(parser, ptcl_token_left_square_type))
    {
        if (!type.is_static)
        {
            ptcl_type excepted = type;
            excepted.is_static = true;
            ptcl_parser_throw_fast_incorrect_type(parser, excepted, type, ptcl_parser_current(parser).location);
            ptcl_type_destroy(type);
            return type;
        }

        ptcl_parser_except(parser, ptcl_token_right_square_type);
        if (ptcl_parser_critical(parser))
        {
            ptcl_type_destroy(type);
            return type;
        }

        ptcl_type *target = malloc(sizeof(ptcl_type));
        if (target == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_type_destroy(type);
            return type;
        }

        *target = type;
        target->is_static = false;
        target->is_primitive = false;
        type = ptcl_type_create_array(target, -1);
        type.is_static = true;
        return ptcl_parser_pointers(parser, type);
    }

    return type;
}

static void ptcl_parser_skip_block_or_expression(ptcl_parser *parser)
{
    ptcl_location location = ptcl_parser_current(parser).location;
    size_t depth = 1;
    while (depth > 0 && !ptcl_parser_ended(parser))
    {
        ptcl_token current = ptcl_parser_current(parser);
        if (current.type == ptcl_token_left_curly_type)
        {
            depth++;
        }
        else if (current.type == ptcl_token_right_curly_type)
        {
            depth--;
        }

        ptcl_parser_skip(parser);
    }

    if (depth > 0)
    {
        ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_curly_type), location);
    }
}

static void ptcl_parser_set_arrays_length(ptcl_type *type, ptcl_type *target)
{
    if (type->type == ptcl_value_array_type)
    {
        type->array.count = target->array.count;
        ptcl_parser_set_arrays_length(type->array.target, target->array.target);
    }
}

static ptcl_statement *ptcl_create_statement_from_expression(ptcl_statement_func_body *root, ptcl_expression *expression, ptcl_location location)
{
    ptcl_statement *statement = ptcl_statement_create(
        expression->internal_statement->type,
        expression->internal_statement->root,
        ptcl_attributes_create(NULL, 0),
        location);
    if (statement == NULL)
    {
        return NULL;
    }

    *statement = *expression->internal_statement;
    statement->root = root;
    return statement;
}

static ptcl_expression *ptcl_create_expression_from_statement(ptcl_statement *statement, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(
        ptcl_expression_in_statement_type,
        ptcl_statement_t_type,
        location);
    if (expression == NULL)
    {
        return NULL;
    }

    if (statement->is_original)
    {
        ptcl_statement *copy = malloc(sizeof(ptcl_statement));
        if (copy == NULL)
        {
            free(expression);
            return NULL;
        }

        *copy = *statement;
        statement->is_original = false;
        statement = copy;
        statement->is_original = true;
    }

    expression->internal_statement = statement;
    return expression;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
static ptcl_expression *ptcl_error_realization(ptcl_parser *parser, ptcl_expression **arguments, size_t count, ptcl_location location, bool is_expression)
{
    if (parser->is_ignore_error)
    {
        PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
        return NULL;
    }

    char *message = ptcl_string_from_array(arguments[0]->array);
    PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
    if (message == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    ptcl_parser_throw_user(parser, message, location);
    return NULL;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
static ptcl_expression *ptcl_defined_realization(ptcl_parser *parser, ptcl_expression **arguments, size_t count, ptcl_location location, bool is_expression)
{
    ptcl_expression *name_argument = arguments[0];
    ptcl_name name = ptcl_name_create_fast_w(ptcl_string_from_array(name_argument->array), false);
    if (name.value == NULL)
    {
        PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    bool is_defined = ptcl_parser_is_defined(parser, name);
    free(name.value);
    PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
    return ptcl_expression_create_integer(is_defined, location);
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
static ptcl_expression *ptcl_get_statements_realization(ptcl_parser *parser, ptcl_expression **arguments, size_t count, ptcl_location location, bool is_expression)
{
    ptcl_expression *argument = arguments[0];
    ptcl_type type = argument->return_type;
    ptcl_type array_type = ptcl_type_create_array(&ptcl_statement_t_type, 0);
    switch (type.type)
    {
    case ptcl_value_function_pointer_type:
    {
        ptcl_parser_tokens_state lated_body = parser->lated_states[argument->lated_body.index];
        const ptcl_parser_tokens_state last_tokens_state = parser->state;
        ptcl_parser_set_state(parser, lated_body);
        ptcl_parser_set_position(parser, 0);
        const ptcl_statement_func_body *last_state = parser->inserted_body;
        ptcl_statement_func_body body = ptcl_statement_func_body_create(
            NULL, 0,
            ptcl_parser_root(parser));
        parser->inserted_body = &body;
        ptcl_parser_func_body_by_pointer(parser, parser->inserted_body, true, false, parser->is_ignore_error);
        ptcl_parser_set_state(parser, last_tokens_state);
        parser->inserted_body = last_state;
        if (ptcl_parser_critical(parser))
        {
            PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
            return NULL;
        }

        ptcl_expression **statements = malloc(body.count * sizeof(ptcl_expression *));
        if (statements == NULL)
        {
            ptcl_statement_func_body_destroy(body);
            PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
            return NULL;
        }

        ptcl_expression *array = ptcl_expression_create(ptcl_expression_array_type, array_type, location);
        if (array == NULL)
        {
            free(statements);
            ptcl_statement_func_body_destroy(body);
            PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
            return NULL;
        }

        for (size_t i = 0; i < body.count; i++)
        {
            ptcl_expression *element = ptcl_create_expression_from_statement(body.statements[i], location);
            if (element == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                ptcl_statement_func_body_destroy(body);
                for (size_t j = 0; j < i; j++)
                {
                    free(statements[j]);
                }

                free(statements);
                PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
                return NULL;
            }

            statements[i] = element;
        }

        PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
        array->array.type = ptcl_statement_t_type;
        array->array.expressions = statements;
        array->array.count = body.count;
        ptcl_statement_func_body_destroy(body);
        return array;
    }
    default:
        ptcl_parser_throw_fast_incorrect_type(
            parser,
            array_type,
            argument->return_type, location);
        PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
        return NULL;
    }
}

static bool ptcl_check_statements(ptcl_parser *parser, ptcl_statement_func_body func_body)
{
    for (size_t i = 0; i < func_body.count; i++)
    {
        ptcl_statement *statement = func_body.statements[i];
        if (statement->type == ptcl_statement_if_type)
        {
            if (!ptcl_check_statements(parser, statement->if_stat.body))
            {
                return false;
            }

            if (statement->if_stat.with_else && !ptcl_check_statements(parser, statement->if_stat.else_body))
            {
                return false;
            }
        }
        else if (statement->type == ptcl_statement_return_type)
        {
            ptcl_type type = statement->ret.value == NULL ? ptcl_type_void : statement->ret.value->return_type;
            if (!ptcl_type_equals(*parser->return_type, type))
            {
                ptcl_parser_throw_fast_incorrect_type(parser, *parser->return_type, type, statement->location);
                return false;
            }
        }
    }

    return true;
}

static bool ptcl_check_array_statements(ptcl_parser *parser, ptcl_expression_array array)
{
    for (size_t i = 0; i < array.count; i++)
    {
        ptcl_expression *expression = array.expressions[i];
        if (expression->type != ptcl_expression_in_statement_type)
        {
            continue;
        }

        ptcl_statement *statement = expression->internal_statement;
        if (statement->type == ptcl_statement_if_type)
        {
            if (!ptcl_check_statements(parser, statement->if_stat.body))
            {
                return false;
            }

            if (statement->if_stat.with_else && !ptcl_check_statements(parser, statement->if_stat.else_body))
            {
                return false;
            }
        }
        else if (statement->type == ptcl_statement_return_type)
        {
            ptcl_type type = statement->ret.value == NULL ? ptcl_type_void : statement->ret.value->return_type;
            if (!ptcl_type_equals(*parser->return_type, type))
            {
                ptcl_parser_throw_fast_incorrect_type(parser, *parser->return_type, type, statement->location);
                return false;
            }
        }
    }

    return true;
}

static ptcl_expression *ptcl_insert_realization(ptcl_parser *parser, ptcl_expression **arguments, size_t count, ptcl_location location, bool is_expression)
{
    ptcl_expression *argument = arguments[0];
    ptcl_type type = argument->return_type;
    ptcl_type array_type = ptcl_type_create_array(&ptcl_statement_t_type, 0);
    if (is_expression)
    {
        if (type.type == ptcl_value_array_type)
        {
            ptcl_type target = *type.array.target;
            if (target.type == ptcl_value_type_type && ptcl_name_compare(target.comp_type->identifier, ptcl_token_t_name))
            {
                goto tokens;
            }
        }

        PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
        ptcl_parser_throw_unknown_expression(parser, location);
        return NULL;
    }

    switch (type.type)
    {
    case ptcl_value_array_type:
    {
        ptcl_type target = *type.array.target;
        if (target.type != ptcl_value_type_type)
        {
            PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
            ptcl_parser_throw_fast_incorrect_type(parser, array_type, type, location);
            return NULL;
        }

        ptcl_expression_array array = argument->array;
        if (ptcl_name_compare(target.comp_type->identifier, ptcl_statement_t_name))
        {
            if (!ptcl_check_array_statements(parser, array))
            {
                PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
                return NULL;
            }

            ptcl_statement_func_body *body = parser->syntax_depth > 0 ? parser->main_root : ptcl_parser_root(parser);
            ptcl_statement **buffer = realloc(body->statements, (body->count + array.count) * sizeof(ptcl_statement *));
            if (buffer == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
                return NULL;
            }

            body->statements = buffer;
            for (size_t i = 0; i < array.count; i++)
            {
                ptcl_expression *expression = array.expressions[i];
                ptcl_statement *element = ptcl_create_statement_from_expression(ptcl_parser_root(parser), expression, location);
                if (expression->is_original)
                {
                    free(expression->internal_statement);
                    expression->is_original = false;
                    element->is_original = true;
                }

                if (element == NULL)
                {
                    ptcl_parser_throw_out_of_memory(parser, location);
                    PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
                    return NULL;
                }

                body->statements[body->count] = element;
                body->count++;
            }
        }
        else if (ptcl_name_compare(target.comp_type->identifier, ptcl_token_t_name))
        {
        tokens:
            if (is_expression)
            {
                ptcl_token *expression_tokens = ptcl_parser_tokens_from_array(argument);
                if (ptcl_parser_critical(parser))
                {
                    PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
                    ptcl_parser_throw_out_of_memory(parser, location);
                    return NULL;
                }

                const ptcl_parser_tokens_state last_state = parser->state;
                ptcl_parser_set_position(parser, 0);
                parser->state.tokens = expression_tokens;
                parser->state.count = argument->array.count;
                ptcl_expression *value = ptcl_parser_cast(parser, NULL, false);
                parser->state = last_state;
                PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
                free(expression_tokens);
                if (ptcl_parser_critical(parser))
                {
                    ptcl_parser_throw_out_of_memory(parser, location);
                    return NULL;
                }

                return value;
            }
            else
            {
                ptcl_expression *statements = ptcl_get_statements_realization(parser, arguments, 1, location, true);
                if (ptcl_parser_critical(parser))
                {
                    ptcl_parser_throw_out_of_memory(parser, location);
                    return NULL;
                }

                return ptcl_insert_realization(parser, &statements, 1, location, false);
            }
        }

        break;
    }
    default:
        ptcl_parser_throw_fast_incorrect_type(
            parser,
            array_type,
            argument->return_type, location);
        PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
        return NULL;
    }

    PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
    return NULL;
}

ptcl_parser *ptcl_parser_create(ptcl_tokens_list *input, ptcl_lexer_configuration *configuration)
{
    ptcl_parser *parser = malloc(sizeof(ptcl_parser));
    if (parser == NULL)
    {
        return NULL;
    }

    parser->configuration = configuration;
    parser->input = input;
    parser->state.tokens = input->tokens;
    parser->state.count = input->count;
    return parser;
}

static void ptcl_parser_reset(ptcl_parser *parser)
{
    parser->is_ignore_error = true;
    parser->state.position = 0;
    parser->insert_states_count = 0;
    parser->is_critical = false;
    parser->root = NULL;
    parser->main_root = NULL;
    parser->inserted_body = NULL;
    parser->return_type = NULL;
    parser->errors = NULL;
    parser->errors_count = 0;

    parser->syntaxes_count = 0;
    parser->comp_types_count = 0;
    parser->typedatas_count = 0;
    parser->functions_count = 0;
    parser->variables_count = 0;
    parser->lated_states_count = 0;
    parser->this_pairs_count = 0;

    parser->syntaxes_capacity = PTCL_PARSER_DEFAULT_INSTANCE_CAPACITY;
    parser->comp_types_capacity = PTCL_PARSER_DEFAULT_INSTANCE_CAPACITY;
    parser->typedatas_capacity = PTCL_PARSER_DEFAULT_INSTANCE_CAPACITY;
    parser->functions_capacity = PTCL_PARSER_DEFAULT_INSTANCE_CAPACITY;
    parser->variables_capacity = PTCL_PARSER_DEFAULT_INSTANCE_CAPACITY;
    parser->lated_states_capacity = PTCL_PARSER_DEFAULT_INSTANCE_CAPACITY;
    parser->interpreter = ptcl_interpreter_create(parser);
    if (parser->interpreter == NULL)
    {
        goto cleanup;
    }

    parser->syntaxes = malloc(parser->syntaxes_capacity * sizeof(ptcl_parser_syntax));
    if (parser->syntaxes == NULL)
    {
        goto cleanup;
    }

    parser->comp_types = malloc(parser->comp_types_capacity * sizeof(ptcl_parser_comp_type));
    if (parser->comp_types == NULL)
    {
        goto cleanup;
    }

    parser->typedatas = malloc(parser->typedatas_capacity * sizeof(ptcl_parser_typedata));
    if (parser->typedatas == NULL)
        goto cleanup;

    parser->functions = malloc(parser->functions_capacity * sizeof(ptcl_parser_function));
    if (parser->functions == NULL)
    {
        goto cleanup;
    }

    parser->variables = malloc(parser->variables_capacity * sizeof(ptcl_parser_variable));
    if (parser->variables == NULL)
    {
        goto cleanup;
    }

    parser->lated_states = malloc(parser->lated_states_capacity * sizeof(ptcl_parser_tokens_state));
    if (parser->lated_states == NULL)
    {
        goto cleanup;
    }

    parser->this_pairs = NULL;
    parser->syntax_depth = 0;
    parser->add_errors = true;
    parser->is_syntax_body = true;
    parser->is_type_body = false;
    return;
cleanup:
    ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
    if (parser->interpreter != NULL)
    {
        ptcl_interpreter_destroy(parser->interpreter);
    }

    free(parser->syntaxes);
    free(parser->comp_types);
    free(parser->typedatas);
    free(parser->functions);
    free(parser->variables);
    free(parser->lated_states);
}

ptcl_parser_result ptcl_parser_parse(ptcl_parser *parser)
{
    ptcl_parser_reset(parser);
    if (ptcl_parser_critical(parser))
    {
        goto out_of_memory;
    }

    SETUP_STATIC_ANY();
    CREATE_TOKEN_TYPE();
    CREATE_STATEMENT_TYPE();
    CREATE_EXPRESSION_TYPE();
    CREATE_GET_STATEMENTS_FUNC();
    CREATE_DEFINED_FUNC();
    CREATE_ERROR_FUNC();
    CREATE_INSERT_FUNC();

    ptcl_parser_result result = {
        .configuration = parser->configuration,
        .body = ptcl_parser_func_body(parser, false, true, parser->is_ignore_error),
        .errors = parser->errors,
        .errors_count = parser->errors_count,
        .syntaxes = parser->syntaxes,
        .syntaxes_count = parser->syntaxes_count,
        .comp_types = parser->comp_types,
        .comp_types_count = parser->comp_types_count,
        .typedatas = parser->typedatas,
        .typedatas_count = parser->typedatas_count,
        .functions = parser->functions,
        .functions_count = parser->functions_count,
        .variables = parser->variables,
        .variables_count = parser->variables_count,
        .lated_states = parser->lated_states,
        .lated_states_count = parser->lated_states_count,
        .this_pairs = parser->this_pairs,
        .this_pairs_count = parser->this_pairs_count,
        .is_critical = ptcl_parser_critical(parser)};

    goto success;

out_of_memory:
    ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
    result = (ptcl_parser_result){
        .body = ptcl_statement_func_body_create(NULL, 0, NULL),
        .errors_count = 1,
        .errors = parser->errors,
        .syntaxes = parser->syntaxes,
        .syntaxes_count = parser->syntaxes_count,
        .comp_types = parser->comp_types,
        .comp_types_count = parser->comp_types_count,
        .typedatas = parser->typedatas,
        .typedatas_count = parser->typedatas_count,
        .functions = parser->functions,
        .functions_count = parser->functions_count,
        .variables = parser->variables,
        .variables_count = parser->variables_count,
        .lated_states = NULL,
        .lated_states_count = 0,
        .is_critical = ptcl_parser_critical(parser)};

success:
    CLEANUP_BUILDERS();
    return result;
}

ptcl_statement_type ptcl_parser_parse_get_statement(ptcl_parser *parser, bool *is_finded)
{
    ptcl_token current = ptcl_parser_current(parser);
    if (current.type == ptcl_token_tilde_type || current.type == ptcl_token_prototype_type || current.type == ptcl_token_global_type)
    {
        ptcl_parser_skip(parser);
        ptcl_statement_type type = ptcl_parser_parse_get_statement(parser, is_finded);
        ptcl_parser_back(parser);
        return type;
    }

    *is_finded = true;
    switch (current.type)
    {
    case ptcl_token_exclamation_mark_type:
        return ptcl_statement_assign_type;
    case ptcl_token_at_type:
        if (ptcl_parser_peek(parser, 1).type == ptcl_token_left_curly_type)
        {
            return ptcl_statement_func_body_type;
        }

        break;
    case ptcl_token_word_type:
        if (ptcl_parser_peek(parser, 1).type == ptcl_token_left_par_type)
        {
            return ptcl_statement_func_call_type;
        }
        else if (ptcl_parser_peek(parser, 1).type == ptcl_token_dot_type)
        {
            ptcl_parser_skip(parser);
            ptcl_parser_skip(parser);
            ptcl_statement_type type = ptcl_parser_parse_get_statement(parser, is_finded);
            ptcl_parser_back(parser);
            ptcl_parser_back(parser);
            return type;
        }

        return ptcl_statement_assign_type;
    case ptcl_token_caret_type:
        return ptcl_statement_func_call_type;
    case ptcl_token_optional_type:
        return ptcl_statement_type_decl_type;
    case ptcl_token_return_type:
        return ptcl_statement_return_type;
    case ptcl_token_static_type:
        if (ptcl_parser_peek(parser, 1).type == ptcl_token_if_type)
        {
            return ptcl_statement_if_type;
        }

        return ptcl_statement_func_decl_type;
    case ptcl_token_if_type:
        return ptcl_statement_if_type;
    case ptcl_token_each_type:
        return ptcl_statement_each_type;
    case ptcl_token_syntax_type:
        return ptcl_statement_syntax_type;
    case ptcl_token_unsyntax_type:
        return ptcl_statement_unsyntax_type;
    case ptcl_token_undefine_type:
        return ptcl_statement_undefine_type;
    case ptcl_token_type_type:
        return ptcl_statement_type_decl_type;
    case ptcl_token_typedata_type:
        return ptcl_statement_typedata_decl_type;
    case ptcl_token_function_type:
        return ptcl_statement_func_decl_type;
    default:
        break;
    }

    *is_finded = false;
    return ptcl_statement_func_body_type;
}

ptcl_statement *ptcl_parser_parse_statement(ptcl_parser *parser)
{
    if (parser->is_syntax_body && ptcl_parser_parse_try_syntax_usage_here(parser, true))
    {
        size_t depth = parser->syntax_depth;
        ptcl_attributes attributes = ptcl_parser_parse_attributes(parser);
        if (ptcl_parser_critical(parser))
        {
            return NULL;
        }

        ptcl_location location = ptcl_parser_current(parser).location;
        ptcl_statement *statement = NULL;
        if (!parser->is_type_body)
        {
            statement = ptcl_statement_create(ptcl_statement_func_body_type, ptcl_parser_root(parser), attributes, location);
            if (statement == NULL)
            {
                ptcl_attributes_destroy(attributes);
                ptcl_parser_throw_out_of_memory(parser, location);
                return NULL;
            }
        }

        ptcl_statement_func_body body = ptcl_statement_func_body_create(
            NULL, 0,
            parser->inserted_body == NULL                      ? parser->main_root
            : parser->inserted_body->root == parser->main_root ? parser->inserted_body
                                                               : parser->main_root);
        ptcl_parser_func_body_by_pointer(parser, &body, false, false, parser->is_ignore_error);
        if (parser->syntax_depth == depth)
        {
            ptcl_parser_leave_from_syntax(parser);
        }

        if (ptcl_parser_critical(parser))
        {
            free(statement);
            ptcl_attributes_destroy(attributes);
            return NULL;
        }

        if (parser->is_type_body)
        {
            ptcl_statement_func_body *previous = ptcl_parser_root(parser);
            parser->root = body.root;

            ptcl_statement_func_body *root_body = ptcl_parser_root(parser);
            size_t new_count = root_body->count + body.count;
            ptcl_statement **new_statements = realloc(root_body->statements,
                                                      sizeof(ptcl_statement *) * new_count);
            if (new_statements == NULL)
            {
                ptcl_statement_func_body_destroy(body);
                ptcl_parser_throw_out_of_memory(parser, location);
                parser->root = previous;
                return NULL;
            }

            for (size_t i = 0; i < body.count; i++)
            {
                new_statements[i + root_body->count] = body.statements[i];
            }

            free(body.statements);
            root_body->statements = new_statements;
            root_body->count = new_count;
            parser->root = previous;
        }
        else
        {
            statement->body = body;
        }

        return statement;
    }

    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    ptcl_attributes attributes = ptcl_parser_parse_attributes(parser);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    ptcl_statement *statement = NULL;
    ptcl_location location = ptcl_parser_current(parser).location;
    bool is_finded;
    ptcl_statement_type type = ptcl_parser_parse_get_statement(parser, &is_finded);
    if (is_finded)
    {
        statement = ptcl_statement_create(type, ptcl_parser_root(parser), attributes, location);
        if (statement == NULL)
        {
            ptcl_attributes_destroy(attributes);
            ptcl_parser_throw_out_of_memory(parser, location);
            return NULL;
        }

        switch (type)
        {
        case ptcl_statement_func_call_type:
        {
            bool with_injection = ptcl_parser_match(parser, ptcl_token_caret_type);
            size_t start = ptcl_parser_position(parser);
            ptcl_name name = ptcl_parser_name(parser, false);
            if (ptcl_parser_critical(parser))
            {
                ptcl_attributes_destroy(attributes);
                free(statement);
                return NULL;
            }

            ptcl_identifier identifier;
            if (ptcl_parser_current(parser).type != ptcl_token_left_par_type)
            {
                // TODO: avoid reparsing
                ptcl_parser_set_position(parser, start);
                ptcl_name_destroy(name);
                ptcl_expression *expression = ptcl_parser_dot(parser, NULL, NULL, false, false);
                if (ptcl_parser_critical(parser))
                {
                    ptcl_attributes_destroy(attributes);
                    free(statement);
                    ptcl_name_destroy(name);
                    return NULL;
                }

                if (expression->return_type.is_static)
                {
                    ptcl_attributes_destroy(attributes);
                    free(statement);
                    return NULL;
                }

                identifier = ptcl_identifier_create_by_expr(expression);
                statement->func_call = ptcl_statement_func_call_create(expression->dot.right->func_call.func_decl, identifier, NULL, 0);
            }
            else
            {
                ptcl_parser_function *function;
                if (!ptcl_parser_try_get_function(parser, name, &function))
                {
                    ptcl_parser_throw_unknown_function(parser, name.value, location);
                    ptcl_attributes_destroy(attributes);
                    free(statement);
                    ptcl_name_destroy(name);
                    return NULL;
                }

                ptcl_parser_function placeholder = *function;
                statement->func_call = ptcl_parser_func_call(parser, &placeholder, NULL, false);
                if (statement->func_call.is_built_in)
                {
                    if (ptcl_parser_critical(parser))
                    {
                        ptcl_attributes_destroy(attributes);
                        free(statement);
                        return NULL;
                    }

                    if (statement->func_call.built_in != NULL)
                    {
                        ptcl_expression_destroy(statement->func_call.built_in);
                    }

                    ptcl_attributes_destroy(attributes);
                    free(statement);
                    statement = NULL;
                }
                else if (with_injection)
                {
                    ptcl_parser_insert_pairs(parser, statement, function->func.func_body);
                    if (ptcl_parser_critical(parser))
                    {
                        ptcl_statement_destroy(statement);
                        return NULL;
                    }
                }
            }

            break;
        }
        case ptcl_statement_func_decl_type:
            statement->func_decl = ptcl_parser_func_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type), ptcl_parser_match(parser, ptcl_token_global_type), ptcl_parser_match(parser, ptcl_token_static_type));
            break;
        case ptcl_statement_typedata_decl_type:
            statement->typedata_decl = ptcl_parser_typedata_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type), ptcl_parser_match(parser, ptcl_token_global_type));
            break;
        case ptcl_statement_type_decl_type:
            statement->type_decl = ptcl_parser_type_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type), ptcl_parser_match(parser, ptcl_token_global_type));
            break;
        case ptcl_statement_assign_type:
            statement->assign = ptcl_parser_assign(parser, ptcl_parser_match(parser, ptcl_token_global_type));

            // By dot
            if (!parser->is_critical && !statement->assign.identifier.is_name && statement->assign.identifier.value->return_type.is_static)
            {
                ptcl_expression_destroy(statement->assign.identifier.value);
                ptcl_attributes_destroy(attributes);
                free(statement);
                return NULL;
            }

            break;
        case ptcl_statement_return_type:
            statement->ret = ptcl_parser_return(parser);
            break;
        case ptcl_statement_if_type:
            free(statement);
            statement = ptcl_parser_if(parser, ptcl_parser_match(parser, ptcl_token_static_type));
            break;
        case ptcl_statement_each_type:
            ptcl_attributes_destroy(attributes);
            ptcl_parser_each(parser);
            break;
        case ptcl_statement_syntax_type:
            ptcl_attributes_destroy(attributes);
            free(statement);
            statement = NULL;
            ptcl_parser_syntax_decl(parser);
            break;
        case ptcl_statement_unsyntax_type:
            statement->type = ptcl_statement_func_body_type;
            statement->body = ptcl_parser_unsyntax(parser);
            break;
        case ptcl_statement_undefine_type:
            ptcl_attributes_destroy(attributes);
            free(statement);
            statement = NULL;
            ptcl_parser_undefine(parser);
            break;
        case ptcl_statement_func_body_type:
            ptcl_attributes_destroy(attributes);
            free(statement);
            statement = NULL;
            // If we in syntax, then it was already parsed
            if (parser->syntax_depth == 0)
            {
                ptcl_parser_extra_body(parser, false);
            }
            else
            {
                ptcl_parser_skip(parser); // Skip '@'
                ptcl_parser_skip(parser); // Skip '{'
                ptcl_parser_skip_block_or_expression(parser);
            }

            break;
        case ptcl_statement_import_type:
            break;
        }
    }
    else
    {
        ptcl_parser_throw_unknown_statement(parser, location);
    }

    if (ptcl_parser_critical(parser))
    {
        if (statement != NULL)
        {
            ptcl_attributes_destroy(attributes);
            free(statement);
        }

        return NULL;
    }

    // Syntax decl, body, static func call or each
    if (statement == NULL)
    {
        return NULL;
    }

    if (parser->is_type_body && type != ptcl_statement_func_decl_type)
    {
        ptcl_attributes_destroy(attributes);
        ptcl_parser_except(parser, ptcl_token_function_type);
        return NULL;
    }

    statement->root = ptcl_parser_root(parser);
    statement->attributes = attributes;
    return statement;
}

ptcl_attributes ptcl_parser_parse_attributes(ptcl_parser *parser)
{
    ptcl_attributes attributes = ptcl_attributes_create(NULL, 0);
    while (ptcl_parser_current(parser).type == ptcl_token_at_type && ptcl_parser_peek(parser, 1).type == ptcl_token_left_square_type)
    {
        ptcl_parser_skip(parser);
        ptcl_parser_skip(parser);
        ptcl_name name = ptcl_parser_name_word(parser);
        if (ptcl_parser_critical(parser))
        {
            return (ptcl_attributes){0};
        }

        ptcl_parser_except(parser, ptcl_token_right_square_type);
        if (ptcl_parser_critical(parser))
        {
            ptcl_name_destroy(name);
            return (ptcl_attributes){0};
        }

        ptcl_attribute *buffer = realloc(attributes.attributes, (attributes.count + 1) * sizeof(ptcl_attribute));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_attributes_destroy(attributes);
            return (ptcl_attributes){0};
        }

        attributes.attributes = buffer;
        attributes.attributes[attributes.count++] = ptcl_attribute_create(name);
    }

    return attributes;
}

bool ptcl_parser_parse_try_syntax_usage_here(ptcl_parser *parser, bool is_statement)
{
    return ptcl_parser_parse_try_syntax_usage(parser, NULL, 0, -1, false, is_statement);
}

static ptcl_expression *ptcl_parser_syntax_tokens(ptcl_parser *parser, char *end_token, ptcl_location location)
{
    size_t original_capacity = 8;
    size_t original_count = 0;
    ptcl_expression **buffer = malloc(original_capacity * sizeof(ptcl_expression *));
    if (buffer == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        return false;
    }

    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_parser_throw_except_token(parser, end_token, location);
            return false;
        }

        ptcl_token current = ptcl_parser_current(parser);
        ptcl_parser_skip(parser);
        if ((current.value == end_token || strcmp(current.value, end_token) != 0))
        {
            if (original_count >= original_capacity)
            {
                original_capacity *= 2;
                ptcl_expression **reallocated = realloc(buffer, original_capacity * sizeof(ptcl_expression *));
                if (reallocated == NULL)
                {
                destroying:
                    for (size_t i = 0; i < original_count; i++)
                    {
                        ptcl_expression_destroy(buffer[i]);
                    }

                    free(buffer);
                    ptcl_parser_throw_out_of_memory(parser, location);
                    return false;
                }

                buffer = reallocated;
            }

            ptcl_expression *expression = ptcl_expression_create_token(current);
            if (expression == NULL)
            {
                goto destroying;
            }

            buffer[original_count++] = expression;
            continue;
        }

        ptcl_parser_back(parser);
        break;
    }

    ptcl_expression *expression = ptcl_expression_create_array(
        ptcl_type_create_array(&ptcl_token_t_type, original_count),
        buffer,
        location);
    if (expression == NULL)
    {
        goto destroying;
    }

    return expression;
}

bool ptcl_parser_parse_try_syntax_usage(
    ptcl_parser *parser, ptcl_parser_syntax_node **nodes, size_t count, int down_start, bool skip_first, bool is_statement)
{
    const bool is_root = down_start < 0;
    if (is_root && !ptcl_parser_match(parser, ptcl_token_hashtag_type))
    {
        return false;
    }

    size_t stop = ptcl_parser_position(parser);
    const size_t start = is_root ? stop : (size_t)down_start;
    const size_t original_count = count;

    ptcl_parser_syntax syntax = ptcl_parser_syntax_create(
        ptcl_name_create_fast_w("", false),
        ptcl_parser_root(parser),
        nodes ? *nodes : NULL,
        count, 0);
    ptcl_parser_syntax result;
    bool found = false;
    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            void *placeholder = NULL;
            ptcl_parser_syntax *target = NULL;
            bool current_found = ptcl_parser_syntax_try_find(parser, syntax.nodes, syntax.count, &target, &placeholder, &placeholder);
            if (current_found)
            {
                stop = ptcl_parser_position(parser);
                result = *target;
                found = true;
            }

            break;
        }

        if (!skip_first)
        {
            ptcl_token current = ptcl_parser_current(parser);
            ptcl_parser_syntax_node *buffer = realloc(syntax.nodes, (syntax.count + 1) * sizeof(ptcl_parser_syntax_node));
            if (buffer == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, current.location);
                ptcl_parser_syntax_destroy(syntax);
                ptcl_parser_set_position(parser, start);
                break;
            }

            syntax.nodes = buffer;
            const size_t position = ptcl_parser_position(parser);
            ptcl_parser_syntax_node node = ptcl_parser_syntax_node_create_word(
                current.type,
                ptcl_name_create(current.value, false, current.location));
            ptcl_parser_skip(parser);
            syntax.nodes[syntax.count++] = node;
            if (ptcl_parser_parse_try_syntax_usage(parser, &syntax.nodes, syntax.count, start, true, is_statement))
            {
                return true;
            }

            if (ptcl_parser_critical(parser))
            {
                // Already destroyed in func below
                return false;
            }

            ptcl_parser_set_position(parser, position);
            const bool last_mode = parser->add_errors;
            parser->add_errors = false;
            ptcl_expression *value = ptcl_parser_cast(parser, NULL, true);
            parser->add_errors = last_mode;
            if (ptcl_parser_critical(parser))
            {
                parser->is_critical = false;
                break;
            }
            else
            {
                node = ptcl_parser_syntax_node_create_value(value);
                syntax.nodes[syntax.count - 1] = node;
            }
        }
        else
        {
            skip_first = false;
        }

        // Try to find matching syntax
        ptcl_parser_syntax *target = NULL;
        bool can_continue = false;
        char *end_token = NULL;
        bool current_found = ptcl_parser_syntax_try_find(parser, syntax.nodes, syntax.count, &target, &can_continue, &end_token);
        if (current_found)
        {
            stop = ptcl_parser_position(parser);
            result = *target;
            found = true;
        }

        if (!can_continue)
        {
            break;
        }

        if (end_token != NULL)
        {
            ptcl_location location = ptcl_parser_current(parser).location;
            ptcl_parser_syntax_node *nodes = realloc(syntax.nodes, (syntax.count + 1) * sizeof(ptcl_parser_syntax_node));
            if (nodes == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                ptcl_parser_syntax_destroy(syntax);
                return false;
            }

            syntax.nodes = nodes;
            ptcl_expression *expression = ptcl_parser_syntax_tokens(parser, end_token, location);
            if (ptcl_parser_critical(parser))
            {
                ptcl_parser_syntax_destroy(syntax);
                return false;
            }

            syntax.nodes[syntax.count++] = ptcl_parser_syntax_node_create_value(expression);
        }
    }

    if (found)
    {
        // Handle syntax depth and memory limits
        if (parser->syntax_depth == 0)
        {
            parser->main_root = ptcl_parser_root(parser);
        }

        if (parser->syntax_depth + 1 >= PTCL_PARSER_MAX_DEPTH)
        {
            ptcl_parser_throw_max_depth(parser, ptcl_parser_current(parser).location);
            ptcl_parser_syntax_destroy(syntax);
            return false;
        }

        // Create new function body context
        ptcl_statement_func_body *temp = malloc(sizeof(ptcl_statement_func_body));
        if (temp == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_parser_syntax_destroy(syntax);
            return false;
        }

        *temp = ptcl_statement_func_body_create(NULL, 0, parser->main_root);
        parser->root = temp;

        // Process syntax variables
        for (size_t i = 0; i < result.count; i++)
        {
            ptcl_parser_syntax_node syntax_node = result.nodes[i];
            ptcl_parser_syntax_node target_node = syntax.nodes[i];
            if (syntax_node.type != ptcl_parser_syntax_node_variable_type)
            {
                ptcl_parser_syntax_node_destroy(target_node);
                continue;
            }

            ptcl_expression *expression = target_node.value;
            ptcl_parser_variable variable = ptcl_parser_variable_create(
                ptcl_name_create_fast_w(syntax_node.variable.name, false),
                syntax_node.variable.type,
                expression,
                true,
                ptcl_parser_root(parser));
            variable.is_syntax_variable = true;
            variable.is_built_in = true;
            variable.built_in = expression;
            if (expression->return_type.is_static && expression->return_type.type == ptcl_value_word_type)
            {
                variable.built_in->word.value = variable.built_in->word.value;
                variable.is_syntax_word = true;
            }

            if (!ptcl_parser_add_instance_variable(parser, variable))
            {
                ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
                ptcl_parser_syntax_destroy(syntax);
                return false;
            }
        }

        if (ptcl_parser_critical(parser))
        {
            ptcl_parser_syntax_destroy(syntax);
            return false;
        }

        ptcl_parser_set_position(parser, stop);
        size_t current = parser->syntax_depth++;
        parser->syntaxes_nodes[current] = ptcl_parser_syntax_pair_create(
            syntax, parser->state, temp,
            parser->syntax_depth == 1 ? temp->root : parser->syntaxes_nodes[current - 1].body);
        ptcl_parser_tokens_state lated_body = parser->lated_states[result.index];
        ptcl_parser_set_state(parser, lated_body);
        ptcl_parser_set_position(parser, 0);
    }
    else
    {
        ptcl_parser_set_position(parser, start);
        if (is_root)
        {
            ptcl_parser_throw_unknown_syntax(parser, syntax, ptcl_parser_tokens(parser)[start].location);
            ptcl_parser_syntax_destroy(syntax);
        }
        else
        {
            *nodes = syntax.nodes;
            for (size_t i = original_count; i < syntax.count; i++)
            {
                ptcl_parser_syntax_node_destroy(syntax.nodes[i]);
            }
        }
    }

    return found;
}

void ptcl_parser_leave_from_syntax(ptcl_parser *parser)
{
    ptcl_parser_clear_scope(parser);

    ptcl_parser_syntax_pair pair = parser->syntaxes_nodes[--parser->syntax_depth];
    parser->state = pair.state;
    ptcl_statement_func_body *temp = pair.body;
    ptcl_statement_func_body_destroy(*temp);
    parser->root = pair.previous_body;
    free(pair.syntax.nodes);
    free(temp);
}

void ptcl_parser_leave_from_insert_state(ptcl_parser *parser)
{
    free(ptcl_parser_tokens(parser));
    ptcl_parser_set_state(parser, ptcl_parser_insert_state_at(parser, --parser->insert_states_count)->state);
}

static inline ptcl_statement_func_call ptcl_handle_builtin_execution(ptcl_parser *parser, ptcl_parser_function *target,
                                                                     ptcl_statement_func_call *func_call, ptcl_location location, bool is_expression)
{
    if (parser->is_ignore_error && strcmp(func_call->func_decl->name.value, PTCL_PARSER_ERROR_FUNC_NAME) == 0)
    {
        return *func_call;
    }

    ptcl_expression *result = target->bind(parser, func_call->arguments, func_call->count, location, is_expression);
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_statement_func_call){0};
    }

    func_call->is_built_in = true;
    func_call->built_in = result;
    return *func_call;
}

static inline ptcl_statement_func_call ptcl_handle_static_function(ptcl_parser *parser, ptcl_parser_function *target,
                                                                   ptcl_statement_func_call *func_call, ptcl_expression *self, ptcl_location location)
{
    ptcl_interpreter_reset(parser->interpreter);
    ptcl_expression *result = ptcl_interpreter_evaluate_function_call(parser->interpreter, *func_call, false, self, location);
    if (ptcl_parser_critical(parser))
    {
        ptcl_statement_func_call_destroy(*func_call);
        return (ptcl_statement_func_call){0};
    }

    func_call->built_in = result;
    func_call->is_built_in = result != NULL;
    if (func_call->is_built_in)
    {
        ptcl_statement_func_call_destroy(*func_call);
    }

    return *func_call;
}

ptcl_statement_func_call ptcl_parser_func_call(ptcl_parser *parser, ptcl_parser_function *function, ptcl_expression *self, bool is_expression)
{
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_identifier identifier;
    ptcl_name name;
    if (function != NULL)
    {
        name = (ptcl_parser_current(parser).type == ptcl_token_left_par_type)
                   ? (ptcl_name){.value = function->name.value, .is_free = false}
                   : ptcl_parser_name_word(parser);
        identifier = ptcl_identifier_create_by_name(name);
    }
    else
    {
        size_t start = ptcl_parser_position(parser);
        name = ptcl_parser_name_word(parser);
        if (ptcl_parser_critical(parser))
        {
            return (ptcl_statement_func_call){0};
        }

        if (ptcl_parser_current(parser).type != ptcl_token_left_par_type)
        {
            ptcl_parser_set_position(parser, start);
            ptcl_name_destroy(name);
            ptcl_expression *expression = ptcl_parser_dot(parser, NULL, NULL, is_expression, false);
            if (ptcl_parser_critical(parser))
            {
                return (ptcl_statement_func_call){0};
            }

            identifier = ptcl_identifier_create_by_expr(expression);
            return ptcl_statement_func_call_create(expression->dot.right->func_call.func_decl, identifier, NULL, 0);
        }

        identifier = ptcl_identifier_create_by_name(name);
    }

    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_identifier_destroy(identifier);
        return (ptcl_statement_func_call){0};
    }

    ptcl_statement_func_decl target;
    if (function == NULL)
    {
        ptcl_parser_function *instance_target;
        if (!ptcl_parser_try_get_function(parser, name, &function))
        {
            ptcl_identifier_destroy(identifier);
            ptcl_parser_throw_unknown_function(parser, name.value, location);
            return (ptcl_statement_func_call){0};
        }
    }

    target = function->func;
    if (is_expression && target.return_type.type == ptcl_value_void_type)
    {
        ptcl_identifier_destroy(identifier);
        ptcl_parser_throw_except_type_specifier(parser, ptcl_parser_current(parser).location);
        return (ptcl_statement_func_call){0};
    }

    size_t expected_arguments = target.is_variadic ? target.count + 4 : target.count;
    ptcl_statement_func_call func_call = ptcl_statement_func_call_create(function, identifier, NULL, 0);
    if (expected_arguments > 0)
    {
        func_call.arguments = malloc(expected_arguments * sizeof(ptcl_expression *));
        if (func_call.arguments == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_func_call){0};
        }
    }

    ptcl_parser_tokens_state state;
    bool is_tokens = false;
    size_t start = ptcl_parser_insert_states_count(parser);
    ptcl_parser_insert_state *start_state = ptcl_parser_insert_state_at(parser, start);
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            if (is_tokens)
            {
                ptcl_parser_leave_from_insert_state(parser);
                if (ptcl_parser_insert_states_count(parser) == 0)
                {
                    is_tokens = false;
                }

                continue;
            }

            ptcl_parser_throw_except_token(
                parser,
                ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type),
                location);
            goto leave;
        }

        ptcl_parser_tokens_state temp_state;
        bool is_toggle = ptcl_parser_try_parse_insert(parser, &temp_state, ptcl_parser_current(parser).location);
        if (ptcl_parser_critical(parser))
        {
            goto leave;
        }

        if (ptcl_parser_insert_states_count(parser) >= PTCL_PARSER_DECL_INSERT_DEPTH)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            goto leave;
        }

        if (is_toggle)
        {
            is_tokens = true;
            start_state->state = parser->state;
            start_state->syntax_depth = parser->syntax_depth;
            ptcl_parser_insert_state *state = ptcl_parser_insert_state_at(parser, ++parser->insert_states_count);
            state->state = temp_state;
            state->syntax_depth = parser->syntax_depth;
            ptcl_parser_set_state(parser, temp_state);
            continue;
        }

        if (!target.is_variadic && func_call.count >= target.count)
        {
            ptcl_parser_throw_wrong_arguments(parser, name.value, func_call.arguments,
                                              func_call.count, target.arguments, target.count, location);
            goto leave;
        }

        if (func_call.count >= expected_arguments)
        {
            expected_arguments = expected_arguments * 2 + 4;
            ptcl_expression **buffer = realloc(func_call.arguments, expected_arguments * sizeof(ptcl_expression *));
            if (buffer == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
            leave:
                ptcl_statement_func_call_destroy(func_call);
                if (ptcl_parser_insert_states_count(parser) > 1)
                {
                    for (size_t i = start + 1; i < ptcl_parser_insert_states_count(parser); i++)
                    {
                        free(ptcl_parser_insert_state_at(parser, i)->state.tokens);
                    }

                    free(parser->state.tokens);
                    ptcl_parser_set_state(parser, ptcl_parser_insert_state_at(parser, 0)->state);
                }

                return (ptcl_statement_func_call){0};
            }

            func_call.arguments = buffer;
        }

        ptcl_argument *argument = (func_call.count < target.count) ? &target.arguments[func_call.count] : NULL;
        ptcl_type *excepted = (argument && !argument->is_variadic) ? &argument->type : NULL;
        func_call.arguments[func_call.count] = ptcl_parser_cast(parser, excepted, false);
        if (ptcl_parser_critical(parser))
        {
            goto leave;
        }

        func_call.count++;
        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    const bool argument_count_mismatch = target.is_variadic
                                             ? (func_call.count < target.count - 1)
                                             : (func_call.count != target.count);
    if (argument_count_mismatch)
    {
        ptcl_parser_throw_wrong_arguments(parser, name.value, func_call.arguments,
                                          func_call.count, target.arguments, target.count, location);
        ptcl_statement_func_call_destroy(func_call);
        return (ptcl_statement_func_call){0};
    }

    func_call.return_type = target.return_type;
    func_call.func_decl = &function->func;
    if (function != NULL && function->is_built_in)
    {
        return ptcl_handle_builtin_execution(parser, function, &func_call, location, is_expression);
    }
    else if (target.is_static)
    {
        return ptcl_handle_static_function(parser, function, &func_call, self, location);
    }
    else
    {
        func_call.is_built_in = false;
    }

    return func_call;
}

ptcl_statement_func_body ptcl_parser_func_body(ptcl_parser *parser, bool with_brackets, bool change_root, bool is_static)
{
    ptcl_statement_func_body func_body = ptcl_statement_func_body_create(NULL, 0, ptcl_parser_root(parser));
    ptcl_parser_func_body_by_pointer(parser, &func_body, with_brackets, change_root, is_static);
    return func_body;
}

void ptcl_parser_func_body_by_pointer(ptcl_parser *parser, ptcl_statement_func_body *func_body_pointer, bool with_brackets, bool change_root, bool is_ignore_error)
{
    ptcl_parser_match(parser, ptcl_token_left_curly_type);

    ptcl_statement_func_body *previous = ptcl_parser_root(parser);
    ptcl_statement_func_body *previous_main = parser->main_root;
    if (change_root)
    {
        parser->root = func_body_pointer;
        parser->main_root = func_body_pointer;
    }

    const bool last_ignore_error = parser->is_ignore_error;
    parser->is_ignore_error = is_ignore_error;
    size_t depth = parser->syntax_depth;
    while (depth <= parser->syntax_depth)
    {
        if (with_brackets && ptcl_parser_match(parser, ptcl_token_right_curly_type))
        {
            break;
        }

        if (ptcl_parser_ended(parser))
        {
            if (with_brackets)
            {
                ptcl_location location = ptcl_parser_current(parser).location;
                char *token = ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_curly_type);
                ptcl_parser_throw_except_token(parser, token, location);
                ptcl_statement_func_body_destroy(*func_body_pointer);
            }

            break;
        }

        ptcl_statement *statement = ptcl_parser_parse_statement(parser);
        if (ptcl_parser_critical(parser))
        {
            ptcl_parser_clear_scope(parser);
            if (statement == NULL)
            {
                ptcl_statement_func_body_destroy(*func_body_pointer);
                break;
            }

            // Already destryoed
            if (statement->type != ptcl_statement_each_type && statement->type != ptcl_statement_unsyntax_type)
            {
                ptcl_statement_func_body_destroy(*func_body_pointer);
            }
            else
            {
                free(statement);
            }

            break;
        }

        if (statement == NULL)
        {
            continue;
        }

        if (statement->type == ptcl_statement_each_type)
        {
            free(statement);
            continue;
        }

        ptcl_statement **buffer = realloc(func_body_pointer->statements, (func_body_pointer->count + 1) * sizeof(ptcl_statement *));
        if (buffer == NULL)
        {
            ptcl_statement_func_body_destroy(*func_body_pointer);
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            break;
        }

        func_body_pointer->statements = buffer;
        func_body_pointer->statements[func_body_pointer->count] = statement;
        if (ptcl_parser_critical(parser))
        {
            ptcl_statement_func_body_destroy(*func_body_pointer);
            break;
        }

        func_body_pointer->count++;
    }

    parser->is_ignore_error = last_ignore_error;
    if (change_root)
    {
        ptcl_parser_clear_scope(parser);
        parser->root = previous;
        parser->main_root = previous_main;
    }
}

void ptcl_parser_extra_body(ptcl_parser *parser, bool is_syntax)
{
    if (ptcl_parser_current(parser).type != ptcl_token_at_type || ptcl_parser_peek(parser, 1).type != ptcl_token_left_curly_type)
    {
        return;
    }

    ptcl_parser_skip(parser);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_statement_func_body body = ptcl_statement_func_body_create(
        NULL, 0,
        parser->inserted_body == NULL                      ? parser->main_root
        : parser->inserted_body->root == parser->main_root ? parser->inserted_body
                                                           : parser->main_root);
    ptcl_parser_func_body_by_pointer(parser, &body, true, false, parser->is_ignore_error);
    if (ptcl_parser_critical(parser))
    {
        return;
    }

    ptcl_statement_func_body *previous = ptcl_parser_root(parser);
    parser->root = body.root;

    ptcl_statement *statement = ptcl_statement_func_body_create_stat(body, location);
    if (statement == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_destroy(statement);
        ptcl_parser_throw_out_of_memory(parser, location);
        parser->root = previous;
        return;
    }

    ptcl_statement_func_body *root_body = ptcl_parser_root(parser);
    size_t new_count = root_body->count + 1;
    ptcl_statement **new_statements = realloc(root_body->statements,
                                              sizeof(ptcl_statement *) * new_count);
    if (new_statements == NULL)
    {
        ptcl_statement_destroy(statement);
        ptcl_parser_throw_out_of_memory(parser, location);
        parser->root = previous;
        return;
    }

    new_statements[root_body->count] = statement;
    root_body->statements = new_statements;
    root_body->count = new_count;
    parser->root = previous;
    ptcl_parser_extra_body(parser, is_syntax);
}

ptcl_type ptcl_parser_type(ptcl_parser *parser, bool with_word, bool with_any)
{
    bool with_syntax = false;
    if (parser->is_syntax_body)
    {
        with_syntax = ptcl_parser_parse_try_syntax_usage_here(parser, false);
    }

    size_t depth = parser->syntax_depth;
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_type){0};
    }

    ptcl_parser_extra_body(parser, with_syntax);
    if (ptcl_parser_critical(parser))
    {
        if (with_syntax && parser->syntax_depth == depth)
        {
            ptcl_parser_leave_from_syntax(parser);
        }

        return (ptcl_type){0};
    }

    bool is_static = ptcl_parser_match(parser, ptcl_token_static_type);
    ptcl_token current = ptcl_parser_current(parser);
    ptcl_parser_skip(parser);

    ptcl_type target;
    bool parse_success = true;

    switch (current.type)
    {
    case ptcl_token_word_word_type:
        if (with_word)
        {
            target = ptcl_type_word;
            break;
        }
        // Fallthrough
    case ptcl_token_character_word_type:
        target = ptcl_type_character;
        break;
    case ptcl_token_type_type:
        target = ptcl_type_any_type;
        break;
    case ptcl_token_double_type:
        target = ptcl_type_double;
        break;
    case ptcl_token_float_type:
        target = ptcl_type_float;
        break;
    case ptcl_token_integer_type:
        target = ptcl_type_integer;
        break;
    case ptcl_token_void_type:
        target = ptcl_type_void;
        break;
    case ptcl_token_pointer_type:
        target = ptcl_type_any_pointer;
        break;
    case ptcl_token_left_par_type:
    {
        ptcl_parse_arguments_result arguments = ptcl_parser_parse_arguments(parser, current.location, false, true);
        if (ptcl_parser_critical(parser))
        {
            return (ptcl_type){0};
        }

        ptcl_parser_except(parser, ptcl_token_colon_type);
        if (ptcl_parser_critical(parser))
        {
            ptcl_parse_arguments_result_destroy(arguments);
            return (ptcl_type){0};
        }

        ptcl_type *return_type = malloc(sizeof(ptcl_type));
        if (return_type == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, current.location);
            ptcl_parse_arguments_result_destroy(arguments);
            return (ptcl_type){0};
        }

        *return_type = ptcl_parser_type(parser, false, false);
        if (ptcl_parser_critical(parser))
        {
            ptcl_parse_arguments_result_destroy(arguments);
            free(return_type);
            return (ptcl_type){0};
        }

        ptcl_type function = ptcl_type_create_func(return_type, NULL, 0);
        function.function_pointer.arguments = arguments.arguments;
        function.function_pointer.count = arguments.count;
        return_type->is_primitive = false;
        target = function;
        target.is_static = is_static;
        break;
    }
    case ptcl_token_any_type:
        if (with_any)
        {
            target = ptcl_type_any;
            break;
        }
        else
        {
            parse_success = false;
            ptcl_parser_throw_unknown_type(parser, current.value, current.location);
            break;
        }
    case ptcl_token_exclamation_mark_type:
        ptcl_parser_back(parser);
        ptcl_name name = ptcl_parser_name_word_or_type(parser);
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="
        if (ptcl_parser_critical(parser))
        {
            break;
        }
    case ptcl_token_word_type:
        // TODO: search type by name in one loop
        if (current.type == ptcl_token_word_type)
        {
            name = ptcl_name_create_fast_w(current.value, false);
        }

        ptcl_parser_typedata *typedata;
        ptcl_parser_comp_type *comp_type;
        ptcl_parser_variable *variable;
        if (ptcl_parser_try_get_typedata(parser, name, &typedata))
        {
            ptcl_name_destroy(name);
            target = ptcl_type_create_typedata(typedata->name.value, false);
            break;
        }
        else if (ptcl_parser_try_get_comp_type(parser, name, is_static, &comp_type))
        {
            ptcl_name_destroy(name);
            if (is_static)
            {
                if (comp_type->static_type == NULL)
                {
                    ptcl_parser_throw_except_type_specifier(parser, current.location);
                    parse_success = false;
                    break;
                }

                target = ptcl_type_create_comp_type_t(comp_type->static_type);
                break;
            }
            else
            {
                if (comp_type->comp_type == NULL)
                {
                    ptcl_parser_throw_except_type_specifier(parser, current.location);
                    parse_success = false;
                    break;
                }

                target = ptcl_type_create_comp_type_t(comp_type->comp_type);
                break;
            }
        }

        if (current.type == ptcl_token_exclamation_mark_type)
        {
            if (ptcl_parser_try_get_variable(parser, name, &variable))
            {
                if (variable->type.type == ptcl_value_object_type_type)
                {
                    ptcl_name_destroy(name);
                    target = ptcl_type_copy(variable->built_in->object_type.type);
                    break;
                }
            }
        }

        ptcl_name_destroy(name);
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="
    default:
        parse_success = false;
        ptcl_parser_throw_unknown_type(parser, current.value, current.location);
        break;
    }

    if (with_syntax && parser->syntax_depth == depth)
    {
        ptcl_parser_leave_from_syntax(parser);
    }

    if (!parse_success)
    {
        return (ptcl_type){0};
    }

    if (is_static)
    {
        target.is_static = true;
    }

    if (target.type != ptcl_value_word_type)
    {
        ptcl_type parsed_target = ptcl_parser_pointers(parser, target);
        if (ptcl_parser_critical(parser))
        {
            return (ptcl_type){0};
        }

        target = parsed_target;
    }
    else
    {
        if (!is_static)
        {
            ptcl_parser_throw_must_be_static(parser, current.location);
            target.is_static = true;
        }
    }

    return target;
}

ptcl_statement_func_decl ptcl_parser_func_decl(ptcl_parser *parser, bool is_prototype, bool is_global, bool is_static)
{
    ptcl_parser_match(parser, ptcl_token_function_type);

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name name = ptcl_parser_name_word(parser);
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_statement_func_decl){0};
    }

    if (ptcl_parser_is_function_defined(parser, name))
    {
        ptcl_parser_throw_redefination(parser, name.value, location);
    }

    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_name_destroy(name);
        return (ptcl_statement_func_decl){0};
    }

    // We set prototype, because on freeing we dont have body
    ptcl_statement_func_decl func_decl = ptcl_statement_func_decl_create(name, NULL, 0, NULL, ptcl_type_integer, true, false);
    func_decl.func_body = malloc(sizeof(ptcl_statement_func_body));
    if (func_decl.func_body == NULL)
    {
        ptcl_name_destroy(name);
        ptcl_parser_throw_out_of_memory(parser, location);
        return (ptcl_statement_func_decl){0};
    }

    *func_decl.func_body = ptcl_statement_func_body_create(NULL, 0, ptcl_parser_root(parser));
    ptcl_parse_arguments_result arguments = ptcl_parser_parse_arguments(parser, location, true, true);
    if (ptcl_parser_critical(parser))
    {
        ptcl_name_destroy(name);
        free(func_decl.func_body);
        return (ptcl_statement_func_decl){0};
    }

    ptcl_parser_except(parser, ptcl_token_colon_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_statement_func_decl_destroy(func_decl);
        return (ptcl_statement_func_decl){0};
    }

    ptcl_type return_type = ptcl_parser_type(parser, false, false);
    if (ptcl_parser_critical(parser))
    {
        ptcl_statement_func_decl_destroy(func_decl);
        return (ptcl_statement_func_decl){0};
    }
    else
    {
        func_decl.return_type = return_type;
    }

    func_decl.arguments = arguments.arguments;
    func_decl.count = arguments.count;
    func_decl.is_variadic = arguments.has_variadic;
    func_decl.is_static = is_static || func_decl.return_type.is_static;
    ptcl_parser_function function = ptcl_parser_function_create(is_global ? NULL : ptcl_parser_root(parser), func_decl);
    ptcl_type *variable_return_type = NULL;
    size_t function_identifier = parser->functions_count;
    size_t variable_identifier = parser->variables_count;
    if (!parser->is_type_body)
    {
        ptcl_name variable_name = function.name;
        variable_name.is_free = false;
        ptcl_type *variable_return_type = malloc(sizeof(ptcl_type));
        if (variable_return_type == NULL)
        {
            ptcl_statement_func_decl_destroy(func_decl);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_func_decl){0};
        }

        *variable_return_type = return_type;
        ptcl_type variable_type = ptcl_type_create_func(variable_return_type, func_decl.arguments, func_decl.count);
        variable_type.is_static = true;
        ptcl_parser_variable function_pointer = ptcl_parser_func_variable_create(variable_name, variable_type, function.root);
        if (!ptcl_parser_add_instance_function(parser, function) || !ptcl_parser_add_instance_variable(parser, function_pointer))
        {
            ptcl_statement_func_decl_destroy(func_decl);
            free(variable_return_type);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_func_decl){0};
        }
    }

    if (!is_prototype)
    {
        ptcl_type *previous_type = parser->return_type;
        parser->return_type = &return_type;
        for (size_t i = 0; i < func_decl.count; i++)
        {
            ptcl_argument argument = func_decl.arguments[i];
            ptcl_parser_variable variable = ptcl_parser_variable_create(argument.name, argument.type, NULL, false, func_decl.func_body);
            if (!ptcl_parser_add_instance_variable(parser, variable))
            {
                ptcl_statement_func_decl_destroy(func_decl);
                free(variable_return_type);
                ptcl_parser_throw_out_of_memory(parser, location);
                return (ptcl_statement_func_decl){0};
            }
        }

        const bool last_state = parser->is_type_body;
        parser->is_type_body = false;
        ptcl_parser_func_body_by_pointer(parser, func_decl.func_body, true, true, func_decl.is_static);
        parser->is_type_body = last_state;
        if (ptcl_parser_critical(parser))
        {
            free(func_decl.func_body);
            func_decl.func_body = NULL;
            func_decl.is_prototype = true;
            ptcl_statement_func_decl_destroy(func_decl);
            parser->return_type = previous_type;
            if (!parser->is_type_body)
            {
                parser->variables[variable_identifier].is_out_of_scope = true;
                parser->functions[function_identifier].is_out_of_scope = true;
            }

            return (ptcl_statement_func_decl){0};
        }

        func_decl.is_prototype = false;
        if (!parser->is_type_body)
        {
            ptcl_parser_function *original = &parser->functions[function_identifier];
            original->func = func_decl;
        }

        parser->return_type = previous_type;
    }

    return func_decl;
}

ptcl_statement_typedata_decl ptcl_parser_typedata_decl(ptcl_parser *parser, bool is_prototype, bool is_global)
{
    ptcl_parser_match(parser, ptcl_token_typedata_type);

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name name = ptcl_parser_name_word(parser);
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_statement_typedata_decl){0};
    }

    if (ptcl_parser_is_typedata_defined(parser, name))
    {
        ptcl_parser_throw_redefination(parser, name.value, location);
    }

    ptcl_statement_typedata_decl decl = ptcl_statement_typedata_decl_create(name, NULL, 0, true);
    if (is_prototype)
    {
        return decl;
    }

    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_statement_typedata_decl_destroy(decl);
        return (ptcl_statement_typedata_decl){0};
    }

    decl.is_prototype = false;
    ptcl_parse_arguments_result members = ptcl_parser_parse_arguments(parser, ptcl_parser_current(parser).location, true, false);
    if (ptcl_parser_critical(parser))
    {
        ptcl_statement_typedata_decl_destroy(decl);
        return (ptcl_statement_typedata_decl){0};
    }

    decl.members = members.arguments;
    decl.count = members.count;
    ptcl_parser_typedata instance = ptcl_parser_typedata_create(is_global ? NULL : ptcl_parser_root(parser), name, decl.members, decl.count);
    if (!ptcl_parser_add_instance_typedata(parser, instance))
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_typedata_decl_destroy(decl);
        return (ptcl_statement_typedata_decl){0};
    }

    return decl;
}

ptcl_statement_type_decl ptcl_parser_type_decl(ptcl_parser *parser, bool is_prototype, bool is_global)
{
    bool is_optional = ptcl_parser_match(parser, ptcl_token_optional_type);
    ptcl_parser_match(parser, ptcl_token_type_type);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name name = ptcl_parser_name_word(parser);
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_statement_type_decl){0};
    }

    ptcl_parser_except(parser, ptcl_token_colon_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_name_destroy(name);
        return (ptcl_statement_type_decl){0};
    }

    ptcl_statement_type_decl decl = ptcl_statement_type_decl_create(name, NULL, 0, NULL, 0, is_optional, is_prototype);
    while (true)
    {
        ptcl_type_member *buffer = realloc(decl.types, (decl.types_count + 1) * sizeof(ptcl_type_member));
        if (buffer == NULL)
        {
            ptcl_statement_type_decl_destroy(decl);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_type_decl){0};
        }

        decl.types = buffer;
        bool is_up = ptcl_parser_match(parser, ptcl_token_up_type);
        ptcl_type type = ptcl_parser_type(parser, false, false);
        if (ptcl_parser_critical(parser))
        {
            if (decl.types_count == 0)
            {
                free(decl.types);
            }

            ptcl_statement_type_decl_destroy(decl);
            return (ptcl_statement_type_decl){0};
        }

        if (decl.types_count > 0)
        {
            ptcl_type_member expected = decl.types[decl.types_count - 1];
            if (!ptcl_type_is_castable(expected.type, type))
            {
                ptcl_parser_throw_fast_incorrect_type(parser, expected.type, type, location);
                ptcl_statement_type_decl_destroy(decl);
                return (ptcl_statement_type_decl){0};
            }
        }

        decl.types[decl.types_count++] = ptcl_type_member_create(type, is_up);
        if (!ptcl_parser_match(parser, ptcl_token_comma_type))
        {
            break;
        }
    }

    const bool is_static = decl.types[0].type.is_static;
    if (ptcl_parser_is_comp_type_defined(parser, name, is_static))
    {
        ptcl_parser_throw_redefination(parser, name.value, location);
    }

    ptcl_type_comp_type *comp_type = malloc(sizeof(ptcl_type_comp_type));
    if (comp_type == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_type_decl_destroy(decl);
        return (ptcl_statement_type_decl){0};
    }

    *comp_type = ptcl_type_create_comp_type(decl, is_static);
    ptcl_parser_comp_type instance = ptcl_parser_comp_type_create(is_global ? NULL : ptcl_parser_root(parser), name, comp_type, comp_type->is_static);
    if (!ptcl_parser_add_instance_comp_type(parser, instance))
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_type_decl_destroy(decl);
        return (ptcl_statement_type_decl){0};
    }

    if (ptcl_parser_match(parser, ptcl_token_left_curly_type))
    {
        ptcl_statement_func_body *body = malloc(sizeof(ptcl_statement_func_body));
        if (body == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_statement_type_decl_destroy(decl);
            return (ptcl_statement_type_decl){0};
        }

        *body = ptcl_statement_func_body_create(NULL, 0, ptcl_parser_root(parser));
        comp_type->functions = body;
        ptcl_parser_variable this_var = ptcl_parser_variable_create(
            ptcl_name_create_fast_w("this", false),
            ptcl_type_create_comp_type_t(comp_type), NULL, false, body);
        this_var.type.is_static = false;
        ptcl_parser_variable self_var = ptcl_parser_variable_create(
            ptcl_name_create_fast_w("self", false),
            decl.types[0].type, NULL, false, body);
        self_var.type.is_static = false;
        if (!ptcl_parser_add_instance_variable(parser, this_var) || !ptcl_parser_add_instance_variable(parser, self_var))
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_statement_type_decl_destroy(decl);
            ptcl_statement_func_body_destroy(*body);
            free(body);
            return (ptcl_statement_type_decl){0};
        }

        const bool last_state = parser->is_type_body;
        parser->is_type_body = true;
        ptcl_parser_func_body_by_pointer(parser, body, true, true, false);
        parser->is_type_body = last_state;
        if (ptcl_parser_critical(parser))
        {
            free(body);
            ptcl_statement_type_decl_destroy(decl);
            return (ptcl_statement_type_decl){0};
        }

        decl.body = body;
        decl.functions = comp_type->functions;
    }

    return decl;
}

ptcl_statement_assign ptcl_parser_assign(ptcl_parser *parser, bool is_global)
{
    ptcl_location location = ptcl_parser_current(parser).location;
    size_t position = ptcl_parser_position(parser);
    ptcl_name name = ptcl_parser_name_word(parser);
    ptcl_identifier identifier;
    ptcl_expression *ctor = NULL;
    size_t index;
    ptcl_token current_identifier = ptcl_parser_current(parser);
    if (current_identifier.type != ptcl_token_equals_type && current_identifier.type != ptcl_token_colon_type)
    {
        // TODO: avoid reparsing
        ptcl_parser_set_position(parser, position);
        ptcl_name_destroy(name);
        identifier = ptcl_identifier_create_by_expr(ptcl_parser_dot(parser, NULL, false, false, false));
        if (ptcl_parser_critical(parser))
        {
            return (ptcl_statement_assign){0};
        }

        // By dot
        if (identifier.value->return_type.is_static)
        {
            ctor = ptcl_parser_get_ctor_from_dot(parser, identifier.value, true);
            ptcl_argument *placeholder;
            if (!ptcl_parser_try_get_typedata_member(parser, ctor->return_type.typedata, identifier.value->dot.name.value, &placeholder, &index))
            {
                ptcl_parser_throw_unknown_member(parser, name.value, location);
                ptcl_expression_destroy(identifier.value);
                return (ptcl_statement_assign){0};
            }
        }
    }
    else
    {
        identifier = ptcl_identifier_create_by_name(name);
    }

    if (ptcl_parser_critical(parser))
    {
        ptcl_identifier_destroy(identifier);
        return (ptcl_statement_assign){0};
    }

    ptcl_type type;
    bool with_type = ptcl_parser_match(parser, ptcl_token_colon_type);
    bool define = true;

    ptcl_parser_variable *created = NULL;
    if (with_type)
    {
        if (ptcl_parser_is_variable_defined(parser, name))
        {
            ptcl_parser_throw_variable_redefinition(parser, name.value, location);
        }

        if (ptcl_parser_current(parser).type == ptcl_token_equals_type)
        {
            with_type = false;
        }
        else
        {
            type = ptcl_parser_type(parser, true, false);
            if (ptcl_parser_critical(parser))
            {
                ptcl_identifier_destroy(identifier);
                return (ptcl_statement_assign){0};
            }
        }
    }
    else
    {
        if (identifier.is_name && !ptcl_parser_try_get_variable(parser, name, &created))
        {
            ptcl_parser_throw_unknown_variable_or_type(parser, name.value, location);
            return (ptcl_statement_assign){0};
        }

        with_type = true;
        define = false;
        if (identifier.is_name)
        {
            type = created->type;
        }
        else
        {
            type = identifier.value->return_type;
        }
    }

    ptcl_parser_except(parser, ptcl_token_equals_type);
    if (ptcl_parser_critical(parser))
    {
        if (with_type)
        {
            ptcl_type_destroy(type);
        }

        ptcl_identifier_destroy(identifier);
        return (ptcl_statement_assign){0};
    }

    ptcl_expression *value = ptcl_parser_cast(parser, with_type ? &type : NULL, false);
    if (ptcl_parser_critical(parser))
    {
        if (with_type)
        {
            ptcl_type_destroy(type);
        }

        ptcl_identifier_destroy(identifier);
        return (ptcl_statement_assign){0};
    }

    if (ctor != NULL)
    {
        // TODO: Add references counter to avoid memory leaks
        ctor->ctor.values[index] = value;
    }

    if (!with_type)
    {
        type = value->return_type;
        if (type.type == ptcl_value_pointer_type && type.pointer.is_null)
        {
            type.pointer.is_any = true;
            type.pointer.is_null = false;
        }
        else if (define && ptcl_type_is_castable_to_unstatic(type))
        {
            type.is_static = false;
        }
    }
    else
    {
        ptcl_parser_set_arrays_length(&type, &value->return_type);
    }

    const bool is_static = type.is_static;
    ptcl_statement_assign assign = ptcl_statement_assign_create(identifier, type, with_type, is_static ? NULL : value, define);
    if (identifier.is_name && created == NULL)
    {
        ptcl_parser_variable variable = ptcl_parser_variable_create(name, type, value, is_static, is_global ? NULL : ptcl_parser_root(parser));
        if (!ptcl_parser_add_instance_variable(parser, variable))
        {
            ptcl_statement_assign_destroy(assign);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_assign){0};
        }
    }
    else
    {
        if (created != NULL)
        {
            created->built_in = value;
        }
    }

    return assign;
}

ptcl_statement_return ptcl_parser_return(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_return_type);
    if (parser->return_type != NULL)
    {
        if (parser->return_type->type == ptcl_value_void_type)
        {
            ptcl_parser_match(parser, ptcl_token_none_type);
            return ptcl_statement_return_create(NULL);
        }
    }

    ptcl_expression *value = ptcl_parser_cast(parser, parser->return_type, false);
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_statement_return){0};
    }

    if (value->return_type.type == ptcl_value_void_type)
    {
        ptcl_parser_throw_except_type_specifier(parser, ptcl_parser_current(parser).location);
        return (ptcl_statement_return){0};
    }

    return ptcl_statement_return_create(value);
}

ptcl_statement *ptcl_parser_if(ptcl_parser *parser, bool is_static)
{
    ptcl_parser_match(parser, ptcl_token_if_type);

    ptcl_type type = ptcl_type_integer;
    type.is_static = is_static;
    ptcl_expression *condition = ptcl_parser_cast(parser, &type, false);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    if (condition->return_type.is_static || is_static)
    {
        ptcl_statement_func_body result;
        const bool condition_value = condition->integer_n;
        ptcl_location location = condition->location;
        ptcl_expression_destroy(condition);
        if (condition_value)
        {
            result = ptcl_parser_func_body(parser, true, false, parser->is_ignore_error);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            if (ptcl_parser_match(parser, ptcl_token_else_type))
            {
                ptcl_parser_except(parser, ptcl_token_left_curly_type);
                if (ptcl_parser_critical(parser))
                {
                    ptcl_statement_func_body_destroy(result);
                    return NULL;
                }

                ptcl_parser_skip_block_or_expression(parser);
                if (ptcl_parser_critical(parser))
                {
                    ptcl_statement_func_body_destroy(result);
                    return NULL;
                }
            }
        }
        else
        {
            ptcl_parser_skip(parser);
            ptcl_parser_skip_block_or_expression(parser);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            if (ptcl_parser_match(parser, ptcl_token_else_type))
            {
                result = ptcl_parser_func_body(parser, true, false, parser->is_ignore_error);
                if (ptcl_parser_critical(parser))
                {
                    return NULL;
                }
            }
            else
            {
                return NULL;
            }
        }

        return ptcl_statement_func_body_create_stat(result, location);
    }

    ptcl_statement_func_body body = ptcl_parser_func_body(parser, true, true, parser->is_ignore_error);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(condition);
        return NULL;
    }

    ptcl_statement_if if_stat = ptcl_statement_if_create(condition, body, false, (ptcl_statement_func_body){0});
    if (ptcl_parser_match(parser, ptcl_token_else_type))
    {
        if_stat.else_body = ptcl_parser_func_body(parser, true, true, parser->is_ignore_error);
        if_stat.with_else = !ptcl_parser_critical(parser);
        if (ptcl_parser_critical(parser))
        {
            ptcl_statement_if_destroy(if_stat);
            return NULL;
        }
    }

    ptcl_statement *statement = ptcl_statement_create(ptcl_statement_if_type, ptcl_parser_root(parser), ptcl_attributes_create(NULL, 0), condition->location);
    if (statement == NULL)
    {
        ptcl_statement_if_destroy(if_stat);
        ptcl_parser_throw_out_of_memory(parser, condition->location);
        return NULL;
    }

    statement->if_stat = if_stat;
    return statement;
}

ptcl_expression *ptcl_parser_if_expression(ptcl_parser *parser, bool is_static)
{
    ptcl_parser_match(parser, ptcl_token_if_type);
    ptcl_type type = ptcl_type_integer;
    type.is_static = is_static;
    ptcl_expression *condition = ptcl_parser_cast(parser, &type, false);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_left_curly_type);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    if (is_static)
    {
        ptcl_expression *result;

        int condition_value = condition->integer_n;
        ptcl_expression_destroy(condition);
        if (condition_value)
        {
            result = ptcl_parser_cast(parser, NULL, false);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_right_curly_type);
            if (ptcl_parser_critical(parser))
            {
                ptcl_expression_destroy(result);
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_else_type);
            ptcl_parser_except(parser, ptcl_token_left_curly_type);
            if (ptcl_parser_critical(parser))
            {
                ptcl_expression_destroy(result);
                return NULL;
            }

            ptcl_parser_skip_block_or_expression(parser);
            if (ptcl_parser_critical(parser))
            {
                ptcl_expression_destroy(result);
                return NULL;
            }
        }
        else
        {
            ptcl_parser_skip_block_or_expression(parser);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_else_type);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_left_curly_type);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            result = ptcl_parser_cast(parser, NULL, false);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_right_curly_type);
            if (ptcl_parser_critical(parser))
            {
                ptcl_expression_destroy(result);
                return NULL;
            }
        }

        return result;
    }

    ptcl_expression *body = ptcl_parser_cast(parser, NULL, false);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(condition);
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_right_curly_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_else_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_left_curly_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return NULL;
    }

    ptcl_expression *else_body = ptcl_parser_cast(parser, &body->return_type, false);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_right_curly_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        ptcl_expression_destroy(else_body);
        return NULL;
    }

    ptcl_expression_if if_expr = ptcl_expression_if_create(condition, body, else_body);
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_if_type, if_expr.body->return_type, condition->location);
    if (expression == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
        ptcl_expression_if_destroy(if_expr);
        return NULL;
    }

    expression->if_expr = if_expr;
    return expression;
}

ptcl_statement_func_body ptcl_parser_unsyntax(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_unsyntax_type);
    parser->is_syntax_body = false;
    ptcl_statement_func_body body = ptcl_parser_func_body(parser, true, false, parser->is_ignore_error);
    parser->is_syntax_body = true;
    return body;
}

void ptcl_parser_syntax_decl(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_syntax_type);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_parser_syntax syntax = ptcl_parser_syntax_create(ptcl_name_create_fast_w("", false), ptcl_parser_root(parser), NULL, 0, 0);
    bool is_injector = false;
    ptcl_location last_location;
    size_t depth = 0;
    bool except_end_token = false;

    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_left_curly_type), location);
            ptcl_parser_syntax_destroy(syntax);
            return;
        }

        ptcl_token current = ptcl_parser_current(parser);
        ptcl_parser_skip(parser);
        if (current.type == ptcl_token_left_curly_type)
        {
            break;
        }
        else if (current.type == ptcl_token_right_curly_type)
        {
            ptcl_parser_throw_not_allowed_token(parser, current.value, current.location);
            ptcl_parser_syntax_destroy(syntax);
            return;
        }

        bool breaked = false;
        ptcl_parser_syntax_node node;
        switch (current.type)
        {
        case ptcl_token_left_square_type:
        {
            ptcl_token next = ptcl_parser_current(parser);
            ptcl_parser_skip(parser);
            switch (next.type)
            {
            case ptcl_token_word_type:
            {
                ptcl_parser_variable *variable;
                if (ptcl_parser_match(parser, ptcl_token_colon_type))
                {
                    if (ptcl_parser_match(parser, ptcl_token_elipsis_type))
                    {
                        ptcl_parser_except(parser, ptcl_token_right_square_type);
                        if (ptcl_parser_critical(parser))
                        {
                            ptcl_parser_syntax_destroy(syntax);
                            return;
                        }

                        ptcl_token end_token = ptcl_parser_current(parser);
                        if (end_token.type == ptcl_token_left_curly_type)
                        {
                            ptcl_parser_throw_not_allowed_token(parser, current.value, current.location);
                            ptcl_parser_syntax_destroy(syntax);
                            return;
                        }

                        node = ptcl_parser_syntax_node_create_end_token(next.value);
                    }
                    else
                    {
                        ptcl_type type = ptcl_parser_type(parser, true, true);
                        if (ptcl_parser_critical(parser))
                        {
                            ptcl_parser_syntax_destroy(syntax);
                            return;
                        }

                        ptcl_parser_except(parser, ptcl_token_right_square_type);
                        if (ptcl_parser_critical(parser))
                        {
                            ptcl_type_destroy(type);
                            ptcl_parser_syntax_destroy(syntax);
                            return;
                        }

                        node = ptcl_parser_syntax_node_create_variable(type, next.value);
                    }

                    break;
                }
                else if (strcmp(next.value, "this") == 0)
                {
                    if (syntax.count != 0)
                    {
                        ptcl_parser_throw_not_allowed_token(parser, current.value, current.location);
                        ptcl_parser_syntax_destroy(syntax);
                        return;
                    }

                    ptcl_parser_except(parser, ptcl_token_right_square_type);
                    if (ptcl_parser_critical(parser))
                    {
                        ptcl_parser_syntax_destroy(syntax);
                        return;
                    }

                    ptcl_parser_except(parser, ptcl_token_left_curly_type);
                    if (ptcl_parser_critical(parser))
                    {
                        ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_curly_type), location);
                        ptcl_parser_syntax_destroy(syntax);
                        return;
                    }

                    is_injector = true;
                    breaked = true;
                    break;
                }

                ptcl_parser_back(parser);
                break;
            }
            case ptcl_token_exclamation_mark_type:
            {
                ptcl_parser_variable *variable;
                size_t start = ptcl_parser_position(parser);
                ptcl_parser_back(parser); // comeback to '!'
                ptcl_name word = ptcl_parser_name_word(parser);
                if (ptcl_parser_critical(parser))
                {
                    ptcl_parser_syntax_destroy(syntax);
                    return;
                }

                if (ptcl_parser_try_get_variable(parser, word, &variable) && variable->type.type == ptcl_value_object_type_type)
                {
                    node = ptcl_parser_syntax_node_create_object_type(*ptcl_type_get_target(variable->type));
                    ptcl_name_destroy(word);
                    break;
                }

                ptcl_name_destroy(word);
                ptcl_parser_set_position(parser, start);
                break;
            }
            case ptcl_token_left_curly_type:
                ptcl_parser_except(parser, ptcl_token_right_square_type);
                if (ptcl_parser_critical(parser))
                {
                    ptcl_parser_syntax_destroy(syntax);
                    return;
                }

                depth++;
                last_location = next.location;
                node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(next.value, false, current.location));
                break;
            case ptcl_token_right_curly_type:
                if (depth == 0)
                {
                    ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_curly_type), next.location);
                    ptcl_parser_syntax_destroy(syntax);
                    return;
                }

                ptcl_parser_except(parser, ptcl_token_right_square_type);
                if (ptcl_parser_critical(parser))
                {
                    ptcl_parser_syntax_destroy(syntax);
                    return;
                }

                depth--;
                node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(next.value, false, current.location));
                break;
            default:
                ptcl_parser_back(parser);
                node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(current.value, false, current.location));
                break;
            }

            break;
        }
        default:
            node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(current.value, false, current.location));
            break;
        }

        if (breaked)
        {
            break;
        }

        if (except_end_token)
        {
            if (node.type != ptcl_parser_syntax_node_word_type)
            {
                ptcl_parser_throw_not_allowed_token(parser, current.value, current.location);
                ptcl_parser_syntax_node_destroy(node);
                ptcl_parser_syntax_destroy(syntax);
                return;
            }

            except_end_token = false;
        }
        else
        {
            except_end_token = node.type == ptcl_parser_syntax_node_variable_type && node.variable.is_variadic;
        }

        ptcl_parser_syntax_node *buffer = realloc(syntax.nodes, (syntax.count + 1) * sizeof(ptcl_parser_syntax_node));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_parser_syntax_node_destroy(node);
            ptcl_parser_syntax_destroy(syntax);
            return;
        }

        syntax.nodes = buffer;
        syntax.nodes[syntax.count++] = node;
    }

    if (depth != 0)
    {
        ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_curly_type), last_location);
        ptcl_parser_syntax_destroy(syntax);
        return;
    }

    size_t start = ptcl_parser_position(parser);
    ptcl_parser_skip_block_or_expression(parser);
    if (ptcl_parser_critical(parser))
    {
        ptcl_parser_syntax_destroy(syntax);
        return;
    }

    // -1 because of curly
    size_t tokens_count = (ptcl_parser_position(parser) - start) - 1;
    size_t index = ptcl_parser_add_lated_body(parser, start, tokens_count, location);
    if (ptcl_parser_critical(parser))
    {
        ptcl_parser_syntax_destroy(syntax);
        return;
    }

    syntax.index = index;
    if (!is_injector)
    {
        if (!ptcl_parser_add_instance_syntax(parser, syntax))
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_parser_syntax_destroy(syntax);
            return;
        }
    }

    // if (is_injector && !ptcl_parser_add_this_pair(parser, ptcl_parser_this_s_pair_create(parser->main_root, syntax.start, syntax.tokens_count)))
    //{
    // ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
    // ptcl_parser_syntax_destroy(syntax);
    // return;
    //}
}

void ptcl_parser_each(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_each_type);
    ptcl_parser_match(parser, ptcl_token_left_par_type);

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name name = ptcl_parser_name_word(parser);
    if (ptcl_parser_critical(parser))
    {
        return;
    }

    ptcl_parser_except(parser, ptcl_token_comma_type);
    if (ptcl_parser_critical(parser))
    {
        return;
    }

    ptcl_type array = ptcl_type_create_pointer(&ptcl_type_any);
    array.is_static = true;
    ptcl_expression *value = ptcl_parser_cast(parser, &array, false);
    if (ptcl_parser_critical(parser))
    {
        return;
    }

    ptcl_parser_match(parser, ptcl_token_right_par_type);
    ptcl_statement_func_body empty = {
        .statements = NULL,
        .count = 0,
        .root = ptcl_parser_root(parser)};
    size_t position = ptcl_parser_position(parser);
    for (size_t i = 0; i < value->array.count; i++)
    {
        ptcl_expression *expression = value->array.expressions[i];
        expression->is_original = false;
        ptcl_parser_variable variable = ptcl_parser_variable_create(name, *ptcl_type_get_target(value->return_type), expression, true, &empty);
        size_t idenitifer = parser->variables_count;
        if (!ptcl_parser_add_instance_variable(parser, variable))
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_expression_destroy(value);
            ptcl_statement_func_body_destroy(*ptcl_parser_root(parser));
            return;
        }

        ptcl_parser_func_body_by_pointer(parser, &empty, true, true, parser->is_ignore_error);
        if (i != value->array.count - 1)
        {
            ptcl_parser_set_position(parser, position);
        }

        expression->is_original = true;
        parser->variables[idenitifer].is_built_in = false;
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(value);
            return;
        }
    }

    ptcl_statement_func_body *body_root = ptcl_parser_root(parser);
    size_t count = body_root->count + empty.count;
    ptcl_statement **buffer = realloc(body_root->statements, count * sizeof(ptcl_statement *));
    if (buffer == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_func_body_destroy(empty);
        ptcl_expression_destroy(value);
        return;
    }

    body_root->statements = buffer;
    for (size_t i = 0; i < empty.count; i++)
    {
        body_root->statements[i + body_root->count] = empty.statements[i];
    }

    body_root->count = count;
    free(empty.statements);
    ptcl_expression_destroy(value);
}

void ptcl_parser_undefine(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_undefine_type);
    ptcl_parser_match(parser, ptcl_token_left_par_type);
    ptcl_name name = ptcl_parser_name_word(parser);
    if (ptcl_parser_critical(parser))
    {
        return;
    }

    for (int i = parser->syntaxes_count - 1; i >= 0; i--)
    {
        ptcl_parser_syntax *instance = &parser->syntaxes[i];
        if (!ptcl_name_compare(instance->name, name) || !ptcl_func_body_can_access(instance->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        instance->is_out_of_scope = true;
        goto here;
    }

    for (int i = parser->comp_types_count - 1; i >= 0; i--)
    {
        ptcl_parser_comp_type *instance = &parser->comp_types[i];
        if (!ptcl_name_compare(instance->identifier, name) || !ptcl_func_body_can_access(instance->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        instance->is_out_of_scope = true;
        goto here;
    }

    for (int i = parser->typedatas_count - 1; i >= 0; i--)
    {
        ptcl_parser_typedata *instance = &parser->typedatas[i];
        if (!ptcl_name_compare(instance->name, name) || !ptcl_func_body_can_access(instance->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        instance->is_out_of_scope = true;
        goto here;
    }

    for (int i = parser->variables_count - 1; i >= 0; i--)
    {
        ptcl_parser_variable *instance = &parser->variables[i];
        if (!ptcl_name_compare(instance->name, name) || !ptcl_func_body_can_access(instance->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        instance->is_out_of_scope = true;
        goto here;
    }

here:
    ptcl_parser_match(parser, ptcl_token_right_par_type);
}

ptcl_expression *ptcl_parser_cast(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
    if (ptcl_parser_match(parser, ptcl_token_auto_type))
    {
        ptcl_location location = ptcl_parser_current(parser).location;
        if (expected == NULL)
        {
            ptcl_parser_throw_except_type_specifier(parser, location);
            return NULL;
        }

        ptcl_expression *value = ptcl_parser_cast(parser, expected, with_word);
        if (parser->is_critical)
        {
            return NULL;
        }

        ptcl_expression *cast = ptcl_expression_cast_create(value, *expected, false, location);
        if (cast == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_expression_destroy(value);
            return NULL;
        }

        return ptcl_expression_static_cast(cast);
    }

    ptcl_expression *left = ptcl_parser_binary(parser, expected, with_word);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    while (true)
    {
        if (ptcl_parser_ended(parser) ||
            ptcl_parser_current(parser).type != ptcl_token_colon_type ||
            ptcl_parser_peek(parser, 1).type != ptcl_token_colon_type)
        {
            break;
        }

        ptcl_location location = ptcl_parser_current(parser).location;
        ptcl_parser_skip(parser);
        ptcl_parser_skip(parser);

        ptcl_type type = ptcl_parser_type(parser, true, false);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        if (type.type == ptcl_value_type_type)
        {
            ptcl_type base = type.comp_type->types[0].type;
            if (!ptcl_type_is_castable(base, left->return_type))
            {
                ptcl_parser_throw_fast_incorrect_type(parser, base, left->return_type, location);
                ptcl_expression_destroy(left);
                ptcl_type_destroy(type);
                return NULL;
            }
        }

        ptcl_expression *cast = ptcl_expression_cast_create(left, type, true, location);
        if (cast == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_expression_destroy(left);
            ptcl_type_destroy(type);
            return NULL;
        }

        left = ptcl_expression_static_cast(cast);
    }

    if (expected != NULL && !ptcl_type_is_castable(*expected, left->return_type))
    {
        ptcl_parser_throw_fast_incorrect_type(parser, *expected, left->return_type, left->location);
        ptcl_expression_destroy(left);
        return NULL;
    }

    return left;
}

ptcl_expression *ptcl_parser_binary(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
    ptcl_expression *left = ptcl_parser_additive(parser, expected, with_word);
    if (ptcl_parser_critical(parser))
    {
        return left;
    }

    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            break;
        }

        ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);
        if (type == ptcl_binary_operator_none_type ||
            type == ptcl_binary_operator_multiplicative_type ||
            type == ptcl_binary_operator_division_type ||
            type == ptcl_binary_operator_plus_type ||
            type == ptcl_binary_operator_minus_type)
        {
            break;
        }

        if (ptcl_parser_peek(parser, 1).type == ptcl_token_equals_type)
        {
            switch (type)
            {
            case ptcl_binary_operator_equals_type:
                type = ptcl_binary_operator_equals_type;
                break;
            case ptcl_binary_operator_negation_type:
                type = ptcl_binary_operator_not_equals_type;
                break;
            case ptcl_binary_operator_greater_than_type:
                type = ptcl_binary_operator_greater_equals_than_type;
                break;
            case ptcl_binary_operator_less_than_type:
                type = ptcl_binary_operator_less_equals_than_type;
                break;
            default:
                break;
            }

            ptcl_parser_skip(parser);
        }
        else
        {
            if (type == ptcl_binary_operator_equals_type || type == ptcl_binary_operator_negation_type)
            {
                break;
            }
        }

        ptcl_type *expected = &left->return_type;
        if (type == ptcl_binary_operator_type_equals_type)
        {
            expected = &ptcl_type_any_type;
        }

        ptcl_parser_skip(parser);
        ptcl_expression *right = ptcl_parser_cast(parser, expected, false);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        left = ptcl_expression_binary_static_evaluate(type, left, right);
        if (left == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            return left;
        }
    }

    return left;
}

ptcl_expression *ptcl_parser_additive(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
    ptcl_expression *left = ptcl_parser_multiplicative(parser, expected, with_word);
    if (ptcl_parser_critical(parser) || !ptcl_value_is_number(left->return_type.type))
    {
        return left;
    }

    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            break;
        }

        ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);
        if (type != ptcl_binary_operator_plus_type && type != ptcl_binary_operator_minus_type)
        {
            break;
        }

        ptcl_parser_skip(parser);
        ptcl_expression *right = ptcl_parser_multiplicative(parser, &left->return_type, false);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        left = ptcl_expression_binary_static_evaluate(type, left, right);
        if (left == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            return left;
        }
    }

    return left;
}

ptcl_expression *ptcl_parser_multiplicative(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
    ptcl_expression *left = ptcl_parser_unary(parser, false, expected, with_word);
    if (ptcl_parser_critical(parser) || !ptcl_value_is_number(left->return_type.type))
    {
        return left;
    }

    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            break;
        }

        ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);
        if (type != ptcl_binary_operator_multiplicative_type && type != ptcl_binary_operator_division_type)
        {
            break;
        }

        ptcl_parser_skip(parser);
        ptcl_expression *right = ptcl_parser_unary(parser, false, &left->return_type, false);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        left = ptcl_expression_binary_static_evaluate(type, left, right);
        if (left == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            return left;
        }
    }

    return left;
}

ptcl_expression *ptcl_parser_unary(ptcl_parser *parser, bool only_value, ptcl_type *expected, bool with_word)
{
    if (ptcl_parser_ended(parser))
    {
        return ptcl_parser_dot(parser, expected, false, true, with_word);
    }

    ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);
    if (type != ptcl_binary_operator_none_type &&
        type != ptcl_binary_operator_equals_type &&
        type != ptcl_binary_operator_greater_than_type &&
        type != ptcl_binary_operator_less_than_type &&
        type != ptcl_binary_operator_greater_equals_than_type &&
        type != ptcl_binary_operator_less_equals_than_type)
    {
        ptcl_parser_skip(parser);
        ptcl_expression *value = ptcl_parser_cast(parser, NULL, false);
        if (ptcl_parser_critical(parser))
        {
            return NULL;
        }

        ptcl_type type_value = value->return_type;
        if (type == ptcl_binary_operator_multiplicative_type)
        {
            if (only_value)
            {
                return value;
            }

            if ((value->return_type.type != ptcl_value_pointer_type ||
                 value->return_type.pointer.is_any || value->return_type.pointer.is_null) ||
                value->return_type.is_static)
            {
                ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_any_pointer, value->return_type, value->location);
                ptcl_expression_destroy(value);
                return NULL;
            }

            type = ptcl_binary_operator_dereference_type;
            type_value = *ptcl_type_get_target(value->return_type);
        }
        else if (type == ptcl_binary_operator_reference_type)
        {
            ptcl_type *target = malloc(sizeof(ptcl_type));
            if (target == NULL)
            {
                ptcl_expression_destroy(value);
                ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
                return NULL;
            }

            *target = value->return_type;
            target->is_primitive = false;
            type_value = ptcl_type_create_pointer(target);
            type_value.pointer.target->is_static = false;
        }

        if (type == ptcl_binary_operator_reference_type || type == ptcl_binary_operator_dereference_type)
        {
            bool is_not_variable = value->type != ptcl_expression_variable_type &&
                                   value->type != ptcl_expression_cast_type &&
                                   (value->type != ptcl_expression_unary_type &&
                                    value->unary.type != ptcl_binary_operator_reference_type &&
                                    value->unary.type != ptcl_binary_operator_dereference_type);
            if (is_not_variable)
            {
                ptcl_parser_throw_must_be_variable(parser, value->location);
            }
        }

        ptcl_expression *evaluated = ptcl_expression_unary_static_evaluate(type, value);
        if (evaluated == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            return NULL;
        }

        evaluated->return_type = type_value;
        evaluated->return_type.is_static = value->return_type.is_static;
        return evaluated;
    }

    return ptcl_parser_dot(parser, expected, false, true, with_word);
}

static ptcl_expression *ptcl_parser_type_dot(ptcl_parser *parser, ptcl_expression *left,
                                             ptcl_name name, bool is_expression, ptcl_location location)
{
    ptcl_type_comp_type *comp_type = left->return_type.comp_type;
    ptcl_statement_func_decl *function;
    bool function_found = false;
    if (comp_type->functions != NULL)
    {
        for (size_t i = 0; i < comp_type->functions->count; i++)
        {
            function = &comp_type->functions->statements[i]->func_decl;
            if (ptcl_name_compare(function->name, name))
            {
                function_found = true;
                break;
            }
        }
    }

    if (!function_found)
    {
        ptcl_parser_throw_unknown_function(parser, name.value, location);
        ptcl_expression_destroy(left);
        return NULL;
    }

    ptcl_parser_function instance = ptcl_parser_function_create(parser->root, *function);
    ptcl_statement_func_call func_call = ptcl_parser_func_call(parser, &instance, left, is_expression);
    if (ptcl_parser_critical(parser))
    {
        ptcl_expression_destroy(left);
        return NULL;
    }

    if (func_call.is_built_in)
    {
        ptcl_expression_destroy(left);
        return func_call.built_in;
    }

    func_call.is_built_in = false;
    func_call.return_type = function->return_type;
    ptcl_expression *func = ptcl_expression_create(
        ptcl_expression_func_call_type,
        func_call.return_type,
        location);
    if (func == NULL)
    {
        ptcl_statement_func_call_destroy(func_call);
        ptcl_expression_destroy(left);
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    func->func_call = func_call;
    ptcl_expression *expression = ptcl_expression_create(
        ptcl_expression_dot_type,
        func_call.return_type,
        location);
    if (expression == NULL)
    {
        ptcl_expression_destroy(func);
        ptcl_expression_destroy(left);
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    expression->dot = ptcl_expression_dot_expression_create(left, func);
    return expression;
}

static ptcl_expression *ptcl_parser_typedata_dot(ptcl_parser *parser, ptcl_expression *left,
                                                 ptcl_name name, bool is_expression, ptcl_location location)
{
    ptcl_argument *member;
    size_t index;
    if (!ptcl_parser_try_get_typedata_member(parser, left->return_type.typedata, name.value, &member, &index))
    {
        ptcl_parser_throw_unknown_member(parser, name.value, location);
        ptcl_expression_destroy(left);
        return NULL;
    }

    const bool is_static = left->return_type.is_static;
    if (is_expression && is_static)
    {
        ptcl_expression *static_value = ptcl_expression_copy(left->ctor.values[index], location);
        if (static_value == NULL)
        {
            ptcl_expression_destroy(left);
            ptcl_parser_throw_out_of_memory(parser, location);
            return NULL;
        }

        static_value->return_type.is_static = true;
        ptcl_expression_destroy(left);
        return static_value;
    }

    ptcl_expression *dot_expr = ptcl_expression_create(
        ptcl_expression_dot_type,
        member->type,
        left->location);
    if (dot_expr == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, left->location);
        ptcl_expression_destroy(left);
        return NULL;
    }

    if (!is_expression && is_static)
    {
        dot_expr->return_type.is_static = true;
    }

    dot_expr->dot = ptcl_expression_dot_create(left, name);
    return dot_expr;
}

ptcl_expression *ptcl_parser_dot(ptcl_parser *parser, ptcl_type *expected, ptcl_expression *left, bool is_expression, bool with_word)
{
    if (left == NULL)
    {
        left = ptcl_parser_array_element(parser, expected, NULL, with_word);
    }

    if (ptcl_parser_critical(parser) ||
        ptcl_parser_ended(parser) ||
        (left->return_type.type != ptcl_value_typedata_type &&
         left->return_type.type != ptcl_value_type_type))
    {
        return left;
    }

    if (ptcl_parser_match(parser, ptcl_token_dot_type))
    {
        ptcl_name name = ptcl_parser_name_word(parser);
        ptcl_location location = ptcl_parser_current(parser).location;
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        ptcl_expression *result;
        if (left->return_type.type == ptcl_value_type_type)
        {
            result = ptcl_parser_type_dot(parser, left, name, is_expression, location);
        }
        else
        {
            result = ptcl_parser_typedata_dot(parser, left, name, is_expression, location);
        }

        if (ptcl_parser_critical(parser))
        {
            return NULL;
        }

        left = result;
        ptcl_token current = ptcl_parser_current(parser);
        if (current.type == ptcl_token_left_square_type)
        {
            left = ptcl_parser_array_element(parser, expected, left, false);
        }
        else if (current.type == ptcl_token_dot_type)
        {
            left = ptcl_parser_dot(parser, expected, left, is_expression, false);
        }
    }

    return left;
}

ptcl_expression *ptcl_parser_array_element(ptcl_parser *parser, ptcl_type *expected, ptcl_expression *left, bool with_word)
{
    if (left == NULL)
    {
        left = ptcl_parser_value(parser, expected, with_word);
    }

    if (ptcl_parser_critical(parser) ||
        (left->return_type.type != ptcl_value_array_type && left->return_type.type != ptcl_value_pointer_type) ||
        ptcl_parser_ended(parser))
    {
        return left;
    }

    if (ptcl_parser_match(parser, ptcl_token_left_square_type))
    {
        if (left->return_type.type == ptcl_value_pointer_type && left->return_type.pointer.is_any)
        {
            ptcl_parser_throw_except_type_specifier(parser, ptcl_parser_current(parser).location);
            ptcl_expression_destroy(left);
            return NULL;
        }

        ptcl_expression *index = ptcl_parser_cast(parser, &ptcl_type_integer, false);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        ptcl_parser_except(parser, ptcl_token_right_square_type);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(index);
            return NULL;
        }

        ptcl_type target;
        if (left->return_type.type == ptcl_value_pointer_type)
        {
            target = *left->return_type.pointer.target;
        }
        else
        {
            target = *left->return_type.array.target;
        }

        return ptcl_expression_array_element_create(left, target, index, index->location);
    }

    return left;
}

static ptcl_expression *ptcl_parser_try_name(ptcl_parser *parser, ptcl_location location)
{
    ptcl_name word_value = ptcl_parser_name_word(parser);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    ptcl_expression *word = ptcl_expression_word_create(word_value, location);
    if (word == NULL)
    {
        ptcl_name_destroy(word_value);
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    word->return_type.is_static = true;
    return word;
}

static ptcl_expression *ptcl_parser_at(ptcl_parser *parser, ptcl_type *expected, bool with_word, ptcl_location location)
{
    if (ptcl_parser_peek(parser, 1).type == ptcl_token_left_curly_type)
    {
        ptcl_parser_extra_body(parser, parser->syntax_depth > 0);
        if (ptcl_parser_critical(parser))
        {
            return NULL;
        }

        return ptcl_parser_cast(parser, expected, with_word);
    }

    ptcl_parser_throw_unknown_expression(parser, location);
    return NULL;
}

static ptcl_expression *ptcl_parser_lated_body_void(ptcl_parser *parser, ptcl_location location)
{
    ptcl_type *previous = parser->return_type;
    parser->return_type = &ptcl_type_void;
    size_t block_start = ptcl_parser_position(parser) - 1;

    ptcl_parser_skip_block_or_expression(parser);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    parser->return_type = previous;
    size_t token_count = ptcl_parser_position(parser) - block_start;
    size_t index = ptcl_parser_add_lated_body(parser, block_start, token_count, location);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    ptcl_type func = ptcl_type_create_func(&ptcl_type_void, NULL, 0);
    func.is_static = true;
    ptcl_expression *result = ptcl_expression_create(ptcl_expression_lated_func_body_type, func, location);
    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    result->lated_body.index = index;
    return result;
}

static ptcl_expression *ptcl_parser_try_ctor(ptcl_parser *parser, ptcl_name name, ptcl_parser_typedata *typedata, ptcl_location location)
{
    ptcl_expression *result = ptcl_expression_create(
        ptcl_expression_ctor_type, ptcl_type_create_typedata(typedata->name.value, typedata->name.is_anonymous), location);
    ptcl_name_destroy(name);
    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    result->ctor = ptcl_parser_ctor(parser, typedata->name, *typedata);
    if (ptcl_parser_critical(parser))
    {
        free(result);
        return NULL;
    }

    result->return_type.is_static = true;
    return result;
}

static ptcl_expression *ptcl_parser_try_func(ptcl_parser *parser, ptcl_name name, ptcl_parser_function *func_instance, ptcl_token current)
{
    ptcl_statement_func_call func_call = ptcl_parser_func_call(parser, func_instance, NULL, true);
    if (ptcl_parser_critical(parser))
    {
        ptcl_name_destroy(name);
        return NULL;
    }

    ptcl_expression *result;
    ptcl_name_destroy(name);
    if (func_call.is_built_in)
    {
        result = func_call.built_in;
    }
    else
    {
        result = ptcl_expression_create(
            ptcl_expression_func_call_type, func_call.return_type, current.location);
        if (result == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, current.location);
            ptcl_statement_func_call_destroy(func_call);
            return NULL;
        }
        else
        {
            result->func_call = func_call;
        }
    }

    return result;
}

static bool ptcl_parser_try_get_left_par(ptcl_parser *parser)
{
    if (ptcl_parser_ended(parser))
    {
        if (ptcl_parser_insert_states_count(parser) > 0)
        {
            ptcl_parser_insert_state *state = ptcl_parser_insert_state_at(parser, ptcl_parser_insert_states_count(parser) - 1);
            if (state->syntax_depth == parser->syntax_depth)
            {
                ptcl_parser_set_state(parser, ptcl_parser_insert_state_at(parser, --parser->insert_states_count)->state);
                bool result = ptcl_parser_try_get_left_par(parser);
                parser->insert_states_count++;
                return result;
            }
            else
            {
                goto syntax;
            }
        }
        else
        {
        syntax:
            if (parser->syntax_depth > 0)
            {
                ptcl_parser_tokens_state current = parser->state;
                ptcl_parser_syntax_pair syntax = parser->syntaxes_nodes[--parser->syntax_depth];
                parser->state = syntax.state;
                bool result = ptcl_parser_try_get_left_par(parser);
                parser->syntax_depth++;
                parser->state = current;
                return result;
            }
        }
    }

    return parser->state.tokens[parser->state.position].type == ptcl_token_left_par_type;
}

static ptcl_expression *ptcl_parser_func_call_or_var(ptcl_parser *parser, ptcl_name name, ptcl_token current, bool with_word)
{
    ptcl_location location = current.location;
    ptcl_parser_typedata *typedata;
    ptcl_parser_variable *variable;
    ptcl_expression *result = NULL;
    if (ptcl_parser_try_get_left_par(parser))
    {
        size_t start = ptcl_parser_position(parser);
        const bool last_state = parser->add_errors;
        parser->add_errors = !with_word;

        if (ptcl_parser_try_get_typedata(parser, name, &typedata))
        {
            result = ptcl_parser_try_ctor(parser, name, typedata, location);
        }
        else
        {
            ptcl_parser_function *func_instance;
            if (ptcl_parser_try_get_function(parser, name, &func_instance))
            {
                result = ptcl_parser_try_func(parser, name, func_instance, current);
            }
            else if (with_word)
            {
                result = ptcl_expression_word_create(ptcl_name_create_fast_w(current.value, false), current.location);
                ptcl_name_destroy(name);
                if (result == NULL)
                {
                    ptcl_parser_throw_out_of_memory(parser, location);
                    return NULL;
                }
                else
                {
                    result->return_type.is_static = true;
                }

                return result;
            }
            else
            {
                ptcl_parser_throw_unknown_function(parser, name.value, current.location);
                ptcl_name_destroy(name);
                return NULL;
            }
        }

        if (ptcl_parser_critical(parser) && with_word)
        {
            parser->is_critical = false;
            parser->add_errors = last_state;
            ptcl_parser_set_position(parser, start + 1);
            result = ptcl_expression_word_create(ptcl_name_create_fast_w(current.value, false), current.location);
            ptcl_name_destroy(name);
            if (result == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                return NULL;
            }
            else
            {
                result->return_type.is_static = true;
            }

            return result;
        }

        if (result == NULL && !ptcl_parser_critical(parser))
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        }

        return result;
    }
    else if (ptcl_parser_try_get_variable(parser, name, &variable))
    {
        if (variable->is_built_in && variable->built_in != NULL)
        {
            result = ptcl_expression_copy(variable->built_in, current.location);
            if (result != NULL)
            {
                if (result->return_type.is_static && result->return_type.type == ptcl_value_word_type)
                {
                    result->word.is_free = false;
                }

                result->return_type.is_static = variable->built_in->return_type.is_static || !variable->is_syntax_variable;
                ptcl_name_destroy(name);
            }
        }
        else
        {
            result = ptcl_expression_create_variable(name, variable->type, current.location);
            if (result == NULL)
            {
                ptcl_name_destroy(name);
            }
        }

        if (result == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        }

        return result;
    }

    ptcl_name_destroy(name);
    if (with_word)
    {
        result = ptcl_expression_word_create(ptcl_name_create_fast_w(current.value, false), current.location);
        if (result == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            return NULL;
        }
        else
        {
            result->return_type.is_static = true;
        }

        return result;
    }

    ptcl_parser_throw_unknown_variable(parser, current.value, location);
    return NULL;
}

static ptcl_expression *ptcl_parser_string(ptcl_parser *parser, ptcl_token current)
{
    size_t length = strlen(current.value);
    ptcl_expression **expressions = malloc((length + 1) * sizeof(ptcl_expression *));
    if (expressions == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, current.location);
        return NULL;
    }

    for (size_t i = 0; i < length; i++)
    {
        ptcl_expression *character = ptcl_expression_create_character(current.value[i], current.location);
        if (character == NULL)
        {
            for (size_t j = 0; j < i; j++)
            {
                ptcl_expression_destroy(expressions[j]);
            }

            free(expressions);
            ptcl_parser_throw_out_of_memory(parser, current.location);
            return NULL;
        }

        expressions[i] = character;
    }

    ptcl_expression *eof = ptcl_expression_create_character('\0', current.location);
    if (eof == NULL)
    {
        for (size_t i = 0; i < length; i++)
        {
            ptcl_expression_destroy(expressions[i]);
        }

        free(expressions);
        ptcl_parser_throw_out_of_memory(parser, current.location);
        return NULL;
    }

    expressions[length] = eof;
    ptcl_expression *result = ptcl_expression_create_characters(expressions, length + 1, current.location);
    if (result == NULL)
    {
        for (size_t i = 0; i < length; i++)
        {
            ptcl_expression_destroy(expressions[i]);
        }

        free(expressions);
        ptcl_parser_throw_out_of_memory(parser, current.location);
        return NULL;
    }

    result->return_type.is_static = true;
    return result;
}

static ptcl_expression *ptcl_parser_number(ptcl_parser *parser, ptcl_token current)
{
    ptcl_expression *result = NULL;
    ptcl_token suffix = ptcl_parser_current(parser);
    bool is_float_suffix = suffix.type == ptcl_token_word_type && strcmp(suffix.value, "f") == 0;
    if (ptcl_string_is_float(current.value))
    {
        double value = atof(current.value);
        if (is_float_suffix)
        {
            result = ptcl_expression_create_float((float)value, current.location);
        }
        else
        {
            result = ptcl_expression_create_double(value, current.location);
        }
    }
    else
    {
        long value = strtol(current.value, NULL, 10);
        if (is_float_suffix)
        {
            result = ptcl_expression_create_float((float)value, current.location);
        }
        else
        {
            result = ptcl_expression_create_integer(value, current.location);
        }
    }

    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, current.location);
        return NULL;
    }

    if (is_float_suffix)
    {
        ptcl_parser_skip(parser);
    }

    return result;
}

static ptcl_expression *ptcl_parser_type_object(ptcl_parser *parser, ptcl_location location)
{
    ptcl_type object_type = ptcl_parser_type(parser, false, false);
    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    ptcl_type *target = malloc(sizeof(ptcl_type));
    if (target == NULL)
    {
        ptcl_type_destroy(object_type);
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    *target = object_type;
    target->is_primitive = false;
    ptcl_expression *result = ptcl_expression_create_object_type(ptcl_type_create_object_type(target), object_type, location);
    if (result == NULL)
    {
        ptcl_type_destroy(object_type);
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    result->return_type.is_static = true;
    return result;
}

static ptcl_expression *ptcl_parse_none(ptcl_parser *parser, ptcl_type *expected, ptcl_location location)
{
    ptcl_expression *result = NULL;
    if (ptcl_parser_match(parser, ptcl_token_left_par_type))
    {
        ptcl_expression *default_value;
        if (ptcl_parser_match(parser, ptcl_token_greater_than_type))
        {
            ptcl_type type = ptcl_parser_type(parser, false, false);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            default_value = ptcl_parser_get_default(parser, type, location);
            ptcl_type_destroy(type);
        }
        else
        {
            ptcl_expression *excepted = ptcl_parser_cast(parser, NULL, false);
            if (ptcl_parser_critical(parser))
            {
                return NULL;
            }

            default_value = ptcl_parser_get_default(parser, excepted->return_type, location);
            ptcl_expression_destroy(excepted);
        }

        if (ptcl_parser_critical(parser))
        {
            return NULL;
        }

        ptcl_parser_except(parser, ptcl_token_right_par_type);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(default_value);
            return NULL;
        }

        result = default_value;
    }
    else
    {
        if (expected == NULL)
        {
            ptcl_parser_throw_none_type(parser, location);
            return NULL;
        }

        result = ptcl_parser_get_default(parser, *expected, location);
    }

    if (ptcl_parser_critical(parser))
    {
        return NULL;
    }

    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
    }

    return result;
}

ptcl_expression *ptcl_parser_value(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
    if (parser->is_syntax_body && ptcl_parser_parse_try_syntax_usage_here(parser, false))
    {
        size_t depth = parser->syntax_depth;
        ptcl_expression *expression = ptcl_parser_cast(parser, expected, with_word);
        if (parser->syntax_depth == depth)
        {
            ptcl_parser_leave_from_syntax(parser);
        }

        return expression;
    }

    const bool is_anonymous = ptcl_parser_match(parser, ptcl_token_tilde_type);
    ptcl_token current = ptcl_parser_current(parser);
    ptcl_location location = current.location;
    if (expected != NULL && expected->type == ptcl_value_word_type)
    {
        return ptcl_parser_try_name(parser, location);
    }

    ptcl_parser_skip(parser);
    switch (current.type)
    {
    case ptcl_token_at_type:
        ptcl_parser_back(parser);
        return ptcl_parser_at(parser, expected, with_word, location);
    case ptcl_token_left_curly_type:
        return ptcl_parser_lated_body_void(parser, location);
    case ptcl_token_exclamation_mark_type:
        ptcl_parser_back(parser);
        ptcl_name word_name = ptcl_parser_name_word(parser);
        if (ptcl_parser_critical(parser))
        {
            return NULL;
        }

        if (with_word)
        {
            ptcl_expression *word = ptcl_expression_word_create(word_name, location);
            if (word == NULL)
            {
                ptcl_name_destroy(word_name);
                ptcl_parser_throw_out_of_memory(parser, location);
                return NULL;
            }

            word->return_type.is_static = true;
            return word;
        }
    case ptcl_token_word_type:
        if (current.type != ptcl_token_exclamation_mark_type)
        {
            word_name = ptcl_name_create_fast_w(current.value, is_anonymous);
        }

        return ptcl_parser_func_call_or_var(parser, word_name, current, with_word);
    case ptcl_token_word_word_type:
        ptcl_parser_back(parser);
        ptcl_name value = ptcl_parser_name_word(parser);
        ptcl_expression *word_expr = ptcl_expression_word_create(value, current.location);
        if (word_expr == NULL)
        {
            word_expr->return_type.is_static = true;
        }
        else
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        }

        return word_expr;
    case ptcl_token_string_type:
        return ptcl_parser_string(parser, current);
    case ptcl_token_number_type:
        return ptcl_parser_number(parser, current);
    case ptcl_token_character_type:
    {
        ptcl_expression *character = ptcl_expression_create_character(current.value[0], current.location);
        if (character == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        }

        return character;
    }
    case ptcl_token_static_type:
        ptcl_parser_except(parser, ptcl_token_if_type);
        if (ptcl_parser_critical(parser))
        {
            return NULL;
        }
    case ptcl_token_if_type:
    {
        bool is_static = current.type == ptcl_token_static_type;
        return ptcl_parser_if_expression(parser, is_static);
    }
    case ptcl_token_left_par_type:
    {
        ptcl_expression *inside = ptcl_parser_cast(parser, expected, with_word);
        if (ptcl_parser_critical(parser))
        {
            return NULL;
        }

        ptcl_parser_except(parser, ptcl_token_right_par_type);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(inside);
        }

        return inside;
    }
    case ptcl_token_greater_than_type:
        return ptcl_parser_type_object(parser, location);
    case ptcl_token_null_type:
    {
        ptcl_expression *null_expr = ptcl_expression_create_null(current.location);
        if (null_expr == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        }

        return null_expr;
    }
    case ptcl_token_none_type:
        return ptcl_parse_none(parser, expected, location);
    default:
    {
        const bool last_state = parser->add_errors;
        parser->add_errors = false;
        ptcl_parser_back(parser);
        ptcl_expression *type = ptcl_parser_type_object(parser, location);
        parser->add_errors = last_state;
        if (ptcl_parser_critical(parser))
        {
            ptcl_parser_throw_unknown_expression(parser, location);
        }

        return type;
    }
    }

    return NULL;
}

ptcl_expression_ctor ptcl_parser_ctor(ptcl_parser *parser, ptcl_name name, ptcl_parser_typedata typedata)
{
    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_expression_ctor){0};
    }

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_expression_ctor ctor = ptcl_expression_ctor_create(name, NULL, 0);
    ctor.values = malloc(typedata.count * sizeof(ptcl_expression *));
    if (ctor.values == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        return (ptcl_expression_ctor){0};
    }

    bool all_none = false;
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser) || ctor.count > typedata.count)
        {
            ptcl_expression_ctor_destroy(ctor);
            ptcl_parser_throw_except_token(
                parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            return (ptcl_expression_ctor){0};
        }

        if (ctor.count == 0 && ptcl_parser_current(parser).type == ptcl_token_exclamation_mark_type && ptcl_parser_peek(parser, 1).type == ptcl_token_none_type)
        {
            ptcl_parser_skip(parser);
            ptcl_parser_skip(parser);
            ptcl_parser_except(parser, ptcl_token_right_par_type);
            if (ptcl_parser_critical(parser))
            {
                ptcl_expression_ctor_destroy(ctor);
                return (ptcl_expression_ctor){0};
            }

            all_none = true;
            break;
        }

        // ptcl_parser_try_parse_insert(parser);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_ctor_destroy(ctor);
            return (ptcl_expression_ctor){0};
        }

        ptcl_expression *value = ptcl_parser_cast(parser, &typedata.members[ctor.count].type, false);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_ctor_destroy(ctor);
            return (ptcl_expression_ctor){0};
        }

        ctor.values[ctor.count++] = value;
        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    if (all_none)
    {
        ctor.count = typedata.count;
        for (size_t i = 0; i < ctor.count; i++)
        {
            ptcl_expression *default_value = ptcl_parser_get_default(parser, typedata.members[i].type, ptcl_parser_current(parser).location);
            if (ptcl_parser_critical(parser))
            {
                ctor.count = i;
                ptcl_expression_ctor_destroy(ctor);
                return (ptcl_expression_ctor){0};
            }

            ctor.values[i] = default_value;
        }
    }
    else
    {
        if (ctor.count < typedata.count)
        {
            ptcl_expression_ctor_destroy(ctor);
            ptcl_parser_throw_except_token(
                parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_comma_type), location);
            return (ptcl_expression_ctor){0};
        }
    }

    return ctor;
}

ptcl_name ptcl_parser_name_word(ptcl_parser *parser)
{
    return ptcl_parser_name(parser, false);
}

ptcl_name ptcl_parser_name_word_or_type(ptcl_parser *parser)
{
    return ptcl_parser_name(parser, true);
}

static ptcl_name ptcl_name_expansion(ptcl_parser *parser, bool with_type)
{
    ptcl_parser_skip(parser);
    const bool is_anonymous = ptcl_parser_match(parser, ptcl_token_tilde_type);
    ptcl_token current = ptcl_parser_current(parser);
    ptcl_parser_skip(parser);
    if (current.type == ptcl_token_less_than_type)
    {
        ptcl_name left = ptcl_parser_name(parser, false);
        if (ptcl_parser_critical(parser))
        {
            return (ptcl_name){0};
        }

        ptcl_name right = ptcl_parser_name(parser, false);
        if (ptcl_parser_critical(parser))
        {
            ptcl_name_destroy(left);
            return (ptcl_name){0};
        }

        ptcl_parser_except(parser, ptcl_token_greater_than_type);
        if (ptcl_parser_critical(parser))
        {
            ptcl_name_destroy(left);
            ptcl_name_destroy(right);
            return (ptcl_name){0};
        }

        char *concatenated = ptcl_string(left.value, right.value, NULL);
        ptcl_name_destroy(left);
        ptcl_name_destroy(right);
        if (concatenated == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, left.location);
            return (ptcl_name){0};
        }

        return ptcl_name_create(concatenated, true, left.location);
    }

    ptcl_parser_variable *instance;
    ptcl_name name = ptcl_name_create_fast_w(current.value, is_anonymous);
    if (!ptcl_parser_try_get_variable(parser, name, &instance))
    {
        ptcl_parser_throw_unknown_variable(parser, name.value, current.location);
        return (ptcl_name){0};
    }

    bool incorrect_type = false;
    if (!instance->type.is_static)
    {
        incorrect_type = true;
    }
    else
    {
        switch (instance->type.type)
        {
        case ptcl_value_object_type_type:
            if (!with_type)
            {
                incorrect_type = true;
                break;
            }

            ptcl_name result = instance->name;
        case ptcl_value_word_type:
            if (instance->type.type == ptcl_value_word_type)
            {
                result = instance->built_in->word;
            }

            return ptcl_name_create(result.value, false, current.location);
        default:
            incorrect_type = true;
            break;
        }
    }

    if (incorrect_type)
    {
        ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_word, instance->type, current.location);
        return (ptcl_name){0};
    }

    return (ptcl_name){0};
}

static ptcl_name ptcl_parser_to_word(ptcl_parser *parser)
{
    ptcl_parser_skip(parser);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_name){0};
    }

    ptcl_expression *object_type = NULL;
    ptcl_name target;
    if (ptcl_parser_current(parser).type == ptcl_token_greater_than_type)
    {
        object_type = ptcl_parser_value(parser, NULL, false);
        if (ptcl_parser_critical(parser))
        {
            return (ptcl_name){0};
        }

        ptcl_parser_except(parser, ptcl_token_right_par_type);
        if (ptcl_parser_critical(parser))
        {
            ptcl_expression_destroy(object_type);
            return (ptcl_name){0};
        }

        char *value = ptcl_type_to_word_copy(object_type->object_type.type);
        ptcl_expression_destroy(object_type);
        if (value == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_name){0};
        }

        return ptcl_name_create(value, true, location);
    }
    else
    {
        target = ptcl_parser_name_word(parser);
        if (ptcl_parser_critical(parser))
        {
            return (ptcl_name){0};
        }
    }

    ptcl_parser_except(parser, ptcl_token_right_par_type);
    if (ptcl_parser_critical(parser))
    {
        ptcl_name_destroy(target);
        return (ptcl_name){0};
    }

    ptcl_parser_variable *variable;
    if (!ptcl_parser_try_get_variable(parser, target, &variable))
    {
        ptcl_parser_throw_unknown_variable(parser, target.value, location);
        ptcl_name_destroy(target);
        return (ptcl_name){0};
    }

    bool incorrect_type = false;
    bool is_free = false;
    char *value;
    if (!variable->type.is_static)
    {
        incorrect_type = true;
    }
    else
    {
        switch (variable->type.type)
        {
        case ptcl_value_object_type_type:
            is_free = true;
            value = ptcl_type_to_word_copy(variable->built_in->object_type.type);
            if (value == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                ptcl_name_destroy(target);
                return (ptcl_name){0};
            }

            break;
        case ptcl_value_integer_type:
            is_free = true;
            char *number = ptcl_from_int(variable->built_in->integer_n);
            if (number == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                ptcl_name_destroy(target);
                return (ptcl_name){0};
            }

            value = ptcl_string("_", number, NULL);
            free(number);
            if (value == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                ptcl_name_destroy(target);
                return (ptcl_name){0};
            }

            break;
        case ptcl_value_word_type:
            value = variable->built_in->word.value;
            break;
        default:
            incorrect_type = true;
            break;
        }
    }

    if (incorrect_type)
    {
        ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_word, variable->type, location);
        ptcl_name_destroy(target);
        return (ptcl_name){0};
    }

    ptcl_name_destroy(target);
    return ptcl_name_create(value, is_free, location);
}

ptcl_name ptcl_parser_name(ptcl_parser *parser, bool with_type)
{
    ptcl_token operator = ptcl_parser_current(parser);
    switch (operator.type)
    {
    case ptcl_token_exclamation_mark_type:
        return ptcl_name_expansion(parser, with_type);
    case ptcl_token_word_word_type:
        return ptcl_parser_to_word(parser);
    case ptcl_token_hashtag_type:
    {
        size_t start = ptcl_parser_position(parser);
        if (!ptcl_parser_parse_try_syntax_usage_here(parser, false))
        {
            return (ptcl_name){0};
        }

        ptcl_parser_set_position(parser, start);
        ptcl_name name = ptcl_parser_name(parser, with_type);
        ptcl_parser_leave_from_syntax(parser);
        return name;
    }
    default:
        break;
    }

    const bool is_anonymous = ptcl_parser_match(parser, ptcl_token_tilde_type);
    ptcl_token token = ptcl_parser_except(parser, ptcl_token_word_type);
    if (ptcl_parser_critical(parser))
    {
        return (ptcl_name){0};
    }

    return ptcl_name_create_l(token.value, is_anonymous, false, token.location);
}

ptcl_expression *ptcl_parser_get_default(ptcl_parser *parser, ptcl_type type, ptcl_location location)
{
    ptcl_expression *result = NULL;
    switch (type.type)
    {
    case ptcl_value_void_type:
    case ptcl_value_array_type:
    case ptcl_value_type_type:
        if (type.comp_type->count == 0)
        {
            ptcl_parser_throw_fast_incorrect_type(parser, type, ptcl_type_any, location);
            return NULL;
        }

        return ptcl_parser_get_default(parser, type.comp_type->types[0].type, location);
    case ptcl_value_typedata_type:
    {
        ptcl_parser_typedata *instance;
        // If type was created, then type registered
        ptcl_parser_try_get_typedata(parser, type.typedata, &instance);
        ptcl_expression **expressions = malloc(instance->count * sizeof(ptcl_expression *));
        if (expressions == NULL)
        {
            return NULL;
        }

        for (size_t i = 0; i < instance->count; i++)
        {
            ptcl_expression *default_value = ptcl_parser_get_default(parser, instance->members[i].type, location);
            if (ptcl_parser_critical(parser))
            {
                for (size_t j = 0; j < i - 1; j++)
                {
                    ptcl_expression_destroy(expressions[j]);
                }

                free(expressions);
                return NULL;
            }

            expressions[i] = default_value;
        }

        ptcl_name typedata_name = instance->name;
        typedata_name.is_free = false;
        ptcl_expression *ctor = ptcl_expression_create(ptcl_expression_ctor_type, ptcl_type_create_typedata(type.typedata.value, false), location);
        if (ctor == NULL)
        {
            for (size_t i = 0; i < instance->count; i++)
            {
                ptcl_expression_destroy(expressions[i]);
            }

            free(expressions);
            return NULL;
        }

        ctor->ctor = ptcl_expression_ctor_create(typedata_name, expressions, instance->count);
        return ctor;
    }
    case ptcl_value_function_pointer_type:
    case ptcl_value_pointer_type:
        result = ptcl_expression_create_null(location);
        break;
    case ptcl_value_any_type:
    case ptcl_value_object_type_type:
        result = ptcl_expression_create_object_type(ptcl_type_create_object_type(&ptcl_type_any_type), ptcl_type_any_type, location);
        break;
    case ptcl_value_word_type:
        result = ptcl_expression_word_create(ptcl_name_create_fast_w("", false), location);
        break;
    case ptcl_value_double_type:
        result = ptcl_expression_create_double(0, location);
        break;
    case ptcl_value_float_type:
        result = ptcl_expression_create_float(0, location);
        break;
    case ptcl_value_integer_type:
        result = ptcl_expression_create_integer(0, location);
        break;
    case ptcl_value_character_type:
        result = ptcl_expression_create_character('\0', location);
        break;
    }

    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
    }

    return result;
}

ptcl_token ptcl_parser_except(ptcl_parser *parser, ptcl_token_type token_type)
{
    ptcl_token current = ptcl_parser_current(parser);
    if (current.type == token_type)
    {
        ptcl_parser_skip(parser);
        return current;
    }

    char *value;
    switch (token_type)
    {
    case ptcl_token_word_type:
        value = "variable, function or type";
        break;
    default:
        value = ptcl_lexer_configuration_get_value(parser->configuration, token_type);
        break;
    }

    ptcl_parser_throw_except_token(parser, value, current.location);
    return current;
}

bool ptcl_parser_match(ptcl_parser *parser, ptcl_token_type token_type)
{
    if (ptcl_parser_count(parser) == 0)
    {
        return false;
    }

    if (ptcl_parser_current(parser).type == token_type)
    {
        ptcl_parser_skip(parser);
        return true;
    }

    return false;
}

ptcl_token ptcl_parser_peek(ptcl_parser *parser, size_t offset)
{
    size_t position = ptcl_parser_position(parser) + offset;
    size_t count = ptcl_parser_count(parser) - 1;
    if (position > count)
    {
        return parser->state.tokens[count];
    }

    return parser->state.tokens[position];
}

ptcl_token ptcl_parser_current(ptcl_parser *parser)
{
    ptcl_token *tokens = ptcl_parser_tokens(parser);
    size_t count = ptcl_parser_count(parser);
    if (ptcl_parser_ended(parser))
    {
        if (ptcl_parser_insert_states_count(parser) > 0)
        {
            ptcl_parser_insert_state *state = ptcl_parser_insert_state_at(parser, ptcl_parser_insert_states_count(parser) - 1);
            if (state->syntax_depth == parser->syntax_depth)
            {
                ptcl_parser_leave_from_insert_state(parser);
                return ptcl_parser_tokens(parser)[ptcl_parser_position(parser)];
            }
        }

        if (parser->syntax_depth > 0)
        {
            ptcl_parser_leave_from_syntax(parser);
            return ptcl_parser_current(parser);
        }

        if (count > 0)
        {
            return tokens[count - 1];
        }
    }

    return tokens[ptcl_parser_position(parser)];
}

inline ptcl_parser_tokens_state ptcl_parser_state(ptcl_parser *parser)
{
    return parser->state;
}

inline void ptcl_parser_set_state(ptcl_parser *parser, ptcl_parser_tokens_state state)
{
    parser->state = state;
}

inline ptcl_token *ptcl_parser_tokens(ptcl_parser *parser)
{
    return parser->state.tokens;
}

void ptcl_parser_set_tokens(ptcl_parser *parser, ptcl_token *tokens)
{
    parser->state.tokens = tokens;
}

inline size_t ptcl_parser_count(ptcl_parser *parser)
{
    return parser->state.count;
}

inline void ptcl_parser_set_count(ptcl_parser *parser, size_t count)
{
    parser->state.count = count;
}

inline size_t ptcl_parser_position(ptcl_parser *parser)
{
    return parser->state.position;
}

inline void ptcl_parser_set_position(ptcl_parser *parser, size_t position)
{
    parser->state.position = position;
}

inline void ptcl_parser_skip(ptcl_parser *parser)
{
    parser->state.position++;
}

inline void ptcl_parser_back(ptcl_parser *parser)
{
    parser->state.position--;
}

inline bool ptcl_parser_ended(ptcl_parser *parser)
{
    if (ptcl_parser_count(parser) == 0)
    {
        return true;
    }

    return ptcl_parser_position(parser) > ptcl_parser_count(parser) - 1;
}

inline bool ptcl_parser_critical(ptcl_parser *parser)
{
    return parser->is_critical;
}

size_t ptcl_parser_insert_states_count(ptcl_parser *parser)
{
    return parser->insert_states_count;
}

ptcl_parser_insert_state *ptcl_parser_insert_state_at(ptcl_parser *parser, size_t index)
{
    return &parser->insert_states[index];
}

void ptcl_parser_set_ignore_error_func_mode(ptcl_parser *parser, bool mode)
{
    parser->is_ignore_error = mode;
}

ptcl_statement_func_body *ptcl_parser_root(ptcl_parser *parser)
{
    return parser->root;
}

size_t ptcl_parser_variables_count(ptcl_parser *parser)
{
    return parser->variables_count;
}

ptcl_parser_variable *ptcl_parser_variables(ptcl_parser *parser)
{
    return parser->variables;
}

bool ptcl_parser_add_this_pair(ptcl_parser *parser, ptcl_parser_this_s_pair instance)
{
    ptcl_parser_this_s_pair *buffer = realloc(parser->this_pairs, (parser->this_pairs_count + 1) * sizeof(ptcl_parser_this_s_pair));
    if (buffer == NULL)
    {
        return false;
    }

    parser->this_pairs = buffer;
    parser->this_pairs[parser->this_pairs_count++] = instance;
    return true;
}

ptcl_statement *ptcl_parser_insert_pairs(ptcl_parser *parser, ptcl_statement *statement, ptcl_statement_func_body *body)
{
    for (int i = parser->this_pairs_count - 1; i >= 0; i--)
    {
        ptcl_parser_this_s_pair *target = &parser->this_pairs[i];
        if (!ptcl_func_body_can_access(body, target->body))
        {
            continue;
        }

        ptcl_location location = ptcl_parser_current(parser).location;
        if (statement->type != ptcl_statement_func_body_type)
        {
            ptcl_statement *statement_body = ptcl_statement_create(ptcl_statement_func_body_type, body, ptcl_attributes_create(NULL, 0), location);
            if (statement_body == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                ptcl_statement_destroy(statement);
                return NULL;
            }

            statement_body->body.count = 1;
            statement_body->body.statements = malloc((statement_body->body.count + 1) * sizeof(ptcl_statement *));
            if (statement_body->body.statements == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                ptcl_statement_destroy(statement);
                free(body);
                return NULL;
            }

            statement_body->body.statements[0] = statement;
        }
        else
        {
            ptcl_statement **buffer = realloc(statement->body.statements, (statement->body.count + 1) * sizeof(ptcl_statement *));
            if (buffer == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                ptcl_statement_destroy(statement);
                return NULL;
            }

            statement->body.statements = buffer;
        }

        const ptcl_parser_tokens_state last_state = parser->state;
        parser->state = target->state;
        ptcl_statement_func_body injected_body = ptcl_parser_func_body(parser, false, false, parser->is_ignore_error);
        if (ptcl_parser_critical(parser))
        {
            ptcl_statement_destroy(statement);
            return NULL;
        }

        ptcl_statement *statement_body = ptcl_statement_create(ptcl_statement_func_body_type, body, ptcl_attributes_create(NULL, 0), location);
        if (statement_body == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_statement_destroy(statement);
            return NULL;
        }

        statement_body->body = injected_body;
        statement->body.statements[statement->body.count] = statement_body;
    }

    return statement;
}

bool ptcl_parser_add_instance_syntax(ptcl_parser *parser, ptcl_parser_syntax instance)
{
    if (instance.root != NULL && parser->syntax_depth > 0 && !instance.name.is_anonymous)
    {
        instance.root = parser->main_root;
    }

    if (parser->syntaxes_count >= parser->syntaxes_capacity)
    {
        parser->syntaxes_capacity *= 2;
        ptcl_parser_syntax *buffer = realloc(parser->syntaxes, parser->syntaxes_capacity * sizeof(ptcl_parser_syntax));
        if (buffer == NULL)
        {
            return false;
        }

        parser->syntaxes = buffer;
    }

    parser->syntaxes[parser->syntaxes_count++] = instance;
    return true;
}

bool ptcl_parser_add_instance_comp_type(ptcl_parser *parser, ptcl_parser_comp_type instance)
{
    if (instance.root != NULL && parser->syntax_depth > 0 && !instance.identifier.is_anonymous)
    {
        instance.root = parser->main_root;
    }

    ptcl_parser_comp_type *target;
    if (ptcl_parser_try_get_any_comp_type(parser, instance.identifier, &target))
    {
        if (instance.comp_type == NULL)
        {
            if (target->static_type != NULL)
            {
                return false;
            }

            target->static_type = instance.static_type;
            target->static_type->has_invariant = target->comp_type != NULL;
        }
        else
        {
            if (target->comp_type != NULL)
            {
                return false;
            }

            target->comp_type = instance.comp_type;
            if (target->static_type != NULL)
            {
                target->static_type->has_invariant = true;
            }
        }

        return true;
    }

    if (parser->comp_types_count >= parser->comp_types_capacity)
    {
        parser->comp_types_capacity *= 2;
        ptcl_parser_comp_type *buffer = realloc(parser->comp_types, parser->comp_types_capacity * sizeof(ptcl_parser_comp_type));
        if (buffer == NULL)
        {
            return false;
        }

        parser->comp_types = buffer;
    }

    parser->comp_types[parser->comp_types_count++] = instance;
    return true;
}

bool ptcl_parser_add_instance_typedata(ptcl_parser *parser, ptcl_parser_typedata instance)
{
    if (instance.root != NULL && parser->syntax_depth > 0 && !instance.name.is_anonymous)
    {
        instance.root = parser->main_root;
    }

    if (parser->typedatas_count >= parser->typedatas_capacity)
    {
        parser->typedatas_capacity *= 2;
        ptcl_parser_typedata *buffer = realloc(parser->typedatas, parser->typedatas_capacity * sizeof(ptcl_parser_typedata));
        if (buffer == NULL)
        {
            return false;
        }

        parser->typedatas = buffer;
    }

    parser->typedatas[parser->typedatas_count++] = instance;
    return true;
}

bool ptcl_parser_add_instance_function(ptcl_parser *parser, ptcl_parser_function instance)
{
    if (instance.root != NULL && parser->syntax_depth > 0 && !instance.name.is_anonymous)
    {
        instance.root = parser->main_root;
    }

    if (parser->functions_count >= parser->functions_capacity)
    {
        parser->functions_capacity *= 2;
        ptcl_parser_function *buffer = realloc(parser->functions, parser->functions_capacity * sizeof(ptcl_parser_function));
        if (buffer == NULL)
        {
            return false;
        }

        parser->functions = buffer;
    }

    parser->functions[parser->functions_count++] = instance;
    return true;
}

bool ptcl_parser_add_instance_variable(ptcl_parser *parser, ptcl_parser_variable instance)
{
    if (instance.root != NULL && parser->syntax_depth > 0 && !instance.name.is_anonymous && !instance.is_syntax_variable)
    {
        instance.root = parser->main_root;
    }

    if (parser->variables_count >= parser->variables_capacity)
    {
        parser->variables_capacity *= 2;
        ptcl_parser_variable *buffer = realloc(parser->variables, parser->variables_capacity * sizeof(ptcl_parser_variable));
        if (buffer == NULL)
        {
            return false;
        }

        parser->variables = buffer;
    }

    parser->variables[parser->variables_count++] = instance;
    return true;
}

bool ptcl_parser_try_get_syntax(ptcl_parser *parser, ptcl_name name, ptcl_parser_syntax **instance)
{
    for (int i = parser->syntaxes_count - 1; i >= 0; i--)
    {
        ptcl_parser_syntax *syntax = &parser->syntaxes[i];
        if (syntax->is_out_of_scope || !ptcl_name_compare(syntax->name, name) || !ptcl_func_body_can_access(syntax->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        *instance = syntax;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_comp_type(ptcl_parser *parser, ptcl_name name, bool is_static, ptcl_parser_comp_type **instance)
{
    for (int i = parser->comp_types_count - 1; i >= 0; i--)
    {
        ptcl_parser_comp_type *comp_type = &parser->comp_types[i];
        if (comp_type->is_out_of_scope)
        {
            continue;
        }

        ptcl_type_comp_type *target = is_static ? comp_type->static_type : comp_type->comp_type;
        if (target == NULL ||
            !ptcl_name_compare(comp_type->identifier, name) ||
            !ptcl_func_body_can_access(comp_type->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        *instance = comp_type;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_any_comp_type(ptcl_parser *parser, ptcl_name name, ptcl_parser_comp_type **instance)
{
    for (int i = parser->comp_types_count - 1; i >= 0; i--)
    {
        ptcl_parser_comp_type *comp_type = &parser->comp_types[i];
        if (comp_type->is_out_of_scope || !ptcl_name_compare(comp_type->identifier, name) || !ptcl_func_body_can_access(comp_type->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        *instance = comp_type;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_typedata(ptcl_parser *parser, ptcl_name name, ptcl_parser_typedata **instance)
{
    for (int i = parser->typedatas_count - 1; i >= 0; i--)
    {
        ptcl_parser_typedata *typedata = &parser->typedatas[i];
        if (typedata->is_out_of_scope || !ptcl_name_compare(typedata->name, name) || !ptcl_func_body_can_access(typedata->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        *instance = typedata;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_function(ptcl_parser *parser, ptcl_name name, ptcl_parser_function **instance)
{
    for (int i = parser->functions_count - 1; i >= 0; i--)
    {
        ptcl_parser_function *function = &parser->functions[i];
        if (function->is_out_of_scope || !ptcl_name_compare(function->name, name) || !ptcl_func_body_can_access(function->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        *instance = function;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_variable(ptcl_parser *parser, ptcl_name name, ptcl_parser_variable **instance)
{
    for (int i = parser->variables_count - 1; i >= 0; i--)
    {
        ptcl_parser_variable *variable = &parser->variables[i];
        if (variable->is_out_of_scope || !ptcl_name_compare(variable->name, name) || !ptcl_func_body_can_access(variable->root, ptcl_parser_root(parser)))
        {
            continue;
        }

        *instance = variable;
        return true;
    }

    return false;
}

int ptcl_parser_add_lated_body(ptcl_parser *parser, size_t start, size_t tokens_count, ptcl_location location)
{
    ptcl_token *body_tokens = NULL;
    if (tokens_count > 0)
    {
        body_tokens = malloc(sizeof(ptcl_token) * tokens_count);
        if (body_tokens == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            return -1;
        }

        for (size_t i = 0; i < tokens_count; i++)
        {
            body_tokens[i] = ptcl_parser_tokens(parser)[start + i];
        }
    }

    if (parser->lated_states_count >= parser->lated_states_capacity)
    {
        size_t new_capacity = parser->lated_states_capacity == 0 ? 8 : parser->lated_states_capacity * 2;
        ptcl_parser_tokens_state *buffer = realloc(parser->lated_states,
                                                   sizeof(ptcl_parser_tokens_state) * new_capacity);
        if (buffer == NULL)
        {
            for (size_t i = 0; i < tokens_count; i++)
            {
                ptcl_token_destroy(body_tokens[i]);
            }

            free(body_tokens);
            ptcl_parser_throw_out_of_memory(parser, location);
            return -1;
        }

        parser->lated_states = buffer;
        parser->lated_states_capacity = new_capacity;
    }

    ptcl_parser_tokens_state *body = &parser->lated_states[parser->lated_states_count];
    body->tokens = body_tokens;
    body->count = tokens_count;
    return parser->lated_states_count++;
}

bool ptcl_parser_is_syntax_defined(ptcl_parser *parser, ptcl_name name)
{
    void *placeholder = NULL;
    return ptcl_parser_try_get_syntax(parser, name, &placeholder);
}

bool ptcl_parser_is_comp_type_defined(ptcl_parser *parser, ptcl_name name, bool is_static)
{
    void *placeholder = NULL;
    return ptcl_parser_try_get_comp_type(parser, name, is_static, &placeholder);
}

bool ptcl_parser_is_typedata_defined(ptcl_parser *parser, ptcl_name name)
{
    void *placeholder = NULL;
    return ptcl_parser_try_get_typedata(parser, name, &placeholder);
}

bool ptcl_parser_is_function_defined(ptcl_parser *parser, ptcl_name name)
{
    void *placeholder = NULL;
    return ptcl_parser_try_get_function(parser, name, &placeholder);
}

bool ptcl_parser_is_variable_defined(ptcl_parser *parser, ptcl_name name)
{
    void *placeholder = NULL;
    return ptcl_parser_try_get_variable(parser, name, &placeholder);
}

bool ptcl_parser_is_defined(ptcl_parser *parser, ptcl_name name)
{
    void *placeholder = NULL;
    if (ptcl_parser_try_get_syntax(parser, name, &placeholder))
    {
        return true;
    }
    else if (ptcl_parser_try_get_any_comp_type(parser, name, &placeholder))
    {
        return true;
    }
    else if (ptcl_parser_try_get_typedata(parser, name, &placeholder))
    {
        return true;
    }
    else if (ptcl_parser_try_get_function(parser, name, &placeholder))
    {
        return true;
    }
    else if (ptcl_parser_try_get_variable(parser, name, &placeholder))
    {
        return true;
    }

    return false;
}

bool ptcl_parser_check_arguments(ptcl_parser *parser, ptcl_parser_function *function, ptcl_expression **arguments, size_t count)
{
    if (function->func.is_variadic)
    {
        if (function->func.count - 1 > count)
        {
            return false;
        }
    }
    else
    {
        if (function->func.count > count)
        {
            return false;
        }
    }

    for (size_t j = 0; j < function->func.count; j++)
    {
        ptcl_argument argument = function->func.arguments[j];
        if (argument.is_variadic)
        {
            break;
        }

        if (ptcl_type_is_castable(argument.type, arguments[j]->return_type))
        {
            continue;
        }

        return false;
    }

    return true;
}

static bool compare_syntax_nodes(ptcl_parser_syntax_node *left, ptcl_parser_syntax_node *right)
{
    switch (left->type)
    {
    case ptcl_parser_syntax_node_word_type:
        if (right->type != ptcl_parser_syntax_node_word_type)
        {
            return false;
        }

        return ptcl_name_compare(left->word.name, right->word.name);
    case ptcl_parser_syntax_node_variable_type:
        return right->type == ptcl_parser_syntax_node_value_type &&
               ptcl_type_is_castable(left->variable.type, right->value->return_type);
    case ptcl_parser_syntax_node_object_type_type:
        if (right->type != ptcl_parser_syntax_node_value_type)
        {
            return false;
        }

        return ptcl_type_is_castable(left->object_type, right->value->return_type);
    default:
        return false;
    }
}

bool ptcl_parser_syntax_try_find(ptcl_parser *parser, ptcl_parser_syntax_node *nodes, size_t count,
                                 ptcl_parser_syntax **syntax, bool *can_continue, char **end_token)
{
    *syntax = NULL;
    *can_continue = false;
    *end_token = NULL;
    bool full_match_found = false;
    for (int i = parser->syntaxes_count - 1; i >= 0; i--)
    {
        ptcl_parser_syntax *target = &parser->syntaxes[i];
        if (target->is_out_of_scope || target->count != count)
        {
            continue;
        }

        bool match = true;
        for (size_t j = 0; j < count; j++)
        {
            if (!compare_syntax_nodes(&target->nodes[j], &nodes[j]))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            *syntax = target;
            full_match_found = true;
            break;
        }
    }

    for (int i = parser->syntaxes_count - 1; i >= 0; i--)
    {
        ptcl_parser_syntax *target = &parser->syntaxes[i];
        if (target->is_out_of_scope || target->count <= count)
        {
            continue;
        }

        bool partial_match = true;
        for (size_t j = 0; j < count; j++)
        {
            if (!compare_syntax_nodes(&target->nodes[j], &nodes[j]))
            {
                partial_match = false;
                break;
            }
        }

        if (partial_match)
        {
            if (target->count > count)
            {
                ptcl_parser_syntax_node last = target->nodes[count];
                if (last.type == ptcl_parser_syntax_node_variable_type && last.variable.is_variadic)
                {
                    *end_token = target->nodes[count + 1].word.name.value;
                }
            }

            *can_continue = true;
            break;
        }
    }

    return full_match_found;
}

bool ptcl_parser_try_get_typedata_member(ptcl_parser *parser, ptcl_name name, char *member_name, ptcl_argument **member, size_t *index)
{
    ptcl_parser_typedata *typedata;
    if (!ptcl_parser_try_get_typedata(parser, name, &typedata))
    {
        return false;
    }

    for (size_t i = 0; i < typedata->count; i++)
    {
        ptcl_argument *typedata_member = &typedata->members[i];
        if (strcmp(typedata_member->name.value, member_name) != 0)
        {
            continue;
        }

        *member = typedata_member;
        *index = i;
        return true;
    }

    return false;
}

void ptcl_parser_clear_scope(ptcl_parser *parser)
{
    for (int i = parser->syntaxes_count - 1; i >= 0; i--)
    {
        ptcl_parser_syntax *instance = &parser->syntaxes[i];
        if (instance->root != ptcl_parser_root(parser))
        {
            continue;
        }

        instance->is_out_of_scope = true;
    }

    for (int i = parser->comp_types_count - 1; i >= 0; i--)
    {
        ptcl_parser_comp_type *instance = &parser->comp_types[i];
        if (instance->root != ptcl_parser_root(parser))
        {
            continue;
        }

        instance->is_out_of_scope = true;
    }

    for (int i = parser->typedatas_count - 1; i >= 0; i--)
    {
        ptcl_parser_typedata *instance = &parser->typedatas[i];
        if (instance->root != ptcl_parser_root(parser))
        {
            continue;
        }

        instance->is_out_of_scope = true;
    }

    for (int i = parser->variables_count - 1; i >= 0; i--)
    {
        ptcl_parser_variable *instance = &parser->variables[i];
        if (instance->root != ptcl_parser_root(parser))
        {
            continue;
        }

        instance->is_out_of_scope = true;
    }
}

ptcl_expression *ptcl_parser_get_ctor_from_dot(ptcl_parser *parser, ptcl_expression *dot, bool is_root)
{
    if (dot->type != ptcl_expression_dot_type || !dot->dot.is_name)
    {
        return NULL;
    }

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_expression *left = dot->dot.left;
    if (left->type != ptcl_expression_ctor_type)
    {
        return ptcl_parser_get_ctor_from_dot(parser, left, false);
    }

    ptcl_name name = dot->dot.name;
    ptcl_expression_ctor ctor = left->ctor;
    ptcl_argument *member;
    size_t index;
    if (!ptcl_parser_try_get_typedata_member(parser, left->return_type.typedata, name.value, &member, &index))
    {
        ptcl_parser_throw_unknown_member(parser, name.value, location);
        ptcl_expression_destroy(left);
        return NULL;
    }

    if (is_root)
    {
        return left;
    }

    ptcl_expression *static_value = ptcl_expression_copy(ctor.values[index], location);
    if (static_value == NULL)
    {
        ptcl_expression_destroy(left);
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    static_value->return_type.is_static = true;
    ptcl_expression_destroy(left);
    return static_value;
}

void ptcl_parser_throw_out_of_memory(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_out_of_memory_type, true, "Out of memory", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_not_allowed_token(ptcl_parser *parser, char *token, ptcl_location location)
{
    char *message = ptcl_string("Token '", token, "' not allowed", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_not_allowed_token_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_except_token(ptcl_parser *parser, char *value, ptcl_location location)
{
    char *message = ptcl_string("Except token '", value, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_except_token_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_except_type_specifier(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_except_type_specifier_type, true, "Expect type specifier", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_incorrect_type(ptcl_parser *parser, char *excepted, char *received, ptcl_location location)
{
    char *message = ptcl_string("Except '", excepted, "' type, but received '", received, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_incorrect_type_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_fast_incorrect_type(ptcl_parser *parser, ptcl_type excepted, ptcl_type received, ptcl_location location)
{
    char *excepted_message = ptcl_type_to_present_string_copy(excepted);
    char *received_message = ptcl_type_to_present_string_copy(received);
    ptcl_parser_throw_incorrect_type(parser, excepted_message, received_message, location);

    free(excepted_message);
    free(received_message);
}

void ptcl_parser_throw_variable_redefinition(ptcl_parser *parser, char *name, ptcl_location location)
{
    char *message = ptcl_string("Redefination variable with '", name, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_variable_redefinition_type, false, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_none_type(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_none_type_type, true, "Type must be specified to use 'none'", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_must_be_variable(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_must_be_variable_type, false, "Must be variable", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_must_be_static(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_must_be_static_type, false, "Must be static", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_member(ptcl_parser *parser, char *name, ptcl_location location)
{
    char *message = ptcl_string("Unknown member with '", name, "' name", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_member_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_statement(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_statement_type, true, "Unknown statement", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_expression(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_expression_type, true, "Unknown expression", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_variable(ptcl_parser *parser, char *name, ptcl_location location)
{
    char *message = ptcl_string("Unknown variable with '", name, "' name", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_variable_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_syntax(ptcl_parser *parser, ptcl_parser_syntax syntax, ptcl_location location)
{
    char *message = ptcl_string("Unknown syntax: '", NULL);
    for (size_t i = 0; i < syntax.count; i++)
    {
        ptcl_parser_syntax_node node = syntax.nodes[i];
        if (node.type == ptcl_parser_syntax_node_word_type)
        {
            message = ptcl_string_append(message, node.word.name.value, NULL);
        }
        else if (node.type == ptcl_parser_syntax_node_value_type)
        {
            char *type = ptcl_type_to_present_string_copy(node.value->return_type);
            message = ptcl_string_append(message, "[", type, "]", NULL);
            free(type);
        }

        if (i != syntax.count - 1)
        {
            message = ptcl_string_append(message, " ", NULL);
        }
    }

    message = ptcl_string_append(message, "'", NULL);

    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_syntax_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_wrong_arguments(ptcl_parser *parser, char *name, ptcl_expression **values, size_t count, ptcl_argument *arguments, size_t arguments_count, ptcl_location location)
{
    char *message = ptcl_string("Wrong arguments '", name, "(", NULL);
    if (message == NULL)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        char *type = ptcl_type_to_present_string_copy(values[i]->return_type);
        if (type == NULL)
        {
            free(message);
            return;
        }

        char *new_message = ptcl_string_append(message, type, NULL);
        free(type);

        if (new_message == NULL)
        {
            free(message);
            return;
        }

        message = new_message;
        if (i != count - 1)
        {
            new_message = ptcl_string_append(message, ", ", NULL);
            if (!new_message)
            {
                free(message);
                return;
            }

            message = new_message;
        }
    }

    char *new_message = ptcl_string_append(message, ")', expected (", NULL);
    if (new_message == NULL)
    {
        free(message);
        return;
    }

    message = new_message;
    for (size_t i = 0; i < arguments_count; i++)
    {
        ptcl_argument argument = arguments[i];
        char *type;
        if (argument.is_variadic)
        {
            type = ptcl_string_duplicate("...");
            if (type == NULL)
            {
                free(message);
                return;
            }
        }
        else
        {
            type = ptcl_type_to_present_string_copy(argument.type);
            if (type == NULL)
            {
                free(message);
                return;
            }
        }

        char *new_message = ptcl_string_append(message, type, NULL);
        free(type);

        if (new_message == NULL)
        {
            free(message);
            return;
        }

        message = new_message;
        if (i != arguments_count - 1)
        {
            new_message = ptcl_string_append(message, ", ", NULL);
            if (!new_message)
            {
                free(message);
                return;
            }

            message = new_message;
        }
    }

    new_message = ptcl_string_append(message, ")", NULL);
    if (new_message == NULL)
    {
        free(message);
        return;
    }

    message = new_message;
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_wrong_arguments_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_max_depth(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_max_depth_type, true, "Max syntaxes depth", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_function(ptcl_parser *parser, char *name, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_function_type, true, ptcl_string("Unknown function '", name, "'", NULL), true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_variable_or_type(ptcl_parser *parser, char *name, ptcl_location location)
{
    char *message = ptcl_string("Unknown variable with '", name, "' name or expected type", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_variable_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_type(ptcl_parser *parser, char *value, ptcl_location location)
{
    char *message = ptcl_string("Unknown type '", value, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_type_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_redefination(ptcl_parser *parser, char *name, ptcl_location location)
{
    char *message = ptcl_string("Redefination '", name, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_redefinition_type, false, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_user(ptcl_parser *parser, char *message, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_max_depth_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_add_error(ptcl_parser *parser, ptcl_parser_error error)
{
    if (!parser->add_errors)
    {
        ptcl_parser_error_destroy(error);
        if (!ptcl_parser_critical(parser))
        {
            parser->is_critical = error.is_critical;
        }

        return;
    }

    ptcl_parser_error *buffer = realloc(parser->errors, (parser->errors_count + 1) * sizeof(ptcl_parser_error));
    if (buffer == NULL)
    {
        parser->is_critical = true;
        ptcl_parser_error_destroy(error);
    }
    else
    {
        parser->errors = buffer;
        parser->errors[parser->errors_count++] = error;
        parser->is_critical = error.is_critical;
    }
}

void ptcl_parser_result_destroy(ptcl_parser_result result)
{
    for (size_t i = 0; i < result.errors_count; i++)
    {
        ptcl_parser_error_destroy(result.errors[i]);
    }

    free(result.errors);
    for (size_t i = 0; i < result.syntaxes_count; i++)
    {
        ptcl_parser_syntax_destroy(result.syntaxes[i]);
    }

    free(result.syntaxes);
    for (size_t i = 0; i < result.comp_types_count; i++)
    {
        ptcl_parser_comp_type_destroy(result.comp_types[i]);
    }

    free(result.comp_types);
    free(result.typedatas);
    free(result.functions);
    for (size_t i = 0; i < result.variables_count; i++)
    {
        ptcl_parser_variable_destroy(result.variables[i]);
    }

    free(result.variables);
    for (size_t i = 0; i < result.lated_states_count; i++)
    {
        free(result.lated_states[i].tokens);
    }

    free(result.lated_states);
    free(result.this_pairs);
    if (!result.is_critical)
    {
        ptcl_statement_func_body_destroy(result.body);
    }
}

void ptcl_parser_destroy(ptcl_parser *parser)
{
    ptcl_interpreter_destroy(parser->interpreter);
    free(parser);
}
