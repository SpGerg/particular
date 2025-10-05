#include <ptcl_parser.h>
#include <ptcl_parser_builder.h>

#define PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count) \
    for (size_t i = 0; i < count; i++)                  \
    {                                                   \
        ptcl_expression_destroy(arguments[i]);          \
    }                                                   \
    free(arguments);

typedef struct ptcl_parser
{
    ptcl_lexer_configuration *configuration;
    ptcl_tokens_list *input;
    ptcl_token *tokens;
    size_t count;
    size_t position;
    ptcl_parser_error *errors;
    size_t errors_count;
    ptcl_parser_instance *instances;
    size_t instances_count;
    ptcl_statement_func_body *root;
    ptcl_statement_func_body *main_root;
    ptcl_statement_func_body *inserted_body;
    ptcl_type *return_type;
    ptcl_parser_syntax syntaxes[PTCL_PARSER_MAX_DEPTH];
    size_t syntax_depth;
    bool is_critical;
    bool add_errors;
    bool is_syntax_body;
    bool is_type_body;
    ptcl_lated_body *lated_bodies;
    size_t lated_bodies_count;
    size_t lated_bodies_capacity;
    ptcl_parser_this_s_pair *this_pairs;
    size_t this_pairs_count;
} ptcl_parser;

typedef struct
{
    ptcl_argument *arguments;
    size_t count;
    bool has_variadic;
} ptcl_parse_arguments_result;

static void ptcl_parse_arguments_result_destroy(ptcl_parse_arguments_result result)
{
    for (size_t i = 0; i < result.count; i++)
    {
        ptcl_argument_destroy(result.arguments[i]);
    }

    free(result.arguments);
}

static ptcl_parse_arguments_result ptcl_parser_parse_arguments(ptcl_parser *parser, ptcl_location location, bool with_names)
{
    ptcl_parse_arguments_result result = {
        .arguments = NULL,
        .count = 0,
        .has_variadic = false};

    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_parse_arguments_result_destroy(result);
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            return result;
        }

        ptcl_argument *buffer = realloc(result.arguments, (result.count + 1) * sizeof(ptcl_argument));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_parse_arguments_result_destroy(result);
            break;
        }

        result.arguments = buffer;
        if (ptcl_parser_match(parser, ptcl_token_elipsis_type))
        {
            if (result.count == 0)
            {
                ptcl_parser_throw_except_type_specifier(parser, location);
                break;
            }

            result.arguments[result.count++] = ptcl_argument_create_variadic();
            result.has_variadic = true;

            ptcl_parser_except(parser, ptcl_token_right_par_type);
            if (parser->is_critical)
            {
                ptcl_parse_arguments_result_destroy(result);
                break;
            }

            break;
        }
        else
        {
            ptcl_name argument_name = ptcl_name_create_fast_w(NULL, false);
            if (with_names)
            {
                argument_name = ptcl_parser_name_word(parser);
                if (parser->is_critical)
                {
                    ptcl_parse_arguments_result_destroy(result);
                    break;
                }

                ptcl_parser_except(parser, ptcl_token_colon_type);
                if (parser->is_critical)
                {
                    ptcl_parse_arguments_result_destroy(result);
                    break;
                }
            }

            ptcl_type type = ptcl_parser_type(parser, false, false);
            if (parser->is_critical)
            {
                ptcl_parse_arguments_result_destroy(result);
                break;
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
        if (parser->is_critical)
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

static char *ptcl_from_array(ptcl_expression_array array)
{
    char *buffer = malloc(array.count * sizeof(char));
    if (buffer == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < array.count; i++)
    {
        buffer[i] = array.expressions[i]->character;
    }

    return buffer;
}

static ptcl_expression *ptcl_defined_realization(ptcl_parser *parser, ptcl_expression **arguments, size_t count, ptcl_location location, bool is_expression)
{
    ptcl_expression *name_argument = arguments[0];
    ptcl_name name = ptcl_name_create_fast_w(ptcl_from_array(name_argument->array), false);
    if (name.value == NULL)
    {
        PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    ptcl_parser_instance *instance;
    bool is_defined = ptcl_parser_try_get_instance_any(parser, name, &instance);
    free(name.value);
    PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
    return ptcl_expression_create_integer(is_defined, location);
}

static bool ptcl_insert_body(ptcl_parser *parser, ptcl_token *tokens, size_t count, ptcl_location location)
{
    size_t old_count = parser->count;
    size_t new_count = old_count + count;
    ptcl_token *buffer = realloc(parser->tokens, new_count * sizeof(ptcl_token));
    if (buffer == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        return false;
    }

    size_t current = parser->position;
    size_t end = current + count;
    parser->tokens = buffer;
    parser->count = new_count;
    if (current < old_count)
    {
        memmove(parser->tokens + current + count,
                parser->tokens + current,
                sizeof(ptcl_token) * (old_count - current));
    }

    for (size_t i = 0; i < count; i++)
    {
        ptcl_token token = tokens[i];
        token.is_free_value = false;
        parser->tokens[current + i] = token;
    }

    return true;
}

static ptcl_expression *ptcl_get_statements_realization(ptcl_parser *parser, ptcl_expression **arguments, size_t count, ptcl_location location, bool is_expression)
{
    ptcl_expression *argument = arguments[0];
    ptcl_type type = argument->return_type;
    ptcl_type array_type = ptcl_type_create_array(&ptcl_statement_t_type, 0);

    switch (type.type)
    {
    case ptcl_value_function_pointer_type:
        ptcl_lated_body lated_body = parser->lated_bodies[argument->lated_body.index];
        if (!ptcl_insert_body(parser, lated_body.tokens, lated_body.count, location))
        {
            PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
            return NULL;
        }

        const ptcl_statement_func_body *last_state = parser->inserted_body;
        ptcl_statement_func_body body = ptcl_statement_func_body_create(
            NULL, 0,
            parser->root);
        parser->inserted_body = &body;
        ptcl_parser_func_body_by_pointer(parser, parser->inserted_body, true, false);
        parser->inserted_body = last_state;
        if (parser->is_critical)
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
                free(statements);
                ptcl_statement_func_body_destroy(body);
                for (size_t j = 0; j < i; j++)
                {
                    free(statements[j]);
                }

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
    if (is_expression)
    {
        PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
        ptcl_parser_throw_unknown_expression(parser, location);
        return NULL;
    }

    ptcl_expression *argument = arguments[0];
    ptcl_type type = argument->return_type;
    ptcl_type array_type = ptcl_type_create_array(&ptcl_statement_t_type, 0);
    switch (type.type)
    {
    case ptcl_value_array_type:
        ptcl_type target = *type.array.target;
        if (target.type != ptcl_value_type_type ||
            !ptcl_name_compare(target.comp_type->identifier, ptcl_statement_t_name))
        {
            PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
            ptcl_parser_throw_fast_incorrect_type(parser, array_type, type, location);
            return NULL;
        }

        ptcl_expression_array array = argument->array;
        if (!ptcl_check_array_statements(parser, array))
        {
            PTCL_PARSER_DESTROY_ARGUMENTS(arguments, count);
            return NULL;
        }

        ptcl_statement_func_body *body = parser->syntax_depth > 0 ? parser->main_root : parser->root;
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
            ptcl_statement *element = ptcl_create_statement_from_expression(parser->root, expression, location);
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

        break;
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
    parser->tokens = input->tokens;
    parser->count = input->count;
    return parser;
}

ptcl_parser_result ptcl_parser_parse(ptcl_parser *parser)
{
    parser->position = 0;
    parser->is_critical = false;
    parser->root = NULL;
    parser->main_root = NULL;
    parser->inserted_body = NULL;
    parser->return_type = NULL;
    parser->errors = NULL;
    parser->errors_count = 0;
    parser->instances = NULL;
    parser->instances_count = 0;
    parser->syntax_depth = 0;
    parser->add_errors = true;
    parser->is_syntax_body = true;
    parser->is_type_body = false;
    parser->lated_bodies = NULL;
    parser->lated_bodies_capacity = 0;
    parser->lated_bodies_count = 0;
    parser->this_pairs = NULL;
    parser->this_pairs_count = 0;

    ptcl_typedata_builder token_typedata_builder = ptcl_typedata_builder_create("ptcl_token_t");
    ptcl_typedata_builder_add_member(&token_typedata_builder, "token_type", ptcl_type_integer);
    ptcl_typedata_builder_add_member(&token_typedata_builder, "value", ptcl_type_create_array(&ptcl_type_character, -1));
    ptcl_parser_add_instance(parser, ptcl_typedata_builder_build(token_typedata_builder));

    ptcl_comp_type_builder statement_builder = ptcl_comp_type_builder_create(PTCL_PARSER_STATEMENT_TYPE_NAME);
    ptcl_type_comp_type statement_comp_type = ptcl_comp_type_builder_build_type(&statement_builder);
    ptcl_type_comp_type *comp_type = malloc(sizeof(ptcl_type_comp_type));
    if (comp_type == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
        return (ptcl_parser_result){
            .body = ptcl_statement_func_body_create(NULL, 0, NULL),
            .count = 1,
            .errors = parser->errors,
            .instances = parser->instances,
            .instances_count = parser->instances_count,
            .lated_bodies = NULL,
            .lated_bodies_count = 0,
            .is_critical = parser->is_critical};
    }

    *comp_type = statement_comp_type;
    ptcl_type statement_type = ptcl_type_create_comp_type_t(comp_type);
    ptcl_parser_add_instance(parser, ptcl_comp_type_builder_build(&statement_builder, statement_type.comp_type));

    ptcl_func_built_in_builder get_statements_builder = ptcl_func_built_in_builder_create("ptcl_get_statements");
    ptcl_type array_statements = ptcl_type_create_array(&statement_type, -1);
    array_statements.array.target->is_static = false;
    ptcl_func_built_in_builder_bind(&get_statements_builder, ptcl_get_statements_realization);
    ptcl_func_built_in_builder_add_argument(&get_statements_builder, ptcl_type_any);
    ptcl_func_built_in_builder_return(&get_statements_builder, array_statements);
    ptcl_parser_add_instance(parser, ptcl_func_built_in_builder_build(&get_statements_builder));

    ptcl_func_built_in_builder defined_builder = ptcl_func_built_in_builder_create("ptcl_defined");
    ptcl_func_built_in_builder_bind(&defined_builder, ptcl_defined_realization);
    ptcl_func_built_in_builder_add_argument(&defined_builder, ptcl_type_create_array(&ptcl_type_character, -1));
    ptcl_func_built_in_builder_return(&defined_builder, ptcl_type_integer);
    ptcl_parser_add_instance(parser, ptcl_func_built_in_builder_build(&defined_builder));

    ptcl_func_built_in_builder insert_builder = ptcl_func_built_in_builder_create("ptcl_insert");
    ptcl_type static_any = ptcl_type_any;
    static_any.is_static = true;
    ptcl_func_built_in_builder_bind(&insert_builder, ptcl_insert_realization);
    ptcl_func_built_in_builder_add_argument(&insert_builder, static_any);
    ptcl_func_built_in_builder_return(&insert_builder, ptcl_type_integer);
    ptcl_parser_add_instance(parser, ptcl_func_built_in_builder_build(&insert_builder));

    ptcl_parser_result result = {
        .configuration = parser->configuration,
        .body = ptcl_parser_func_body(parser, false, true),
        .errors = parser->errors,
        .count = parser->errors_count,
        .instances = parser->instances,
        .instances_count = parser->instances_count,
        .lated_bodies = parser->lated_bodies,
        .lated_bodies_count = parser->lated_bodies_count,
        .this_pairs = parser->this_pairs,
        .this_pairs_count = parser->this_pairs_count,
        .is_critical = parser->is_critical};

    ptcl_func_built_in_builder_destroy(&get_statements_builder);
    ptcl_func_built_in_builder_destroy(&defined_builder);
    ptcl_func_built_in_builder_destroy(&insert_builder);
    ptcl_typedata_builder_destroy(&token_typedata_builder);
    ptcl_comp_type_builder_destroy(&statement_builder);
    parser->input->tokens = parser->tokens;
    parser->input->count = parser->count;
    return result;
}

ptcl_statement_type ptcl_parser_parse_get_statement(ptcl_parser *parser, bool *is_finded)
{
    ptcl_token current = ptcl_parser_current(parser);
    if (current.type == ptcl_token_tilde_type || current.type == ptcl_token_prototype_type || current.type == ptcl_token_global_type)
    {
        ptcl_parser_skip(parser);
        ptcl_statement_type type = ptcl_parser_parse_get_statement(parser, is_finded);
        parser->position--;
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
            parser->position -= 2;
            return type;
        }

        return ptcl_statement_assign_type;
    case ptcl_token_caret_type:
        return ptcl_statement_func_call_type;
    case ptcl_token_optional_type:
        return ptcl_statement_type_decl_type;
    case ptcl_token_return_type:
        return ptcl_statement_return_type;
    case ptcl_token_if_type:
    case ptcl_token_static_type:
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
        *is_finded = false;
    }
}

ptcl_statement *ptcl_parser_parse_statement(ptcl_parser *parser)
{
    size_t start = parser->position;
    if (parser->is_syntax_body && ptcl_parser_parse_try_syntax_usage_here(parser, true))
    {
        parser->position = start;
        ptcl_parser_extra_body(parser, true);
        ptcl_parser_leave_from_syntax(parser);
        return NULL;
    }

    if (parser->is_critical)
    {
        return NULL;
    }

    ptcl_attributes attributes = ptcl_parser_parse_attributes(parser);
    if (parser->is_critical)
    {
        return NULL;
    }

    ptcl_statement *statement = NULL;
    ptcl_location location = ptcl_parser_current(parser).location;
    bool is_finded;
    ptcl_statement_type type = ptcl_parser_parse_get_statement(parser, &is_finded);
    if (is_finded)
    {
        if (parser->is_type_body && type != ptcl_statement_func_decl_type)
        {
            ptcl_parser_except(parser, ptcl_token_function_type);
            return NULL;
        }

        statement = ptcl_statement_create(type, parser->root, attributes, location);
        if (statement == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            return NULL;
        }

        switch (type)
        {
        case ptcl_statement_func_call_type:
            statement->func_call = ptcl_parser_func_call(parser, NULL, false);
            if (statement->func_call.is_built_in)
            {
                if (parser->is_critical)
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

            break;
        case ptcl_statement_func_decl_type:
            statement->func_decl = ptcl_parser_func_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type), ptcl_parser_match(parser, ptcl_token_global_type));
            break;
        case ptcl_statement_typedata_decl_type:
            statement->typedata_decl = ptcl_parser_typedata_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type), ptcl_parser_match(parser, ptcl_token_global_type));
            break;
        case ptcl_statement_type_decl_type:
            statement->type_decl = ptcl_parser_type_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type), ptcl_parser_match(parser, ptcl_token_global_type));
            break;
        case ptcl_statement_assign_type:
            statement->assign = ptcl_parser_assign(parser, ptcl_parser_match(parser, ptcl_token_global_type));
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
        }
    }
    else
    {
        ptcl_parser_throw_unknown_statement(parser, location);
    }

    if (parser->is_critical)
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

    statement->root = parser->root;
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
        if (parser->is_critical)
        {
            return (ptcl_attributes){};
        }

        ptcl_parser_except(parser, ptcl_token_right_square_type);
        if (parser->is_critical)
        {
            ptcl_name_destroy(name);
            return (ptcl_attributes){};
        }

        ptcl_attribute *buffer = realloc(attributes.attributes, (attributes.count + 1) * sizeof(ptcl_attribute));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_attributes_destroy(attributes);
            return (ptcl_attributes){};
        }

        attributes.attributes = buffer;
        attributes.attributes[attributes.count++] = ptcl_attribute_create(name);
    }

    return attributes;
}

static bool ptcl_parser_insert_syntax(
    ptcl_parser *parser,
    size_t start_tokens,
    size_t tokens_count,
    size_t start_position,
    size_t end_position,
    bool is_statement)
{
    ptcl_token *original_tokens = malloc(tokens_count * sizeof(ptcl_token));
    if (original_tokens == NULL)
    {
        return false;
    }

    for (size_t i = 0; i < tokens_count; i++)
    {
        original_tokens[i] = ptcl_token_same(parser->tokens[start_tokens + i]);
    }

    size_t tokens_to_insert_count = tokens_count;
    if (is_statement)
    {
        tokens_to_insert_count += 3;
    }

    ptcl_location location = ptcl_parser_current(parser).location;
    const size_t number_of_tokens_to_replace = end_position - start_position;
    const int size_difference = (int)number_of_tokens_to_replace - (int)tokens_to_insert_count;
    for (size_t token_index = start_position; token_index < end_position; token_index++)
    {
        ptcl_token_destroy(parser->tokens[token_index]);
    }

    ptcl_token at, left_curly, right_curly;
    if (is_statement)
    {
        at = ptcl_token_create(
            ptcl_token_at_type,
            ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_at_type), location, false);
        left_curly = ptcl_token_create(
            ptcl_token_left_curly_type,
            ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_left_curly_type), location, false);
        right_curly = ptcl_token_create(
            ptcl_token_right_curly_type,
            ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_curly_type), location, false);
    }

    if (size_difference < 0)
    {
        const size_t additional_space_needed = -size_difference;
        ptcl_token *new_token_buffer = realloc(parser->tokens,
                                               (parser->count + additional_space_needed) * sizeof(ptcl_token));
        if (new_token_buffer == NULL)
        {
            free(original_tokens);
            return false;
        }

        parser->tokens = new_token_buffer;
        for (size_t token_index = parser->count; token_index > start_position + number_of_tokens_to_replace; token_index--)
        {
            size_t source = token_index - 1;
            size_t destination = source + additional_space_needed;
            if (destination < parser->count + additional_space_needed)
            {
                parser->tokens[destination] = parser->tokens[source];
            }
        }

        parser->count += additional_space_needed;
        size_t insert_pos = start_position;
        if (is_statement)
        {
            parser->tokens[insert_pos++] = at;
            parser->tokens[insert_pos++] = left_curly;
        }

        for (size_t i = 0; i < tokens_count; i++)
        {
            parser->tokens[insert_pos++] = original_tokens[i];
        }

        if (is_statement)
        {
            parser->tokens[insert_pos] = right_curly;
        }
    }
    else if (size_difference > 0)
    {
        const size_t new_token_count = parser->count - size_difference;
        for (size_t token_index = end_position; token_index < parser->count; token_index++)
        {
            parser->tokens[token_index - size_difference] = parser->tokens[token_index];
        }

        ptcl_token *new_token_buffer = realloc(parser->tokens, new_token_count * sizeof(ptcl_token));
        if (new_token_buffer != NULL)
        {
            parser->tokens = new_token_buffer;
        }

        parser->count = new_token_count;
        size_t insert_pos = start_position;
        if (is_statement)
        {
            parser->tokens[insert_pos++] = at;
            parser->tokens[insert_pos++] = left_curly;
        }

        for (size_t i = 0; i < tokens_count; i++)
        {
            parser->tokens[insert_pos++] = original_tokens[i];
        }

        if (is_statement)
        {
            parser->tokens[insert_pos] = right_curly;
        }
    }
    else
    {
        size_t insert_pos = start_position;
        if (is_statement)
        {
            parser->tokens[insert_pos++] = at;
            parser->tokens[insert_pos++] = left_curly;
        }

        for (size_t i = 0; i < tokens_count; i++)
        {
            parser->tokens[insert_pos++] = original_tokens[i];
        }

        if (is_statement)
        {
            parser->tokens[insert_pos] = right_curly;
        }
    }

    free(original_tokens);
    return true;
}

bool ptcl_parser_parse_try_syntax_usage_here(ptcl_parser *parser, bool is_statement)
{
    return ptcl_parser_parse_try_syntax_usage(parser, NULL, 0, -1, false, is_statement);
}

bool ptcl_parser_parse_try_syntax_usage(
    ptcl_parser *parser, ptcl_parser_syntax_node **nodes, size_t count, int down_start, bool skip_first, bool is_statement)
{
    if (down_start < 0 && !ptcl_parser_match(parser, ptcl_token_hashtag_type))
    {
        return false;
    }

    const bool is_root = down_start < 0;
    const size_t start = is_root ? parser->position : (size_t)down_start;
    const size_t original_count = count;
    size_t stop = parser->position;

    ptcl_parser_instance instance = ptcl_parser_syntax_create(
        ptcl_name_create_fast_w("", false),
        parser->root,
        nodes ? *nodes : NULL,
        count);
    ptcl_parser_syntax syntax = instance.syntax;
    ptcl_parser_syntax result;
    bool found = false;

    while (!ptcl_parser_ended(parser))
    {
        if (!skip_first)
        {
            ptcl_token current = ptcl_parser_current(parser);
            ptcl_parser_syntax_node *buffer = realloc(syntax.nodes, (syntax.count + 1) * sizeof(ptcl_parser_syntax_node));
            if (buffer == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, current.location);
                ptcl_parser_syntax_destroy(syntax);
                parser->position = start;
                break;
            }

            syntax.nodes = buffer;
            const size_t position = parser->position;
            ptcl_parser_syntax_node node = ptcl_parser_syntax_node_create_word(
                current.type,
                ptcl_name_create(current.value, false, current.location));
            ptcl_parser_skip(parser);
            syntax.nodes[syntax.count++] = node;

            if (ptcl_parser_parse_try_syntax_usage(parser, &syntax.nodes, syntax.count, start, true, is_statement))
            {
                return true;
            }

            if (parser->is_critical)
            {
                // Already destroyed in func below
                return false;
            }

            parser->position = position;
            const bool last_mode = parser->add_errors;
            parser->add_errors = false;
            ptcl_expression *value = ptcl_parser_cast(parser, NULL, true);
            parser->add_errors = last_mode;
            if (parser->is_critical)
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
        bool current_found = ptcl_parser_syntax_try_find(parser, syntax.nodes, syntax.count, &target, &can_continue);

        if (current_found)
        {
            stop = parser->position;
            result = *target;
            found = true;
        }

        if (!can_continue)
        {
            break;
        }
    }

    if (found)
    {
        // Handle syntax depth and memory limits
        if (parser->syntax_depth == 0)
        {
            parser->main_root = parser->root;
        }

        if (parser->syntax_depth + 1 >= PTCL_PARSER_MAX_DEPTH)
        {
            ptcl_parser_throw_max_depth(parser, ptcl_parser_current(parser).location);
            ptcl_parser_syntax_destroy(syntax);
            return false;
        }

        // Create new function body context
        ptcl_statement_func_body *temp = malloc(sizeof(ptcl_statement_func_body));
        if (!temp)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_parser_syntax_destroy(syntax);
            return false;
        }

        *temp = ptcl_statement_func_body_create(NULL, 0, parser->root);
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
            ptcl_parser_instance variable = ptcl_parser_variable_create(
                ptcl_name_create_fast_w(syntax_node.variable.name, false),
                syntax_node.variable.type,
                expression,
                true,
                parser->root);
            variable.variable.is_syntax_variable = true;
            variable.variable.is_built_in = true;
            variable.variable.built_in = expression;
            if (expression->return_type.is_static && expression->return_type.type == ptcl_value_word_type)
            {
                variable.variable.built_in->word.value = ptcl_string_duplicate(variable.variable.built_in->word.value);
                variable.variable.is_syntax_word = true;
            }

            if (!ptcl_parser_add_instance(parser, variable))
            {
                ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
                ptcl_parser_syntax_destroy(syntax);
                return false;
            }
        }

        parser->position = stop;
        ptcl_parser_insert_syntax(parser, result.start, result.tokens_count, start - 1, stop, is_statement);
        parser->syntaxes[parser->syntax_depth++] = syntax;
    }
    else
    {
        parser->position = start;
        if (is_root)
        {
            ptcl_parser_throw_unknown_syntax(parser, syntax, parser->tokens[start].location);
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

    ptcl_statement_func_body *temp = parser->root;
    ptcl_statement_func_body_destroy(*temp);
    parser->root = temp->root;
    free(temp);
    free(parser->syntaxes[--parser->syntax_depth].nodes);
}

ptcl_statement_func_call ptcl_parser_func_call(ptcl_parser *parser, ptcl_parser_function *function, bool is_expression)
{
    bool with_injection = ptcl_parser_match(parser, ptcl_token_caret_type);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_identifier identifier;
    ptcl_name name;
    if (function != NULL)
    {
        if (ptcl_parser_current(parser).type == ptcl_token_left_par_type)
        {
            name = function->func.name;
            name.is_free = false;
        }
        else
        {
            name = ptcl_parser_name_word(parser);
        }

        identifier = ptcl_identifier_create_by_name(name);
    }
    else
    {
        size_t start = parser->position;
        name = ptcl_parser_name_word(parser);
        if (parser->is_critical)
        {
            return (ptcl_statement_func_call){};
        }

        // TODO: avoid reparsing
        if (ptcl_parser_current(parser).type != ptcl_token_left_par_type)
        {
            parser->position = start;
            ptcl_name_destroy(name);
            identifier = ptcl_identifier_create_by_expr(ptcl_parser_dot(parser, NULL, NULL, false));
            if (parser->is_critical)
            {
                return (ptcl_statement_func_call){};
            }

            return ptcl_statement_func_call_create(identifier, NULL, 0);
        }
        else
        {
            identifier = ptcl_identifier_create_by_name(name);
        }
    }

    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (parser->is_critical)
    {
        return (ptcl_statement_func_call){};
    }

    ptcl_parser_instance *instance_target;
    ptcl_parser_function target;
    if (function == NULL)
    {
        if (!ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_function_type, &instance_target))
        {
            ptcl_parser_throw_unknown_function(parser, name.value, location);
            return (ptcl_statement_func_call){};
        }

        target = instance_target->function;
    }
    else
    {
        target = *function;
    }

    ptcl_statement_func_call func_call = ptcl_statement_func_call_create(identifier, NULL, 0);
    bool is_variadic_now = false;
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_statement_func_call_destroy(func_call);
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            return (ptcl_statement_func_call){};
        }

        ptcl_expression **arguments = realloc(func_call.arguments, (func_call.count + 1) * sizeof(ptcl_expression *));
        if (arguments == NULL)
        {
            ptcl_statement_func_call_destroy(func_call);
            return (ptcl_statement_func_call){};
        }

        func_call.arguments = arguments;
        ptcl_argument *argument = func_call.count < target.func.count ? &target.func.arguments[func_call.count] : NULL;
        ptcl_type *excepted = NULL;
        if (argument != NULL)
        {
            excepted = &argument->type;
            is_variadic_now = argument->is_variadic;
        }

        func_call.arguments[func_call.count] = ptcl_parser_cast(parser, excepted, false);
        if (parser->is_critical)
        {
            ptcl_statement_func_call_destroy(func_call);
            if (func_call.count == 0)
            {
                free(func_call.arguments);
            }

            return (ptcl_statement_func_call){};
        }

        func_call.count++;
        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    bool argument_count_mismatch = false;
    if (target.func.is_variadic)
    {
        argument_count_mismatch = func_call.count < target.func.count - 1;
    }
    else
    {
        argument_count_mismatch = func_call.count != target.func.count;
    }

    if (argument_count_mismatch)
    {
        ptcl_parser_throw_wrong_arguments(parser, name.value, func_call.arguments,
                                          func_call.count, location);
        ptcl_statement_func_call_destroy(func_call);
        return (ptcl_statement_func_call){};
    }

    func_call.return_type = target.func.return_type;
    if (target.is_built_in)
    {
        ptcl_expression *result = target.bind(parser, func_call.arguments, func_call.count, location, is_expression);
        if (parser->is_critical)
        {
            return (ptcl_statement_func_call){};
        }

        func_call.is_built_in = true;
        func_call.built_in = result;
    }
    else
    {
        func_call.is_built_in = false;
    }

    if (with_injection && !func_call.is_built_in)
    {
        ptcl_parser_insert_pairs(parser, target.func.func_body);
    }

    return func_call;
}

ptcl_statement_func_body ptcl_parser_func_body(ptcl_parser *parser, bool with_brackets, bool change_root)
{
    ptcl_statement_func_body func_body = ptcl_statement_func_body_create(NULL, 0, parser->root);
    func_body.root = parser->root;

    ptcl_parser_func_body_by_pointer(parser, &func_body, with_brackets, change_root);
    return func_body;
}

void ptcl_parser_func_body_by_pointer(ptcl_parser *parser, ptcl_statement_func_body *func_body_pointer, bool with_brackets, bool change_root)
{
    ptcl_parser_match(parser, ptcl_token_left_curly_type);

    ptcl_statement_func_body *previous = parser->root;
    ptcl_statement_func_body *previous_main = parser->main_root;
    if (change_root)
    {
        parser->root = func_body_pointer;
        parser->main_root = func_body_pointer;
    }

    while (true)
    {
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

        if (with_brackets && ptcl_parser_match(parser, ptcl_token_right_curly_type))
        {
            break;
        }

        ptcl_statement *statement = ptcl_parser_parse_statement(parser);
        if (parser->is_critical)
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
        if (parser->is_critical)
        {
            ptcl_statement_func_body_destroy(*func_body_pointer);
            break;
        }

        func_body_pointer->count++;
    }

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
    ptcl_parser_func_body_by_pointer(parser, &body, true, false);
    if (parser->is_critical)
    {
        return;
    }

    ptcl_statement_func_body *previous = parser->root;
    parser->root = body.root;

    ptcl_statement *statement = ptcl_statement_func_body_create_stat(body, location);
    size_t new_count = parser->root->count + 1;
    ptcl_statement **new_statements = realloc(parser->root->statements,
                                              sizeof(ptcl_statement *) * new_count);
    if (new_statements == NULL)
    {
        ptcl_statement_destroy(statement);
        ptcl_parser_throw_out_of_memory(parser, location);
        parser->root = previous;
        return;
    }

    new_statements[parser->root->count] = statement;
    parser->root->statements = new_statements;
    parser->root->count = new_count;
    parser->root = previous;
    ptcl_parser_extra_body(parser, is_syntax);
}

ptcl_type ptcl_parser_type(ptcl_parser *parser, bool with_word, bool with_any)
{
    size_t start = parser->position;
    bool with_syntax = false;
    if (parser->is_syntax_body)
    {
        with_syntax = ptcl_parser_parse_try_syntax_usage_here(parser, false);
    }

    if (parser->is_critical)
    {
        return (ptcl_type){};
    }

    parser->position = start;
    size_t position = parser->position;
    size_t original_statements_count = parser->root->count;
    size_t original_instances_count = parser->instances_count;
    ptcl_parser_extra_body(parser, with_syntax);
    if (parser->is_critical)
    {
        if (with_syntax)
        {
            ptcl_parser_leave_from_syntax(parser);
        }

        return (ptcl_type){};
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
        ptcl_parse_arguments_result arguments = ptcl_parser_parse_arguments(parser, current.location, false);
        if (parser->is_critical)
        {
            return (ptcl_type){};
        }

        ptcl_parser_except(parser, ptcl_token_colon_type);
        if (parser->is_critical)
        {
            ptcl_parse_arguments_result_destroy(arguments);
            return (ptcl_type){};
        }

        ptcl_type *return_type = malloc(sizeof(ptcl_type));
        if (return_type == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, current.location);
            ptcl_parse_arguments_result_destroy(arguments);
            return (ptcl_type){};
        }

        *return_type = ptcl_parser_type(parser, false, false);
        if (parser->is_critical)
        {
            ptcl_parse_arguments_result_destroy(arguments);
            free(return_type);
            return (ptcl_type){};
        }

        ptcl_type function = ptcl_type_create_func(return_type, NULL, 0);
        function.function_pointer.arguments = arguments.arguments;
        function.function_pointer.count = arguments.count;
        return_type->is_primitive = false;
        target = function;
        target.is_static = is_static;
        break;
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
        ptcl_parser_instance *typedata;
        parser->position--;
        ptcl_name name = ptcl_parser_name_word_or_type(parser);
        if (parser->is_critical)
        {
            break;
        }

    case ptcl_token_word_type:
        // TODO: search type by name in one loop
        if (current.type == ptcl_token_word_type)
        {
            name = ptcl_name_create_fast_w(current.value, false);
        }

        if (ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_typedata_type, &typedata))
        {
            ptcl_name_destroy(name);
            target = ptcl_type_create_typedata(typedata->name.value, false);
            break;
        }
        else if (ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_comp_type_type, &typedata))
        {
            ptcl_name_destroy(name);
            target = ptcl_type_create_comp_type_t(typedata->comp_type.comp_type);
            break;
        }

        if (current.type == ptcl_token_exclamation_mark_type)
        {
            if (ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_variable_type, &typedata))
            {
                if (typedata->variable.type.type == ptcl_value_object_type_type)
                {
                    ptcl_name_destroy(name);
                    target = ptcl_type_copy(typedata->variable.built_in->object_type.type);
                    break;
                }
            }
        }

        ptcl_name_destroy(name);
        // Fallthrough
    default:
        parse_success = false;
        ptcl_parser_throw_unknown_type(parser, current.value, current.location);
        break;
    }

    if (with_syntax)
    {
        ptcl_parser_leave_from_syntax(parser);
    }

    if (!parse_success)
    {
        return (ptcl_type){};
    }

    if (is_static)
    {
        target.is_static = true;
    }

    if (target.type != ptcl_value_word_type)
    {
        ptcl_type parsed_target = ptcl_parser_pointers(parser, target);
        if (parser->is_critical)
        {
            return (ptcl_type){};
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

ptcl_statement_func_decl ptcl_parser_func_decl(ptcl_parser *parser, bool is_prototype, bool is_global)
{
    ptcl_parser_match(parser, ptcl_token_function_type);

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name name = ptcl_parser_name_word(parser);
    if (parser->is_critical)
    {
        return (ptcl_statement_func_decl){};
    }

    if (ptcl_parser_is_defined(parser, name))
    {
        ptcl_parser_throw_redefination(parser, name.value, location);
        ptcl_name_destroy(name);
        return (ptcl_statement_func_decl){};
    }

    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (parser->is_critical)
    {
        ptcl_name_destroy(name);
        return (ptcl_statement_func_decl){};
    }

    ptcl_statement_func_body func_body = ptcl_statement_func_body_create(NULL, 0, parser->root);

    // We set prototype, because on freeing we dont have body
    ptcl_statement_func_decl func_decl = ptcl_statement_func_decl_create(name, NULL, 0, NULL, ptcl_type_integer, true, false);
    func_decl.func_body = malloc(sizeof(ptcl_statement_func_body));
    if (func_decl.func_body == NULL)
    {
        ptcl_name_destroy(name);
        ptcl_parser_throw_out_of_memory(parser, location);
        return (ptcl_statement_func_decl){};
    }

    *func_decl.func_body = ptcl_statement_func_body_create(NULL, 0, parser->root);
    ptcl_parse_arguments_result arguments = ptcl_parser_parse_arguments(parser, location, true);
    if (parser->is_critical)
    {
        ptcl_name_destroy(name);
        free(func_decl.func_body);
        return (ptcl_statement_func_decl){};
    }

    ptcl_parser_except(parser, ptcl_token_colon_type);
    if (parser->is_critical)
    {
        ptcl_statement_func_decl_destroy(func_decl);
        return (ptcl_statement_func_decl){};
    }

    ptcl_type return_type = ptcl_parser_type(parser, false, false);
    if (parser->is_critical)
    {
        ptcl_statement_func_decl_destroy(func_decl);
        return (ptcl_statement_func_decl){};
    }
    else
    {
        func_decl.return_type = return_type;
    }

    func_decl.arguments = arguments.arguments;
    func_decl.count = arguments.count;
    func_decl.is_variadic = arguments.has_variadic;
    ptcl_parser_instance function = ptcl_parser_function_create(is_global ? NULL : parser->root, func_decl);
    ptcl_type *variable_return_type = NULL;
    size_t identifier = parser->instances_count;

    if (!parser->is_type_body)
    {
        ptcl_name variable_name = function.name;
        variable_name.is_free = false;
        ptcl_type *variable_return_type = malloc(sizeof(ptcl_type));
        if (variable_return_type == NULL)
        {
            ptcl_statement_func_decl_destroy(func_decl);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_func_decl){};
        }

        *variable_return_type = return_type;
        ptcl_type variable_type = ptcl_type_create_func(variable_return_type, func_decl.arguments, func_decl.count);
        variable_type.is_static = true;
        ptcl_parser_instance function_pointer = ptcl_parser_func_variable_create(variable_name, variable_type, function.root);
        if (!ptcl_parser_add_instance(parser, function) || !ptcl_parser_add_instance(parser, function_pointer))
        {
            ptcl_statement_func_decl_destroy(func_decl);
            free(variable_return_type);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_func_decl){};
        }
    }

    if (!is_prototype)
    {
        ptcl_type *previous_type = parser->return_type;
        parser->return_type = &return_type;
        for (size_t i = 0; i < func_decl.count; i++)
        {
            ptcl_argument argument = func_decl.arguments[i];
            ptcl_parser_instance variable = ptcl_parser_variable_create(argument.name, argument.type, NULL, false, func_decl.func_body);
            if (!ptcl_parser_add_instance(parser, variable))
            {
                ptcl_statement_func_decl_destroy(func_decl);
                free(variable_return_type);
                ptcl_parser_throw_out_of_memory(parser, location);
                return (ptcl_statement_func_decl){};
            }
        }

        ptcl_parser_func_body_by_pointer(parser, func_decl.func_body, true, true);
        if (parser->is_critical)
        {
            free(func_decl.func_body);
            func_decl.func_body = NULL;
            func_decl.is_prototype = true;
            ptcl_statement_func_decl_destroy(func_decl);
            parser->return_type = previous_type;
            if (!parser->is_type_body)
            {
                parser->instances[identifier - 1].is_out_of_scope = true;
                parser->instances[identifier].is_out_of_scope = true;
            }

            return (ptcl_statement_func_decl){};
        }

        func_decl.is_prototype = false;
        if (!parser->is_type_body)
        {
            ptcl_parser_function *original = &parser->instances[identifier].function;
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
    if (parser->is_critical)
    {
        return (ptcl_statement_typedata_decl){};
    }

    if (ptcl_parser_is_defined(parser, name))
    {
        ptcl_parser_throw_redefination(parser, name.value, location);
        ptcl_name_destroy(name);
        return (ptcl_statement_typedata_decl){};
    }

    ptcl_statement_typedata_decl decl = ptcl_statement_typedata_decl_create(name, NULL, 0, true);
    if (is_prototype)
    {
        return decl;
    }

    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (parser->is_critical)
    {
        return (ptcl_statement_typedata_decl){};
    }

    decl.is_prototype = false;
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        ptcl_typedata_member *buffer = realloc(decl.members, (decl.count + 1) * sizeof(ptcl_typedata_member));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        decl.members = buffer;
        ptcl_name member_name = ptcl_parser_name_word(parser);
        if (parser->is_critical)
        {
            if (decl.count == 0)
            {
                free(buffer);
            }

            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        ptcl_parser_except(parser, ptcl_token_colon_type);
        if (parser->is_critical)
        {
            if (decl.count == 0)
            {
                free(buffer);
            }

            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        ptcl_type type = ptcl_parser_type(parser, false, false);
        if (parser->is_critical)
        {
            if (decl.count == 0)
            {
                free(buffer);
            }

            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        decl.members[decl.count] = ptcl_typedata_member_create(member_name, type, decl.count);
        decl.count++; // UB
        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    ptcl_parser_instance instance = ptcl_parser_typedata_create(is_global ? NULL : parser->root, name, decl.members, decl.count);
    if (!ptcl_parser_add_instance(parser, instance))
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_typedata_decl_destroy(decl);
        return (ptcl_statement_typedata_decl){};
    }

    return decl;
}

ptcl_statement_type_decl ptcl_parser_type_decl(ptcl_parser *parser, bool is_prototype, bool is_global)
{
    bool is_optional = ptcl_parser_match(parser, ptcl_token_optional_type);
    ptcl_parser_match(parser, ptcl_token_type_type);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name name = ptcl_parser_name_word(parser);
    if (parser->is_critical)
    {
        return (ptcl_statement_type_decl){};
    }

    if (ptcl_parser_is_defined(parser, name))
    {
        ptcl_parser_throw_redefination(parser, name.value, location);
        ptcl_name_destroy(name);
        return (ptcl_statement_type_decl){};
    }

    ptcl_parser_except(parser, ptcl_token_colon_type);
    if (parser->is_critical)
    {
        ptcl_name_destroy(name);
        return (ptcl_statement_type_decl){};
    }

    ptcl_statement_type_decl decl = ptcl_statement_type_decl_create(name, NULL, 0, NULL, 0, is_optional, is_prototype);
    while (true)
    {
        ptcl_type_member *buffer = realloc(decl.types, (decl.types_count + 1) * sizeof(ptcl_type_member));
        if (buffer == NULL)
        {
            ptcl_statement_type_decl_destroy(decl);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_type_decl){};
        }

        decl.types = buffer;
        bool is_up = ptcl_parser_match(parser, ptcl_token_up_type);
        ptcl_type type = ptcl_parser_type(parser, false, false);
        if (parser->is_critical)
        {
            if (decl.types_count == 0)
            {
                free(decl.types);
            }

            ptcl_statement_type_decl_destroy(decl);
            return (ptcl_statement_type_decl){};
        }

        if (decl.types_count > 0)
        {
            ptcl_type_member expected = decl.types[decl.types_count - 1];
            if (!ptcl_type_is_castable(expected.type, type))
            {
                ptcl_parser_throw_fast_incorrect_type(parser, expected.type, type, location);
                ptcl_statement_type_decl_destroy(decl);
                return (ptcl_statement_type_decl){};
            }
        }

        decl.types[decl.types_count++] = ptcl_type_member_create(type, is_up);
        if (!ptcl_parser_match(parser, ptcl_token_comma_type))
        {
            break;
        }
    }

    ptcl_type_comp_type *comp_type = malloc(sizeof(ptcl_type_comp_type));
    if (comp_type == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_type_decl_destroy(decl);
        return (ptcl_statement_type_decl){};
    }

    *comp_type = ptcl_type_create_comp_type(decl);
    ptcl_parser_instance instance = ptcl_parser_comp_type_create(comp_type->identifier, is_global ? NULL : parser->root, comp_type);
    if (!ptcl_parser_add_instance(parser, instance))
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_type_decl_destroy(decl);
        return (ptcl_statement_type_decl){};
    }

    if (ptcl_parser_match(parser, ptcl_token_left_curly_type))
    {
        ptcl_statement_func_body *body = malloc(sizeof(ptcl_statement_func_body));
        if (body == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_statement_type_decl_destroy(decl);
            return (ptcl_statement_type_decl){};
        }

        *body = ptcl_statement_func_body_create(NULL, 0, parser->root);
        comp_type->functions = body;
        ptcl_parser_instance this_var = ptcl_parser_variable_create(
            ptcl_name_create_fast_w("this", false),
            ptcl_type_create_comp_type_t(comp_type), NULL, false, body);
        ptcl_parser_instance self_var = ptcl_parser_variable_create(
            ptcl_name_create_fast_w("self", false),
            decl.types[0].type, NULL, false, body);
        if (!ptcl_parser_add_instance(parser, this_var) || !ptcl_parser_add_instance(parser, self_var))
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_statement_type_decl_destroy(decl);
            ptcl_statement_func_body_destroy(*body);
            free(body);
            return (ptcl_statement_type_decl){};
        }

        ptcl_parser_func_body_by_pointer(parser, body, true, true);
        if (parser->is_critical)
        {
            free(body);
            ptcl_statement_type_decl_destroy(decl);
            return (ptcl_statement_type_decl){};
        }

        decl.body = body;
        decl.functions = comp_type->functions;
    }

    return decl;
}

ptcl_statement_assign ptcl_parser_assign(ptcl_parser *parser, bool is_global)
{
    ptcl_location location = ptcl_parser_current(parser).location;
    size_t position = parser->position;
    ptcl_name name = ptcl_parser_name_word(parser);
    ptcl_identifier identifier;
    ptcl_token current_identifier = ptcl_parser_current(parser);
    if (current_identifier.type != ptcl_token_equals_type && current_identifier.type != ptcl_token_colon_type)
    {
        // TODO: avoid reparsing
        parser->position = position;
        ptcl_name_destroy(name);
        identifier = ptcl_identifier_create_by_expr(ptcl_parser_dot(parser, NULL, false, false));
        if (parser->is_critical)
        {
            return (ptcl_statement_assign){};
        }
    }
    else
    {
        identifier = ptcl_identifier_create_by_name(name);
    }

    if (parser->is_critical)
    {
        ptcl_identifier_destroy(identifier);
        return (ptcl_statement_assign){};
    }

    ptcl_type type;
    bool with_type = ptcl_parser_match(parser, ptcl_token_colon_type);
    bool define = true;

    ptcl_parser_instance *created = NULL;
    if (with_type)
    {
        if (ptcl_parser_is_defined(parser, name))
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
            if (parser->is_critical)
            {
                ptcl_identifier_destroy(identifier);
                return (ptcl_statement_assign){};
            }
        }
    }
    else
    {
        if (identifier.is_name && !ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_variable_type, &created))
        {
            ptcl_parser_throw_unknown_variable_or_type(parser, name.value, location);
            return (ptcl_statement_assign){};
        }

        with_type = true;
        define = false;
        if (identifier.is_name)
        {
            type = created->variable.type;
        }
        else
        {
            type = identifier.value->return_type;
        }
    }

    ptcl_parser_except(parser, ptcl_token_equals_type);
    if (parser->is_critical)
    {
        if (with_type)
        {
            ptcl_type_destroy(type);
        }

        ptcl_identifier_destroy(identifier);
        return (ptcl_statement_assign){};
    }

    ptcl_expression *value = ptcl_parser_cast(parser, with_type ? &type : NULL, false);
    if (parser->is_critical)
    {
        if (with_type)
        {
            ptcl_type_destroy(type);
        }

        ptcl_identifier_destroy(identifier);
        return (ptcl_statement_assign){};
    }

    if (!with_type)
    {
        type = value->return_type;
        if (define && type.type != ptcl_value_function_pointer_type)
        {
            type.is_static = false;
            if (type.type == ptcl_value_array_type && type.array.target->type != ptcl_value_typedata_type)
            {
                type.type = ptcl_value_pointer_type;
                type.pointer.target = type.array.target;
            }
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
        ptcl_parser_instance variable = ptcl_parser_variable_create(name, type, value, is_static, is_global ? NULL : parser->root);
        if (!ptcl_parser_add_instance(parser, variable))
        {
            ptcl_statement_assign_destroy(assign);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_assign){};
        }
    }
    else
    {
        if (created != NULL)
        {
            created->variable.built_in = value;
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

    return ptcl_statement_return_create(ptcl_parser_cast(parser, parser->return_type, false));
}

ptcl_statement *ptcl_parser_if(ptcl_parser *parser, bool is_static)
{
    ptcl_parser_match(parser, ptcl_token_if_type);

    ptcl_type type = ptcl_type_integer;
    type.is_static = is_static;
    ptcl_expression *condition = ptcl_parser_cast(parser, &type, false);
    if (parser->is_critical)
    {
        return NULL;
    }

    if (is_static)
    {
        ptcl_statement_func_body result;
        const bool condtion_value = condition->integer_n;
        ptcl_location location = condition->location;
        ptcl_expression_destroy(condition);
        if (condtion_value)
        {
            result = ptcl_parser_func_body(parser, true, false);
            if (parser->is_critical)
            {
                return NULL;
            }

            if (ptcl_parser_match(parser, ptcl_token_else_type))
            {
                ptcl_parser_except(parser, ptcl_token_left_curly_type);
                if (parser->is_critical)
                {
                    ptcl_statement_func_body_destroy(result);
                    return NULL;
                }

                ptcl_parser_skip_block_or_expression(parser);
                if (parser->is_critical)
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
            if (parser->is_critical)
            {
                return NULL;
            }

            if (ptcl_parser_match(parser, ptcl_token_else_type))
            {
                result = ptcl_parser_func_body(parser, true, false);
                if (parser->is_critical)
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

    ptcl_statement_func_body body = ptcl_parser_func_body(parser, true, true);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(condition);
        return NULL;
    }

    ptcl_statement_if if_stat = ptcl_statement_if_create(condition, body, false, (ptcl_statement_func_body){0});
    if (ptcl_parser_match(parser, ptcl_token_else_type))
    {
        if_stat.else_body = ptcl_parser_func_body(parser, true, true);
        if_stat.with_else = !parser->is_critical;
        if (parser->is_critical)
        {
            ptcl_statement_if_destroy(if_stat);
            return NULL;
        }
    }

    ptcl_statement *statement = ptcl_statement_create(ptcl_statement_if_type, parser->root, ptcl_attributes_create(NULL, 0), condition->location);
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
    if (parser->is_critical)
    {
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_left_curly_type);
    if (parser->is_critical)
    {
        return NULL;
    }

    if (is_static)
    {
        ptcl_expression *result;

        if (condition->integer_n)
        {
            result = ptcl_parser_cast(parser, NULL, false);
            if (parser->is_critical)
            {
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_right_curly_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_else_type);
            ptcl_parser_except(parser, ptcl_token_left_curly_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return NULL;
            }

            ptcl_parser_skip_block_or_expression(parser);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return NULL;
            }
        }
        else
        {
            ptcl_parser_skip_block_or_expression(parser);
            if (parser->is_critical)
            {
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_else_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_left_curly_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return NULL;
            }

            result = ptcl_parser_cast(parser, NULL, false);
            if (parser->is_critical)
            {
                return NULL;
            }

            ptcl_parser_except(parser, ptcl_token_right_curly_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return NULL;
            }
        }

        return result;
    }

    ptcl_expression *body = ptcl_parser_cast(parser, NULL, false);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(condition);
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_right_curly_type);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_else_type);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_left_curly_type);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return NULL;
    }

    ptcl_expression *else_body = ptcl_parser_cast(parser, &body->return_type, false);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return NULL;
    }

    ptcl_parser_except(parser, ptcl_token_right_curly_type);
    if (parser->is_critical)
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
    ptcl_statement_func_body body = ptcl_parser_func_body(parser, true, false);
    parser->is_syntax_body = true;
    return body;
}

void ptcl_parser_syntax_decl(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_syntax_type);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_parser_instance instance = ptcl_parser_syntax_create(ptcl_name_create_fast_w("", false), parser->root, NULL, 0);
    ptcl_parser_syntax *syntax = &instance.syntax;
    bool is_injector = false;

    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_left_curly_type), location);
            ptcl_parser_syntax_destroy(*syntax);
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
            ptcl_parser_syntax_destroy(*syntax);
            return;
        }

        bool breaked = false;
        ptcl_parser_syntax_node node;
        switch (current.type)
        {
        case ptcl_token_left_square_type:
            ptcl_token next = ptcl_parser_current(parser);
            ptcl_parser_skip(parser);
            switch (next.type)
            {
            case ptcl_token_word_type:
                ptcl_parser_instance *variable;
                if (ptcl_parser_match(parser, ptcl_token_colon_type))
                {
                    ptcl_type type = ptcl_parser_type(parser, true, true);
                    if (parser->is_critical)
                    {
                        ptcl_parser_syntax_destroy(*syntax);
                        return;
                    }

                    ptcl_parser_except(parser, ptcl_token_right_square_type);
                    if (parser->is_critical)
                    {
                        ptcl_type_destroy(type);
                        ptcl_parser_syntax_destroy(*syntax);
                        return;
                    }

                    node = ptcl_parser_syntax_node_create_variable(type, next.value);
                    break;
                }
                else if (strcmp(next.value, "this") == 0)
                {
                    ptcl_parser_except(parser, ptcl_token_right_square_type);
                    if (parser->is_critical)
                    {
                        ptcl_parser_syntax_destroy(*syntax);
                        return;
                    }

                    ptcl_parser_except(parser, ptcl_token_left_curly_type);
                    if (parser->is_critical)
                    {
                        ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_curly_type), location);
                        ptcl_parser_syntax_destroy(*syntax);
                        return;
                    }

                    is_injector = true;
                    breaked = true;
                    break;
                }

                parser->position--;
                break;
            case ptcl_token_exclamation_mark_type:
                size_t start = parser->position--; // comeback to '!'
                ptcl_name word = ptcl_parser_name_word(parser);
                if (parser->is_critical)
                {
                    ptcl_parser_syntax_destroy(*syntax);
                    return;
                }

                if (ptcl_parser_try_get_instance(parser, word, ptcl_parser_instance_variable_type, &variable) && variable->variable.type.type == ptcl_expression_object_type_type)
                {
                    node = ptcl_parser_syntax_node_create_object_type(*ptcl_type_get_target(variable->variable.type));
                    ptcl_name_destroy(word);
                    break;
                }

                ptcl_name_destroy(word);
                parser->position = start;
                break;
            default:
                parser->position--;

                char *copy = ptcl_string_duplicate(current.value);
                if (copy == NULL)
                {
                    ptcl_parser_syntax_destroy(*syntax);
                    return;
                }

                node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(copy, true, current.location));
                break;
            }

            break;
        default:
            char *copy = ptcl_string_duplicate(current.value);
            if (copy == NULL)
            {
                ptcl_parser_syntax_destroy(*syntax);
                return;
            }

            node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(copy, true, current.location));
            break;
        }

        if (breaked)
        {
            break;
        }

        ptcl_parser_syntax_node *buffer = realloc(syntax->nodes, (syntax->count + 1) * sizeof(ptcl_parser_syntax_node));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_parser_syntax_destroy(*syntax);
            return;
        }

        syntax->nodes = buffer;
        syntax->nodes[syntax->count++] = node;
    }

    syntax->start = parser->position;
    ptcl_parser_skip_block_or_expression(parser);
    if (parser->is_critical)
    {
        ptcl_parser_syntax_destroy(*syntax);
        return;
    }

    // -1 because of curly
    syntax->tokens_count = (parser->position - syntax->start) - 1;
    if (parser->is_critical)
    {
        ptcl_parser_syntax_destroy(*syntax);
        return;
    }

    if (!is_injector)
    {
        if (!ptcl_parser_add_instance(parser, instance))
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_parser_syntax_destroy(*syntax);
            return;
        }
    }

    if (is_injector && !ptcl_parser_add_this_pair(parser, ptcl_parser_this_s_pair_create(parser->main_root, syntax->start, syntax->tokens_count)))
    {
        ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
        ptcl_parser_syntax_destroy(*syntax);
        return;
    }
}

void ptcl_parser_each(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_each_type);
    ptcl_parser_match(parser, ptcl_token_left_par_type);

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name name = ptcl_parser_name_word(parser);
    if (parser->is_critical)
    {
        return;
    }

    ptcl_parser_except(parser, ptcl_token_comma_type);
    if (parser->is_critical)
    {
        return;
    }

    ptcl_type array = ptcl_type_create_pointer(&ptcl_type_any);
    array.is_static = true;
    ptcl_expression *value = ptcl_parser_cast(parser, &array, false);
    if (parser->is_critical)
    {
        return;
    }

    ptcl_parser_match(parser, ptcl_token_right_par_type);
    ptcl_statement_func_body empty = {
        .statements = NULL,
        .count = 0,
        .root = parser->root};
    size_t position = parser->position;
    for (size_t i = 0; i < value->array.count; i++)
    {
        ptcl_expression *expression = value->array.expressions[i];
        expression->is_original = false;
        ptcl_parser_instance variable = ptcl_parser_variable_create(name, *ptcl_type_get_target(value->return_type), expression, true, &empty);
        size_t idenitifer = parser->instances_count;
        if (!ptcl_parser_add_instance(parser, variable))
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_expression_destroy(value);
            ptcl_statement_func_body_destroy(*parser->root);
            return;
        }

        ptcl_parser_func_body_by_pointer(parser, &empty, true, true);
        if (i != value->array.count - 1)
        {
            parser->position = position;
        }

        expression->is_original = true;
        parser->instances[idenitifer].variable.is_built_in = false;
        if (parser->is_critical)
        {
            ptcl_expression_destroy(value);
            return;
        }
    }

    size_t count = parser->root->count + empty.count;
    ptcl_statement **buffer = realloc(parser->root->statements, count * sizeof(ptcl_statement *));
    if (buffer == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_func_body_destroy(empty);
        ptcl_expression_destroy(value);
        return;
    }

    parser->root->statements = buffer;
    for (size_t i = 0; i < empty.count; i++)
    {
        parser->root->statements[i + parser->root->count] = empty.statements[i];
    }

    parser->root->count = count;
    free(empty.statements);
    ptcl_expression_destroy(value);
}

void ptcl_parser_undefine(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_undefine_type);
    ptcl_parser_match(parser, ptcl_token_left_par_type);
    ptcl_name name = ptcl_parser_name_word(parser);
    if (parser->is_critical)
    {
        return;
    }

    ptcl_parser_instance *instance;
    if (ptcl_parser_try_get_instance_any(parser, name, &instance))
    {
        instance->is_out_of_scope = true;
    }

    ptcl_parser_match(parser, ptcl_token_right_par_type);
}

ptcl_expression *ptcl_parser_cast(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
    size_t start = parser->position;
    if (parser->is_syntax_body && ptcl_parser_parse_try_syntax_usage_here(parser, false))
    {
        parser->position = start;
        ptcl_expression *expression = ptcl_parser_cast(parser, expected, with_word);
        ptcl_parser_leave_from_syntax(parser);
        return expression;
    }

    if (parser->is_critical)
    {
        return NULL;
    }

    ptcl_expression *left = ptcl_parser_binary(parser, expected, with_word);
    if (parser->is_critical)
    {
        return left;
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
        if (parser->is_critical)
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

        ptcl_expression *cast = ptcl_expression_cast_create(left, type, location);
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
    if (parser->is_critical)
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
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        left = ptcl_expression_binary_create(type, left, right, left->location);
        if (left == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, left->location);
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            return NULL;
        }

        // Conditions always return integer type
        bool is_static = left->return_type.is_static;
        left->return_type = ptcl_type_integer;
        left->return_type.is_static = is_static;
    }

    left = ptcl_expression_binary_static_evaluate(left);
    if (left == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
        return left;
    }

    return left;
}

ptcl_expression *ptcl_parser_additive(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
    ptcl_expression *left = ptcl_parser_multiplicative(parser, expected, with_word);
    if (parser->is_critical || !ptcl_value_is_number(left->return_type.type))
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
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        left = ptcl_expression_binary_create(type, left, right, left->location);
        if (left == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, left->location);
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            return NULL;
        }
    }

    left = ptcl_expression_binary_static_evaluate(left);
    if (left == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
    }

    return left;
}

ptcl_expression *ptcl_parser_multiplicative(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
    ptcl_expression *left = ptcl_parser_unary(parser, false, expected, with_word);
    if (parser->is_critical || !ptcl_value_is_number(left->return_type.type))
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
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        left = ptcl_expression_binary_create(type, left, right, left->location);
        if (left == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, left->location);
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            return NULL;
        }
    }

    left = ptcl_expression_binary_static_evaluate(left);
    if (left == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
    }

    return left;
}

ptcl_expression *ptcl_parser_unary(ptcl_parser *parser, bool only_value, ptcl_type *expected, bool with_word)
{
    if (ptcl_parser_ended(parser))
    {
        return ptcl_parser_dot(parser, expected, false, with_word);
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
        if (parser->is_critical)
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
                 value->return_type.pointer.is_any) ||
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

        ptcl_expression *result = ptcl_expression_unary_create(type, value, value->location);
        if (result == NULL)
        {
            ptcl_expression_destroy(value);
            ptcl_parser_throw_out_of_memory(parser, value->location);
            return NULL;
        }

        result->return_type = type_value;
        result->return_type.is_static = value->return_type.is_static;
        ptcl_expression *evaluated = ptcl_expression_unary_static_evaluate(result, result->unary);
        if (evaluated == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            return NULL;
        }

        return evaluated;
    }

    return ptcl_parser_dot(parser, expected, false, with_word);
}

static ptcl_expression *ptcl_parser_type_dot(ptcl_parser *parser, ptcl_expression *left,
                                             ptcl_name name, ptcl_location location)
{
    ptcl_type_comp_type *comp_type = left->return_type.comp_type;
    ptcl_statement_func_decl function;
    bool function_found = false;
    if (comp_type->functions != NULL)
    {
        for (size_t i = 0; i < comp_type->functions->count; i++)
        {
            function = comp_type->functions->statements[i]->func_decl;
            if (ptcl_name_compare(function.name, name))
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

    ptcl_parser_instance target = ptcl_parser_function_create(parser->root, function);
    ptcl_statement_func_call func_call = ptcl_parser_func_call(parser, &target.function, true);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(left);
        return NULL;
    }

    func_call.is_built_in = false;
    func_call.return_type = function.return_type;
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
                                                 ptcl_name name, ptcl_location location)
{
    ptcl_typedata_member *member;
    if (!ptcl_parser_try_get_typedata_member(parser, left->return_type.typedata, name.value, &member))
    {
        ptcl_parser_throw_unknown_member(parser, name.value, location);
        ptcl_expression_destroy(left);
        return NULL;
    }

    if (left->return_type.is_static)
    {
        ptcl_expression *static_value = ptcl_expression_copy(left->ctor.values[member->index], location);
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

    dot_expr->dot = ptcl_expression_dot_create(left, name);
    return dot_expr;
}

ptcl_expression *ptcl_parser_dot(ptcl_parser *parser, ptcl_type *expected, ptcl_expression *left, bool with_word)
{
    if (left == NULL)
    {
        left = ptcl_parser_array_element(parser, expected, NULL, with_word);
    }

    if (parser->is_critical ||
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
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        ptcl_expression *result;
        if (left->return_type.type == ptcl_value_type_type)
        {
            result = ptcl_parser_type_dot(parser, left, name, location);
        }
        else
        {
            result = ptcl_parser_typedata_dot(parser, left, name, location);
        }

        if (parser->is_critical)
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
            left = ptcl_parser_dot(parser, expected, left, false);
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

    if (parser->is_critical ||
        (left->return_type.type != ptcl_value_array_type && left->return_type.type != ptcl_value_pointer_type) ||
        ptcl_parser_ended(parser))
    {
        return left;
    }

    if (ptcl_parser_match(parser, ptcl_token_left_square_type))
    {
        ptcl_expression *index = ptcl_parser_cast(parser, &ptcl_type_integer, false);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        ptcl_parser_except(parser, ptcl_token_right_square_type);
        if (parser->is_critical)
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
    if (parser->is_critical)
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
        if (parser->is_critical)
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
    size_t start = parser->position;
    ptcl_type *previous = parser->return_type;
    parser->return_type = &ptcl_type_void;
    size_t block_start = parser->position - 1;

    ptcl_parser_skip_block_or_expression(parser);
    if (parser->is_critical)
    {
        return NULL;
    }

    parser->return_type = previous;
    size_t token_count = parser->position - block_start;
    ptcl_token *body_tokens = NULL;
    if (token_count > 0)
    {
        body_tokens = malloc(sizeof(ptcl_token) * token_count);
        if (body_tokens == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            return NULL;
        }

        for (size_t i = block_start; i < parser->position; i++)
        {
            ptcl_token copy = parser->tokens[i];
            copy.value = ptcl_string_duplicate(copy.value);
            if (copy.value == NULL)
            {
                for (size_t j = block_start; j < i; j++)
                {
                    ptcl_token_destroy(body_tokens[i]);
                }

                free(body_tokens);
                ptcl_parser_throw_out_of_memory(parser, location);
                return NULL;
            }

            body_tokens[i - block_start] = copy;
        }
    }

    ptcl_type func = ptcl_type_create_func(&ptcl_type_void, NULL, 0);
    func.is_static = true;
    ptcl_expression *result = ptcl_expression_create(ptcl_expression_lated_func_body_type, func, location);
    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        free(body_tokens);
        return NULL;
    }

    if (parser->lated_bodies_count >= parser->lated_bodies_capacity)
    {
        size_t new_capacity = parser->lated_bodies_capacity == 0 ? 8 : parser->lated_bodies_capacity * 2;
        ptcl_lated_body *buffer = realloc(parser->lated_bodies,
                                          sizeof(ptcl_lated_body) * new_capacity);
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            free(result);
            return NULL;
        }

        parser->lated_bodies = buffer;
        parser->lated_bodies_capacity = new_capacity;
    }

    if (parser->lated_bodies_count < parser->lated_bodies_capacity)
    {
        ptcl_lated_body *body = &parser->lated_bodies[parser->lated_bodies_count];
        body->tokens = body_tokens;
        body->count = token_count;
        body->start = block_start;

        result->lated_body.index = parser->lated_bodies_count;
        parser->lated_bodies_count++;
    }
    else
    {
        free(body_tokens);
        result->lated_body.index = (size_t)-1;
    }

    return result;
}

static ptcl_expression *ptcl_parser_try_ctor(ptcl_parser *parser, ptcl_name name, ptcl_parser_instance *typedata, ptcl_location location)
{
    ptcl_expression *result = ptcl_expression_create(
        ptcl_expression_ctor_type, ptcl_type_create_typedata(typedata->name.value, typedata->name.is_anonymous), location);
    ptcl_name_destroy(name);
    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        return NULL;
    }

    result->ctor = ptcl_parser_ctor(parser, typedata->name, typedata->typedata);
    if (parser->is_critical)
    {
        free(result);
        return NULL;
    }

    return result;
}

static ptcl_expression *ptcl_parser_try_func(ptcl_parser *parser, ptcl_name name, ptcl_parser_function *func_instance, ptcl_token current)
{
    ptcl_statement_func_call func_call = ptcl_parser_func_call(parser, func_instance, true);
    if (parser->is_critical)
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

static ptcl_expression *ptcl_parser_func_call_or_var(ptcl_parser *parser, ptcl_name name, ptcl_token current, bool with_word)
{
    ptcl_location location = current.location;
    ptcl_parser_instance *instance;
    ptcl_expression *result = NULL;

    // TODO: create type search in one loop
    if (ptcl_parser_current(parser).type == ptcl_token_left_par_type)
    {
        size_t start = parser->position;
        const bool last_state = parser->add_errors;
        parser->add_errors = !with_word;

        if (ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_typedata_type, &instance))
        {
            result = ptcl_parser_try_ctor(parser, name, instance, location);
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

        if (parser->is_critical && with_word)
        {
            parser->is_critical = false;
            parser->add_errors = last_state;
            parser->position = start + 1;
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

        if (result == NULL && !parser->is_critical)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        }

        return result;
    }
    else if (ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_variable_type, &instance))
    {
        if (instance->variable.is_built_in)
        {
            result = ptcl_expression_copy(instance->variable.built_in, current.location);
            if (result != NULL)
            {
                result->return_type.is_static = instance->variable.built_in->return_type.is_static || !instance->variable.is_syntax_variable;
                ptcl_name_destroy(name);
            }
        }
        else
        {
            char *name_copy = ptcl_string_duplicate(name.value);
            if (name_copy != NULL)
            {
                name.value = name_copy;
                name.is_free = true;
                result = ptcl_expression_create_variable(name, instance->variable.type, current.location);
                if (result == NULL)
                {
                    ptcl_name_destroy(name);
                }
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
    if (parser->is_critical)
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
        ptcl_expression *excepted = ptcl_parser_cast(parser, NULL, false);
        if (parser->is_critical)
        {
            return NULL;
        }

        ptcl_parser_except(parser, ptcl_token_right_par_type);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(excepted);
            return NULL;
        }

        ptcl_expression *default_value = ptcl_parser_get_default(parser, excepted->return_type, location);
        ptcl_expression_destroy(excepted);
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

    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, location);
    }

    return result;
}

ptcl_expression *ptcl_parser_value(ptcl_parser *parser, ptcl_type *expected, bool with_word)
{
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
        parser->position--;
        return ptcl_parser_at(parser, expected, with_word, location);
    case ptcl_token_left_curly_type:
        return ptcl_parser_lated_body_void(parser, location);
    case ptcl_token_exclamation_mark_type:
        parser->position--;
        ptcl_name word_name = ptcl_parser_name_word(parser);
        if (parser->is_critical)
        {
            return NULL;
        }
    case ptcl_token_word_type:
        if (current.type != ptcl_token_exclamation_mark_type)
        {
            word_name = ptcl_name_create_fast_w(current.value, is_anonymous);
        }

        return ptcl_parser_func_call_or_var(parser, word_name, current, with_word);
    case ptcl_token_word_word_type:
        parser->position--;
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
        ptcl_expression *character = ptcl_expression_create_character(current.value[0], current.location);
        if (character == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        }

        return character;
    case ptcl_token_static_type:
        ptcl_parser_except(parser, ptcl_token_if_type);
        if (parser->is_critical)
        {
            return NULL;
        }
    case ptcl_token_if_type:
        bool is_static = current.type == ptcl_token_static_type;
        return ptcl_parser_if_expression(parser, is_static);
    case ptcl_token_left_par_type:
        ptcl_expression *inside = ptcl_parser_cast(parser, expected, with_word);
        if (parser->is_critical)
        {
            return NULL;
        }

        ptcl_parser_except(parser, ptcl_token_right_par_type);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(inside);
        }

        return inside;
    case ptcl_token_greater_than_type:
        return ptcl_parser_type_object(parser, location);
    case ptcl_token_null_type:
        if (expected == NULL || (expected->type != ptcl_value_pointer_type && expected->type != ptcl_value_function_pointer_type))
        {
            ptcl_type received = expected == NULL ? ptcl_type_any_pointer : *expected;
            ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_any_pointer, received, current.location);
            return NULL;
        }

        ptcl_expression *null_expr = ptcl_expression_create_null(*expected, current.location);
        if (null_expr == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
        }

        return null_expr;
    case ptcl_token_none_type:
        return ptcl_parse_none(parser, expected, location);
    default:
        ptcl_parser_throw_unknown_expression(parser, current.location);
    }

    return NULL;
}

ptcl_expression_ctor ptcl_parser_ctor(ptcl_parser *parser, ptcl_name name, ptcl_parser_typedata typedata)
{
    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (parser->is_critical)
    {
        return (ptcl_expression_ctor){};
    }

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_expression_ctor ctor = ptcl_expression_ctor_create(name, NULL, 0);
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser) || (ctor.count + 1) > typedata.count)
        {
            ptcl_expression_ctor_destroy(ctor);
            ptcl_parser_throw_except_token(
                parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            return (ptcl_expression_ctor){};
        }

        ptcl_expression **buffer = realloc(ctor.values, (ctor.count + 1) * sizeof(ptcl_expression *));
        if (buffer == NULL)
        {
            ptcl_expression_ctor_destroy(ctor);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_expression_ctor){};
        }

        ptcl_expression *value = ptcl_parser_cast(parser, &typedata.members[ctor.count].type, false);
        if (parser->is_critical)
        {
            if (ctor.count == 0)
            {
                free(buffer);
            }

            ptcl_expression_ctor_destroy(ctor);
            return (ptcl_expression_ctor){};
        }

        ctor.values = buffer;
        ctor.values[ctor.count++] = value;
        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    if (ctor.count < typedata.count)
    {
        ptcl_expression_ctor_destroy(ctor);
        ptcl_parser_throw_except_token(
            parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_comma_type), location);
        return (ptcl_expression_ctor){};
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

ptcl_name ptcl_parser_name(ptcl_parser *parser, bool with_type)
{
    ptcl_token operator = ptcl_parser_current(parser);
    if (operator.type == ptcl_token_exclamation_mark_type)
    {
        ptcl_parser_skip(parser);
        const bool is_anonymous = ptcl_parser_match(parser, ptcl_token_tilde_type);
        ptcl_token current = ptcl_parser_current(parser);
        ptcl_parser_skip(parser);
        switch (current.type)
        {
        case ptcl_token_less_than_type:
            bool left_succesful;
            ptcl_name left = ptcl_parser_name(parser, false);
            if (parser->is_critical)
            {
                return (ptcl_name){};
            }

            ptcl_name right = ptcl_parser_name(parser, false);
            if (parser->is_critical)
            {
                ptcl_name_destroy(left);
                return (ptcl_name){};
            }

            ptcl_parser_except(parser, ptcl_token_greater_than_type);
            if (parser->is_critical)
            {
                ptcl_name_destroy(left);
                ptcl_name_destroy(right);
                return (ptcl_name){};
            }

            char *concatenated = ptcl_string(left.value, right.value, NULL);
            ptcl_name_destroy(left);
            ptcl_name_destroy(right);
            if (concatenated == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, left.location);
                return (ptcl_name){};
            }

            return ptcl_name_create(concatenated, true, left.location);
        }

        ptcl_parser_instance *instance;
        ptcl_name name = ptcl_name_create_fast_w(current.value, is_anonymous);
        if (!ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_variable_type, &instance))
        {
            ptcl_parser_throw_unknown_variable(parser, name.value, current.location);
            return (ptcl_name){};
        }

        bool incorrect_type = false;
        bool is_free = false;
        char *value;
        if (!instance->variable.type.is_static)
        {
            incorrect_type = true;
        }
        else
        {
            switch (instance->variable.type.type)
            {
            case ptcl_value_object_type_type:
                if (!with_type)
                {
                    incorrect_type = true;
                    break;
                }
                ptcl_name result = instance->name;
            case ptcl_value_word_type:
                if (instance->variable.type.type == ptcl_value_word_type)
                {
                    result = instance->variable.built_in->word;
                }

                return ptcl_name_create(result.value, false, current.location);
            default:
                incorrect_type = true;
                break;
            }
        }

        if (incorrect_type)
        {
            ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_word, instance->variable.type, current.location);
            return (ptcl_name){};
        }

        return (ptcl_name){};
    }
    else if (operator.type == ptcl_token_word_word_type)
    {
        ptcl_parser_skip(parser);
        ptcl_location location = ptcl_parser_current(parser).location;
        ptcl_parser_except(parser, ptcl_token_left_par_type);
        if (parser->is_critical)
        {
            return (ptcl_name){};
        }

        ptcl_expression *object_type = NULL;
        ptcl_name target;
        if (ptcl_parser_current(parser).type == ptcl_token_greater_than_type)
        {
            object_type = ptcl_parser_value(parser, NULL, false);
            if (parser->is_critical)
            {
                return (ptcl_name){};
            }

            ptcl_parser_except(parser, ptcl_token_right_par_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(object_type);
                return (ptcl_name){};
            }

            char *value = ptcl_type_to_word_copy(object_type->object_type.type);
            ptcl_expression_destroy(object_type);
            if (value == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, location);
                return (ptcl_name){};
            }

            return ptcl_name_create(value, true, location);
        }
        else
        {
            target = ptcl_parser_name_word(parser);
            if (parser->is_critical)
            {
                return (ptcl_name){};
            }
        }

        ptcl_parser_except(parser, ptcl_token_right_par_type);
        if (parser->is_critical)
        {
            ptcl_name_destroy(target);
            return (ptcl_name){};
        }

        ptcl_parser_instance *variable;
        if (!ptcl_parser_try_get_instance(parser, target, ptcl_parser_instance_variable_type, &variable))
        {
            ptcl_parser_throw_unknown_variable(parser, target.value, location);
            ptcl_name_destroy(target);
            return (ptcl_name){};
        }

        bool incorrect_type = false;
        bool is_free = false;
        char *value;
        if (!variable->variable.type.is_static)
        {
            incorrect_type = true;
        }
        else
        {
            switch (variable->variable.type.type)
            {
            case ptcl_value_object_type_type:
                is_free = true;
                value = ptcl_type_to_word_copy(variable->variable.built_in->object_type.type);
                if (value == NULL)
                {
                    ptcl_parser_throw_out_of_memory(parser, location);
                    ptcl_name_destroy(target);
                    return (ptcl_name){};
                }

                break;
            case ptcl_value_word_type:
                value = variable->variable.built_in->word.value;
                break;
            default:
                incorrect_type = true;
                break;
            }
        }

        if (incorrect_type)
        {
            ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_word, variable->variable.type, location);
            ptcl_name_destroy(target);
            return (ptcl_name){};
        }

        ptcl_name_destroy(target);
        return ptcl_name_create(value, is_free, location);
    }
    else if (operator.type == ptcl_token_hashtag_type)
    {
        size_t start = parser->position;
        if (!ptcl_parser_parse_try_syntax_usage_here(parser, false))
        {
            return (ptcl_name){};
        }

        parser->position = start;
        ptcl_name name = ptcl_parser_name(parser, with_type);
        ptcl_parser_leave_from_syntax(parser);
        return name;
    }

    const bool is_anonymous = ptcl_parser_match(parser, ptcl_token_tilde_type);
    ptcl_token token = ptcl_parser_except(parser, ptcl_token_word_type);
    if (parser->is_critical)
    {
        return (ptcl_name){};
    }

    return ptcl_name_create_l(token.value, is_anonymous, false, token.location);
}

ptcl_expression *ptcl_parser_get_default(ptcl_parser *parser, ptcl_type type, ptcl_location location)
{
    switch (type.type)
    {
    case ptcl_value_typedata_type:
        ptcl_parser_instance *instance;
        // If type was created, then type registered
        ptcl_parser_try_get_instance(parser, type.typedata, ptcl_parser_instance_typedata_type, &instance);
        ptcl_expression **expressions = malloc(instance->typedata.count * sizeof(ptcl_expression *));
        if (expressions == NULL)
        {
            return NULL;
        }

        for (size_t i = 0; i < instance->typedata.count; i++)
        {
            expressions[i] = ptcl_parser_get_default(parser, instance->typedata.members[i].type, location);
            if (parser->is_critical)
            {
                for (size_t j = 0; j < i - 1; j++)
                {
                    ptcl_expression_destroy(expressions[j]);
                }

                free(expressions);
                return NULL;
            }
        }

        ptcl_name typedata_name = instance->name;
        typedata_name.is_free = false;
        ptcl_expression *ctor = ptcl_expression_create(ptcl_expression_ctor_type, ptcl_type_copy(type), location);
        if (ctor == NULL)
        {
            for (size_t i = 0; i < instance->typedata.count; i++)
            {
                ptcl_expression_destroy(expressions[i]);
            }

            free(expressions);
            return NULL;
        }

        ctor->ctor = ptcl_expression_ctor_create(typedata_name, expressions, instance->typedata.count);
        return ctor;
    case ptcl_value_pointer_type:
        return ptcl_expression_create_null(type, location);
    case ptcl_value_array_type:
        return ptcl_expression_create_array(ptcl_type_copy(type), NULL, 0, location);
    case ptcl_value_any_type:
    case ptcl_value_object_type_type:
        return ptcl_expression_create_object_type(ptcl_type_create_object_type(&ptcl_type_any_type), ptcl_type_any_type, location);
    case ptcl_value_word_type:
        return ptcl_expression_word_create(ptcl_name_create_fast_w("", false), location);
    case ptcl_value_double_type:
        return ptcl_expression_create_double(0, location);
    case ptcl_value_float_type:
        return ptcl_expression_create_float(0, location);
    case ptcl_value_integer_type:
        return ptcl_expression_create_integer(0, location);
    case ptcl_value_character_type:
        return ptcl_expression_create_character('\0', location);
    }
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
    if (ptcl_parser_current(parser).type == token_type)
    {
        ptcl_parser_skip(parser);
        return true;
    }

    return false;
}

ptcl_token ptcl_parser_peek(ptcl_parser *parser, size_t offset)
{
    size_t position = parser->position + offset;
    size_t count = parser->count - 1;
    if (position > count)
    {
        return parser->tokens[count];
    }

    return parser->tokens[position];
}

ptcl_token ptcl_parser_current(ptcl_parser *parser)
{
    if (ptcl_parser_ended(parser))
    {
        return parser->tokens[parser->count - 1];
    }

    return parser->tokens[parser->position];
}

void ptcl_parser_skip(ptcl_parser *parser)
{
    parser->position++;
}

bool ptcl_parser_ended(ptcl_parser *parser)
{
    return parser->position > parser->count - 1;
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

bool ptcl_parser_insert_pairs(ptcl_parser *parser, ptcl_statement_func_body *body)
{
    for (int i = parser->this_pairs_count - 1; i >= 0; i--)
    {
        ptcl_parser_this_s_pair *target = &parser->this_pairs[i];
        if (!ptcl_func_body_can_access(body, target->body))
        {
            continue;
        }

        if (ptcl_parser_insert_syntax(parser, target->start, target->tokens_count, parser->position, parser->position, false))
        {
            continue;
        }

        return false;
    }

    return true;
}

bool ptcl_parser_add_instance(ptcl_parser *parser, ptcl_parser_instance instance)
{
    if (instance.root != NULL && parser->syntax_depth > 0 && !instance.name.is_anonymous)
    {
        instance.root = parser->main_root;
    }

    ptcl_parser_instance *buffer = realloc(parser->instances, (parser->instances_count + 1) * sizeof(ptcl_parser_instance));
    if (buffer == NULL)
    {
        return false;
    }

    parser->instances = buffer;
    parser->instances[parser->instances_count++] = instance;
    return true;
}

bool ptcl_parser_try_get_instance(ptcl_parser *parser, ptcl_name name, ptcl_parser_instance_type type, ptcl_parser_instance **instance)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *target = &parser->instances[i];
        if (target->type != type || target->is_out_of_scope)
        {
            continue;
        }

        if (!ptcl_name_compare(target->name, name) || !ptcl_func_body_can_access(target->root, parser->root))
        {
            continue;
        }

        *instance = target;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_instance_any(ptcl_parser *parser, ptcl_name name, ptcl_parser_instance **instance)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *target = &parser->instances[i];
        if (target->is_out_of_scope)
        {
            continue;
        }

        if (!ptcl_name_compare(target->name, name) || !ptcl_func_body_can_access(target->root, parser->root))
        {
            continue;
        }

        *instance = target;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_instance_in_root(ptcl_parser *parser, ptcl_name name, ptcl_parser_instance_type type, ptcl_parser_instance **instance)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *target = &parser->instances[i];
        if (target->type != type || target->is_out_of_scope)
        {
            continue;
        }

        if (target->root != parser->root || !ptcl_name_compare(target->name, name))
        {
            continue;
        }

        *instance = target;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_function(ptcl_parser *parser, ptcl_name name, ptcl_parser_function **function)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *instance = &parser->instances[i];
        if (instance->type != ptcl_parser_instance_function_type || instance->is_out_of_scope)
        {
            continue;
        }

        ptcl_parser_function *target = &instance->function;
        if (!ptcl_name_compare(target->func.name, name) || !ptcl_func_body_can_access(instance->root, parser->root))
        {
            continue;
        }

        *function = target;
        return true;
    }

    return false;
}

bool ptcl_parser_is_defined(ptcl_parser *parser, ptcl_name name)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *instance = &parser->instances[i];
        if (instance->is_out_of_scope || !ptcl_name_compare(instance->name, name))
        {
            continue;
        }

        if (instance->name.is_anonymous)
        {
            return instance->root == parser->root;
        }

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

    bool breaked = false;
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

        return strcmp(left->word.name.value, right->word.name.value) == 0;
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
                                 ptcl_parser_syntax **syntax, bool *can_continue)
{
    *syntax = NULL;
    *can_continue = false;
    bool full_match_found = false;
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *instance = &parser->instances[i];
        if (instance->type != ptcl_parser_instance_syntax_type || instance->is_out_of_scope)
        {
            continue;
        }

        ptcl_parser_syntax *target = &instance->syntax;
        if (target->count != count)
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

    for (size_t i = 0; i < parser->instances_count; i++)
    {
        ptcl_parser_instance *instance = &parser->instances[i];
        if (instance->type != ptcl_parser_instance_syntax_type)
        {
            continue;
        }

        ptcl_parser_syntax *target = &instance->syntax;
        if (target->count <= count)
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
            *can_continue = true;
            break;
        }
    }

    return full_match_found;
}

bool ptcl_parser_try_get_typedata_member(ptcl_parser *parser, ptcl_name name, char *member_name, ptcl_typedata_member **member)
{
    ptcl_parser_instance *typedata;
    if (!ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_typedata_type, &typedata))
    {
        return false;
    }

    bool finded = false;
    ptcl_typedata_member *target;
    for (size_t j = 0; j < typedata->typedata.count; j++)
    {
        ptcl_typedata_member *typedata_member = &typedata->typedata.members[j];
        if (strcmp(typedata_member->name.value, member_name) != 0)
        {
            continue;
        }

        finded = true;
        target = typedata_member;
        break;
    }

    if (!finded)
    {
        return false;
    }

    *member = target;
    return true;
}

void ptcl_parser_clear_scope(ptcl_parser *parser)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *instance = &parser->instances[i];
        if (instance->root != parser->root)
        {
            continue;
        }

        instance->is_out_of_scope = true;
    }
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

void ptcl_parser_throw_wrong_arguments(ptcl_parser *parser, char *name, ptcl_expression **values, size_t count, ptcl_location location)
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

void ptcl_parser_add_error(ptcl_parser *parser, ptcl_parser_error error)
{
    if (!parser->add_errors)
    {
        ptcl_parser_error_destroy(error);
        if (!parser->is_critical)
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
    if (result.count > 0)
    {
        for (size_t i = 0; i < result.count; i++)
        {
            ptcl_parser_error_destroy(result.errors[i]);
        }

        free(result.errors);
    }

    if (result.instances_count > 0)
    {
        for (size_t i = 0; i < result.instances_count; i++)
        {
            ptcl_parser_instance_destroy(&result.instances[i]);
        }

        free(result.instances);
    }

    if (result.lated_bodies_count > 0)
    {
        for (size_t i = 0; i < result.lated_bodies_count; i++)
        {
            ptcl_lated_body body = result.lated_bodies[i];
            for (size_t j = 0; j < body.count; j++)
            {
                free(body.tokens[j].value);
            }

            free(body.tokens);
        }

        free(result.lated_bodies);
    }

    if (result.this_pairs > 0)
    {
        free(result.this_pairs);
    }

    if (!result.is_critical)
    {
        ptcl_statement_func_body_destroy(result.body);
    }
}

void ptcl_parser_destroy(ptcl_parser *parser)
{
    free(parser);
}
