#include <ptcl_parser.h>

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
    ptcl_parser_function *parent;
    ptcl_type *return_type;
    bool is_critical;
} ptcl_parser;

static ptcl_expression ptcl_parser_get_statements(ptcl_parser *parser, ptcl_expression *arguments, size_t count, ptcl_location location)
{
    ptcl_expression name_array = arguments[0];

    char *name_of = malloc(name_array.array.count * sizeof(char));
    if (name_of == NULL)
    {
        return ptcl_expression_create_integer(0, location);
    }

    for (size_t i = 0; i < name_array.array.count; i++)
    {
        name_of[i] = name_array.array.expressions[i].character;
    }

    ptcl_parser_function *function = NULL;
    for (size_t i = 0; i < parser->instances_count; i++)
    {
        ptcl_parser_instance *target;
        if (!ptcl_parser_try_get_instance(parser, name_of, ptcl_parser_instance_function_type, &target))
        {
            continue;
        }

        function = &target->function;
        break;
    }

    free(name_of);
    if (function == NULL)
    {
        return ptcl_expression_create_integer(0, location);
    }

    ptcl_expression *values = malloc(function->func.func_body.count * sizeof(ptcl_expression));
    for (size_t i = 0; i < function->func.func_body.count; i++)
    {
        ptcl_expression *ctor_values = malloc(sizeof(ptcl_expression));
        ctor_values[0] = ptcl_expression_create_integer((int)function->func.func_body.statements[i].type, location);
        values[i] = (ptcl_expression){
            .type = ptcl_expression_ctor_type,
            .return_type = ptcl_type_create_typedata("statement"),
            .location = location,
            .ctor = ptcl_expression_ctor_create("statement", ctor_values, 1)};
    }

    ptcl_type *type = malloc(sizeof(ptcl_type));
    *type = ptcl_type_create_typedata("statement");
    ptcl_type array_type = ptcl_type_create_array(type, function->func.func_body.count);
    array_type.is_static = true;

    return ptcl_expression_create_array(array_type, values, array_type.count, location);
}

static ptcl_type ptcl_parser_pointers(ptcl_parser *parser, ptcl_type type)
{
    if (ptcl_parser_match(parser, ptcl_token_asterisk_type))
    {
        ptcl_type *target = malloc(sizeof(ptcl_type));
        if (target == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_type_destroy(type);
            return type;
        }

        *target = type;
        target->is_primitive = false;
        type = ptcl_type_create_pointer(target);
        return ptcl_parser_pointers(parser, type);
    }
    else if (ptcl_parser_match(parser, ptcl_token_left_square_type))
    {
        ptcl_parser_except(parser, ptcl_token_right_square_type);
        if (parser->is_critical)
        {
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
        type = ptcl_type_create_array(target, -1);
        return ptcl_parser_pointers(parser, type);
    }

    return type;
}

static void ptcl_parser_set_arrays_length(ptcl_type *type, ptcl_type *target)
{
    if (type->type == ptcl_value_array_type)
    {
        type->count = target->count;
        ptcl_parser_set_arrays_length(type->target, target->target);
    }
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
    parser->parent = NULL;
    parser->return_type = NULL;
    parser->errors = NULL;
    parser->errors_count = 0;
    parser->instances = NULL;
    parser->instances_count = 0;

    ptcl_type type = ptcl_type_create_pointer(&ptcl_type_character);
    type.is_static = true;
    ptcl_type return_type = ptcl_type_create_typedata("statement");
    return_type.is_static = true;
    ptcl_argument arguments[] = {ptcl_argument_create(type, NULL)};
    ptcl_parser_instance get_statements = ptcl_parser_built_in_create(NULL, "get_statements", ptcl_parser_get_statements, arguments, 1, ptcl_type_create_pointer(&return_type));
    ptcl_parser_add_instance(parser, get_statements);

    ptcl_parser_result result = {
        .configuration = parser->configuration,
        .body = ptcl_parser_parse_func_body(parser, false),
        .errors = parser->errors,
        .count = parser->errors_count,
        .instances = parser->instances,
        .instances_count = parser->instances_count,
        .is_critical = parser->is_critical};
    parser->input->tokens = parser->tokens;
    return result;
}

ptcl_statement_type ptcl_parser_parse_get_statement(ptcl_parser *parser)
{
    ptcl_token current = ptcl_parser_current(parser);

    switch (current.type)
    {
    case ptcl_token_word_type:
        if (ptcl_parser_peek(parser, 1).type == ptcl_token_left_par_type)
        {
            return ptcl_statement_func_call_type;
        }

        return ptcl_statement_assign_type;
    case ptcl_token_return_type:
        return ptcl_statement_return_type;
    case ptcl_token_if_type:
    case ptcl_token_static_type:
        return ptcl_statement_if_type;
    case ptcl_token_each_type:
        return ptcl_statement_each_type;
    case ptcl_token_syntax_type:
        return ptcl_statement_syntax_type;
    case ptcl_token_typedata_type:
        return ptcl_statement_typedata_decl_type;
    case ptcl_token_prototype_type:
    case ptcl_token_function_type:
        return ptcl_statement_func_decl_type;
    }
}

ptcl_statement ptcl_parser_parse_statement(ptcl_parser *parser)
{
    if (ptcl_parser_parse_try_parse_syntax_usage_here(parser))
    {
        return ptcl_parser_parse_statement(parser);
    }

    if (parser->is_critical)
    {
        return (ptcl_statement){};
    }

    ptcl_statement statement;
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_statement_type type = ptcl_parser_parse_get_statement(parser);

    switch (type)
    {
    case ptcl_statement_func_call_type:
        statement = (ptcl_statement){
            .type = ptcl_statement_func_call_type,
            .func_call = ptcl_parser_parse_func_call(parser),
            .location = location};

        if (statement.func_call.is_built_in)
        {
            ptcl_expression_destroy(*statement.func_call.built_in);
        }

        break;
    case ptcl_statement_func_decl_type:
        statement = (ptcl_statement){
            .type = ptcl_statement_func_decl_type,
            .func_decl = ptcl_parser_parse_func_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type)),
            .location = location};
        break;
    case ptcl_statement_typedata_decl_type:
        statement = (ptcl_statement){
            .type = ptcl_statement_typedata_decl_type,
            .typedata_decl = ptcl_parser_parse_typedata_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type)),
            .location = location};
        break;
    case ptcl_statement_assign_type:
        statement = (ptcl_statement){
            .type = ptcl_statement_assign_type,
            .assign = ptcl_parser_parse_assign(parser),
            .location = location};
        break;
    case ptcl_statement_return_type:
        statement = (ptcl_statement){
            .type = ptcl_statement_return_type,
            .ret = ptcl_parser_parse_return(parser),
            .location = location};
        break;
    case ptcl_statement_if_type:
        bool is_static = ptcl_parser_match(parser, ptcl_token_static_type);
        statement = (ptcl_statement){
            .type = ptcl_statement_if_type,
            .if_stat = ptcl_parser_parse_if(parser, is_static),
            .location = location};

        if (is_static)
        {
            statement.type = ptcl_statement_func_body_type;

            if (statement.if_stat.condition.integer_n)
            {
                if (statement.if_stat.with_else)
                {
                    ptcl_statement_func_body_destroy(statement.if_stat.else_body);
                }

                statement = ptcl_statement_func_body_create_stat(statement.if_stat.body, location);
            }
            else
            {
                if (statement.if_stat.with_else)
                {
                    statement = ptcl_statement_func_body_create_stat(statement.if_stat.else_body, location);
                }
                else
                {
                    statement.type = ptcl_statement_each_type;
                }
            }
        }

        break;
    case ptcl_statement_each_type:
        ptcl_parser_parse_each(parser);
        statement = (ptcl_statement){
            .type = ptcl_statement_each_type,
            .location = location};
        break;
    case ptcl_statement_syntax_type:
        ptcl_parser_parse_syntax(parser);
        statement = (ptcl_statement){
            .type = ptcl_statement_syntax_type,
            .location = location};
        break;
    default:
        statement.type = 0;
        ptcl_parser_throw_unknown_statement(parser, location);
        break;
    }

    if (parser->is_critical)
    {
        return statement;
    }

    statement.root = parser->root;
    return statement;
}

static bool ptcl_parser_parse_insert_syntax(
    ptcl_parser *parser,
    ptcl_parser_syntax *syntax,
    size_t start,
    size_t stop)
{
    size_t tokens_count = parser->position - start;
    int offset = tokens_count - syntax->tokens_count;

    for (size_t i = start; i < start + tokens_count; i++)
    {
        ptcl_token_destroy(parser->tokens[i]);
    }

    if (offset <= 0)
    {
        offset *= -1;
        ptcl_token *buffer = realloc(parser->tokens, (parser->count + offset) * sizeof(ptcl_token));
        if (buffer == NULL)
        {
            return false;
        }

        parser->count += offset;
        parser->tokens = buffer;
        for (size_t i = parser->count - 1; i >= start; i--)
        {
            parser->tokens[i] = parser->tokens[i - offset];
        }

        for (size_t i = start; i < start + offset + tokens_count; i++)
        {
            ptcl_token copy = ptcl_token_same(parser->tokens[syntax->start + (i - start)]);
            parser->tokens[i] = copy;
        }
    }
    else
    {
        for (size_t i = start; i < parser->count; i++)
        {
            parser->tokens[i] = parser->tokens[i + (offset * -1)];
        }

        parser->count -= offset * -1;
    }
}

bool ptcl_parser_parse_try_parse_syntax_usage_here(ptcl_parser *parser)
{
    return ptcl_parser_parse_try_parse_syntax_usage(parser, NULL, 0, true, -1, false);
}

bool ptcl_parser_parse_try_parse_syntax_usage(ptcl_parser *parser, ptcl_parser_syntax_node *nodes, size_t count, bool is_free, int down_start, bool skip_first)
{
    size_t start = down_start >= 0 ? down_start : parser->position;
    size_t stop = parser->position;
    ptcl_parser_instance instance = ptcl_parser_syntax_create(NULL, nodes, count);
    ptcl_parser_syntax syntax = instance.syntax;
    ptcl_parser_syntax *result = NULL;

    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            break;
        }

        if (!skip_first)
        {
            ptcl_parser_syntax_node node;
            size_t position = parser->position;
            size_t count = parser->errors_count;
            ptcl_expression value = ptcl_parser_parse_binary(parser, NULL);
            if (parser->is_critical)
            {
                parser->position = position;

                ptcl_token current = ptcl_parser_current(parser);
                ptcl_parser_skip(parser);
                node = ptcl_parser_syntax_node_create_word(current.type, current.value);

                for (int i = parser->errors_count - 1; i != count - 1; i--)
                {
                    ptcl_parser_error_destroy(parser->errors[i]);
                    parser->errors_count--;
                }

                parser->errors = realloc(parser->errors, count * sizeof(ptcl_parser_error));
                parser->is_critical = false;
            }
            else
            {
                node = ptcl_parser_syntax_node_create_value(value);
            }

            ptcl_parser_syntax_node *buffer = realloc(syntax.nodes, (syntax.count + 1) * sizeof(ptcl_parser_syntax_node));
            if (buffer == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
                parser->position = start;
                break;
            }

            syntax.nodes = buffer;
            syntax.nodes[syntax.count++] = node;
            if (node.type == ptcl_parser_syntax_node_value_type)
            {
                if (ptcl_parser_parse_try_parse_syntax_usage(parser, syntax.nodes, syntax.count, false, start, true))
                {
                    ptcl_parser_syntax_destroy(syntax);
                    return true;
                }

                parser->position = position;
                ptcl_parser_syntax_node_destroy(node);

                ptcl_token current = ptcl_parser_current(parser);
                ptcl_parser_skip(parser);
                node = ptcl_parser_syntax_node_create_word(current.type, current.value);
                syntax.nodes[syntax.count - 1] = node;
            }
        }
        else
        {
            skip_first = false;
        }

        ptcl_parser_syntax *target = NULL;
        bool can_continue;
        bool found = ptcl_parser_syntax_try_find(parser, syntax.nodes, syntax.count, &target, &can_continue);
        if (found)
        {
            stop = parser->position;
            result = target;
        }

        if (!can_continue)
        {
            break;
        }
    }

    bool finded = result != NULL;
    if (finded)
    {
        parser->position = stop;
        ptcl_parser_parse_insert_syntax(parser, result, start, stop);
    }

    if (is_free)
    {
        ptcl_parser_syntax_destroy(syntax);
    }

    parser->position = start;
    return finded;
}

ptcl_statement_func_call ptcl_parser_parse_func_call(ptcl_parser *parser)
{
    ptcl_token name = ptcl_parser_except(parser, ptcl_token_word_type);
    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (parser->is_critical)
    {
        return (ptcl_statement_func_call){};
    }

    ptcl_statement_func_call func_call = ptcl_statement_func_call_create(ptcl_identifier_create_by_name(name.value), NULL, 0);
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_statement_func_call_destroy(func_call);
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), name.location);
            return (ptcl_statement_func_call){};
        }

        ptcl_expression *arguments = realloc(func_call.arguments, (func_call.count + 1) * sizeof(ptcl_expression));
        if (arguments == NULL)
        {
            ptcl_statement_func_call_destroy(func_call);
            return (ptcl_statement_func_call){};
        }

        func_call.arguments = arguments;
        func_call.arguments[func_call.count] = ptcl_parser_parse_binary(parser, NULL);
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

    ptcl_parser_function *target;
    if (!ptcl_parser_try_get_function(parser, name.value, func_call.arguments, func_call.count, &target))
    {
        ptcl_parser_throw_unknown_function(parser, name.value, func_call.arguments, func_call.count, name.location);
        ptcl_statement_func_call_destroy(func_call);
        return (ptcl_statement_func_call){};
    }

    func_call.return_type = target->func.return_type;
    if (target->is_built_in)
    {
        ptcl_expression *result = malloc(sizeof(ptcl_expression));
        if (result == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, name.location);
            ptcl_statement_func_call_destroy(func_call);
            return (ptcl_statement_func_call){};
        }

        *result = target->bind(parser, func_call.arguments, func_call.count, name.location);
        func_call.is_built_in = true;
        func_call.built_in = result;
    }
    else
    {
        func_call.is_built_in = false;
    }

    return func_call;
}

ptcl_statement_func_body ptcl_parser_parse_func_body(ptcl_parser *parser, bool with_brackets)
{
    ptcl_statement_func_body func_body = ptcl_statement_func_body_create(NULL, 0, parser->root);
    func_body.root = parser->root;

    ptcl_parser_parse_func_body_by_pointer(parser, &func_body, with_brackets);
    return func_body;
}

void ptcl_parser_parse_func_body_by_pointer(ptcl_parser *parser, ptcl_statement_func_body *func_body_pointer, bool with_brackets)
{
    ptcl_parser_match(parser, ptcl_token_left_curly_type);

    ptcl_statement_func_body *previous = parser->root;
    parser->root = func_body_pointer;
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

        ptcl_statement statement = ptcl_parser_parse_statement(parser);
        if (parser->is_critical)
        {
            // Already destryoed
            if (statement.type != ptcl_statement_each_type)
            {
                ptcl_statement_func_body_destroy(*func_body_pointer);
            }

            break;
        }

        if (statement.type == ptcl_statement_each_type || statement.type == ptcl_statement_syntax_type)
        {
            continue;
        }

        ptcl_statement *buffer = realloc(func_body_pointer->statements, (func_body_pointer->count + 1) * sizeof(ptcl_statement));
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

    // ptcl_parser_clear_scope(parser);
    parser->root = previous;
}

ptcl_type ptcl_parser_parse_type(ptcl_parser *parser)
{
    bool is_static = ptcl_parser_match(parser, ptcl_token_static_type);
    ptcl_token current = ptcl_parser_current(parser);
    ptcl_parser_skip(parser);

    ptcl_value_type type;
    ptcl_type target;

    switch (current.type)
    {
    case ptcl_token_character_word_type:
        target = ptcl_type_character;
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
    case ptcl_token_word_type:
        ptcl_parser_instance *typedata;
        if (ptcl_parser_try_get_instance(parser, current.value, ptcl_parser_instance_typedata_type, &typedata))
        {
            target = ptcl_type_create_typedata(current.value);
            break;
        }
    default:
        ptcl_parser_throw_unknown_type(parser, current.value, current.location);
        return (ptcl_type){};
    }

    target = ptcl_parser_pointers(parser, target);
    if (parser->is_critical)
    {
        ptcl_type_destroy(target);
        return (ptcl_type){};
    }

    if (is_static)
    {
        target.is_static = true;
    }

    return target;
}

ptcl_statement_func_decl ptcl_parser_parse_func_decl(ptcl_parser *parser, bool is_prototype)
{
    ptcl_parser_match(parser, ptcl_token_function_type);

    ptcl_token name = ptcl_parser_except(parser, ptcl_token_word_type);
    if (parser->is_critical)
    {
        return (ptcl_statement_func_decl){};
    }

    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (parser->is_critical)
    {
        return (ptcl_statement_func_decl){};
    }

    ptcl_statement_func_body func_body = ptcl_statement_func_body_create(NULL, 0, parser->root);

    // We set prototype, because on freeing we dont have body
    ptcl_statement_func_decl func_decl = ptcl_statement_func_decl_create(name.value, NULL, 0, func_body, ptcl_type_integer, true, false);
    bool is_variadic = false;

    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_statement_func_decl_destroy(func_decl);
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), name.location);
            return (ptcl_statement_func_decl){};
        }

        ptcl_argument *buffer = realloc(func_decl.arguments, (func_decl.count + 1) * sizeof(ptcl_argument));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            ptcl_statement_func_decl_destroy(func_decl);
            return (ptcl_statement_func_decl){};
        }

        if (ptcl_parser_match(parser, ptcl_token_elipsis_type))
        {
            func_decl.arguments = buffer;
            func_decl.arguments[func_decl.count++] = ptcl_argument_create_variadic();
            is_variadic = true;

            ptcl_parser_except(parser, ptcl_token_right_par_type);
            if (parser->is_critical)
            {
                ptcl_statement_func_decl_destroy(func_decl);
                return (ptcl_statement_func_decl){};
            }

            break;
        }
        else
        {
            ptcl_token name = ptcl_parser_except(parser, ptcl_token_word_type);
            if (parser->is_critical)
            {
                ptcl_statement_func_decl_destroy(func_decl);
                return (ptcl_statement_func_decl){};
            }

            ptcl_parser_except(parser, ptcl_token_colon_type);
            if (parser->is_critical)
            {
                ptcl_statement_func_decl_destroy(func_decl);
                return (ptcl_statement_func_decl){};
            }

            ptcl_type type = ptcl_parser_parse_type(parser);
            if (parser->is_critical)
            {
                ptcl_statement_func_decl_destroy(func_decl);
                return (ptcl_statement_func_decl){};
            }

            func_decl.arguments = buffer;
            func_decl.arguments[func_decl.count++] = ptcl_argument_create(type, name.value);
        }

        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    ptcl_parser_except(parser, ptcl_token_colon_type);
    if (parser->is_critical)
    {
        ptcl_statement_func_decl_destroy(func_decl);
        return (ptcl_statement_func_decl){};
    }

    ptcl_type return_type = ptcl_parser_parse_type(parser);
    if (parser->is_critical)
    {
        ptcl_statement_func_decl_destroy(func_decl);
        return (ptcl_statement_func_decl){};
    }
    else
    {
        func_decl.return_type = return_type;
    }

    func_decl.is_variadic = is_variadic;
    ptcl_parser_function *parent;
    if (parser->parent != NULL)
    {
        parent = malloc(sizeof(ptcl_parser_function));
        *parent = *parser->parent;

        if (parent == NULL)
        {
            ptcl_statement_func_decl_destroy(func_decl);
            return (ptcl_statement_func_decl){};
        }
    }
    else
    {
        parent = NULL;
    }

    ptcl_parser_instance function = ptcl_parser_function_create(parser->root, func_decl, parent);
    size_t identifier = parser->instances_count;
    if (!ptcl_parser_add_instance(parser, function))
    {
        ptcl_statement_func_decl_destroy(func_decl);
        ptcl_parser_throw_out_of_memory(parser, name.location);
        return (ptcl_statement_func_decl){};
    }

    if (!is_prototype)
    {
        ptcl_parser_function *previous = parser->parent;
        ptcl_type *previous_type = parser->return_type;
        parser->parent = &function.function;
        parser->return_type = &return_type;
        ptcl_parser_parse_func_body_by_pointer(parser, &func_decl.func_body, true);
        if (parser->is_critical)
        {
            ptcl_statement_func_decl_destroy(func_decl);
            parser->parent = previous;
            parser->return_type = previous_type;
            parser->instances_count--;
            return (ptcl_statement_func_decl){};
        }

        func_decl.is_prototype = false;

        ptcl_parser_function *original = &parser->instances[identifier].function;
        original->func = func_decl;
        parser->parent = previous;
        parser->return_type = previous_type;
    }

    return func_decl;
}

ptcl_statement_typedata_decl ptcl_parser_parse_typedata_decl(ptcl_parser *parser, bool is_prototype)
{
    ptcl_parser_match(parser, ptcl_token_typedata_type);
    ptcl_token name = ptcl_parser_except(parser, ptcl_token_word_type);
    if (parser->is_critical)
    {
        return (ptcl_statement_typedata_decl){};
    }

    ptcl_statement_typedata_decl decl = ptcl_statement_typedata_decl_create(name.value, NULL, 0, true);
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
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), name.location);
            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        ptcl_typedata_member *buffer = realloc(decl.members, (decl.count + 1) * sizeof(ptcl_typedata_member));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, name.location);
            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        decl.members = buffer;
        ptcl_token member_name = ptcl_parser_except(parser, ptcl_token_word_type);
        if (parser->is_critical)
        {
            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        ptcl_parser_except(parser, ptcl_token_colon_type);
        if (parser->is_critical)
        {
            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        ptcl_type type = ptcl_parser_parse_type(parser);
        if (parser->is_critical)
        {
            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        decl.members[decl.count] = ptcl_typedata_member_create(member_name.value, type, decl.count);
        decl.count++; // UB
    }

    ptcl_parser_instance instance = ptcl_parser_typedata_create(name.value, decl.members, decl.count);
    if (!ptcl_parser_add_instance(parser, instance))
    {
        ptcl_parser_throw_out_of_memory(parser, name.location);
        ptcl_statement_typedata_decl_destroy(decl);
        return (ptcl_statement_typedata_decl){};
    }

    return decl;
}

ptcl_statement_assign ptcl_parser_parse_assign(ptcl_parser *parser)
{
    ptcl_token name = ptcl_parser_except(parser, ptcl_token_word_type);
    if (parser->is_critical)
    {
        return (ptcl_statement_assign){};
    }

    ptcl_type type;
    bool with_type = ptcl_parser_match(parser, ptcl_token_colon_type);
    bool define = true;

    ptcl_parser_instance *created = NULL;
    if (with_type)
    {
        ptcl_parser_instance *empty = NULL;
        if (ptcl_parser_try_get_instance(parser, name.value, ptcl_parser_instance_variable_type, &empty))
        {
            ptcl_parser_throw_variable_redefination(parser, name.value, name.location);
        }

        if (ptcl_parser_current(parser).type == ptcl_token_equals_type)
        {
            with_type = false;
        }
        else
        {
            type = ptcl_parser_parse_type(parser);

            if (parser->is_critical)
            {
                return (ptcl_statement_assign){};
            }
        }
    }
    else
    {
        if (!ptcl_parser_try_get_instance(parser, name.value, ptcl_parser_instance_variable_type, &created))
        {
            ptcl_parser_throw_unknown_variable_or_type(parser, name.value, name.location);
            return (ptcl_statement_assign){};
        }

        with_type = true;
        define = false;
        type = created->variable.type;
    }

    ptcl_parser_except(parser, ptcl_token_equals_type);
    if (parser->is_critical)
    {
        if (with_type)
        {
            ptcl_type_destroy(type);
        }

        return (ptcl_statement_assign){};
    }

    ptcl_expression value = ptcl_parser_parse_binary(parser, with_type ? &type : NULL);
    if (parser->is_critical)
    {
        if (with_type)
        {
            ptcl_type_destroy(type);
        }

        return (ptcl_statement_assign){};
    }

    if (!with_type)
    {
        type = value.return_type;
    }
    else
    {
        ptcl_parser_set_arrays_length(&type, &value.return_type);
    }

    ptcl_statement_assign assign = ptcl_statement_assign_create(ptcl_identifier_create_by_name(name.value), type, with_type, value, define);
    if (created == NULL)
    {
        ptcl_parser_instance variable = ptcl_parser_variable_create(name.value, type, value, type.is_static, parser->root);
        if (!ptcl_parser_add_instance(parser, variable))
        {
            ptcl_statement_assign_destroy(assign);
            ptcl_parser_throw_out_of_memory(parser, name.location);
            return (ptcl_statement_assign){};
        }
    }

    if (created != NULL)
    {
        ptcl_expression_destroy(created->variable.built_in);
        created->variable.built_in = value;
    }

    return assign;
}

ptcl_statement_return ptcl_parser_parse_return(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_return_type);
    return ptcl_statement_return_create(ptcl_parser_parse_binary(parser, parser->return_type));
}

ptcl_statement_if ptcl_parser_parse_if(ptcl_parser *parser, bool is_static)
{
    ptcl_parser_match(parser, ptcl_token_if_type);

    ptcl_type type = ptcl_type_integer;
    type.is_static = is_static;
    ptcl_expression condition = ptcl_parser_parse_binary(parser, &type);
    if (parser->is_critical)
    {
        return (ptcl_statement_if){};
    }

    ptcl_statement_func_body body = ptcl_parser_parse_func_body(parser, true);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(condition);
        return (ptcl_statement_if){};
    }

    ptcl_statement_if if_stat = ptcl_statement_if_create(condition, body, false, (ptcl_statement_func_body){0});
    if (ptcl_parser_match(parser, ptcl_token_else_type))
    {
        if_stat.else_body = ptcl_parser_parse_func_body(parser, true);
        if_stat.with_else = !parser->is_critical;
        if (parser->is_critical)
        {
            ptcl_statement_if_destroy(if_stat);
            return (ptcl_statement_if){};
        }
    }

    return if_stat;
}

void ptcl_parser_parse_syntax(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_syntax_type);

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_parser_instance instance = ptcl_parser_syntax_create(NULL, NULL, 0);
    ptcl_parser_syntax *syntax = &instance.syntax;

    bool breaked = false;
    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_parser_throw_except_token(parser, "[end]", location);
            ptcl_parser_syntax_destroy(*syntax);
            return;
        }

        ptcl_token current = ptcl_parser_current(parser);
        ptcl_parser_skip(parser);

        ptcl_parser_syntax_node node;

        switch (current.type)
        {
        case ptcl_token_left_square_type:
            current = ptcl_parser_current(parser);
            ptcl_parser_skip(parser);

            switch (current.type)
            {
            case ptcl_token_word_type:
                if (strcmp(current.value, "end") == 0)
                {
                    ptcl_parser_except(parser, ptcl_token_right_square_type);
                    if (parser->is_critical)
                    {
                        ptcl_parser_syntax_destroy(*syntax);
                        return;
                    }

                    breaked = true;
                    break;
                }
                else if (ptcl_parser_match(parser, ptcl_token_colon_type))
                {
                    ptcl_type type = ptcl_parser_parse_type(parser);

                    ptcl_parser_except(parser, ptcl_token_right_square_type);
                    if (parser->is_critical)
                    {
                        ptcl_type_destroy(type);
                        ptcl_parser_syntax_destroy(*syntax);
                        return;
                    }

                    node = ptcl_parser_syntax_node_create_variable(type, current.value);
                    break;
                }

                parser->position--;
                break;
            }

            break;
        default:
            node = ptcl_parser_syntax_node_create_word(current.type, current.value);
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

    ptcl_parser_except(parser, ptcl_token_left_curly_type);
    if (parser->is_critical)
    {
        ptcl_parser_syntax_destroy(*syntax);
        return;
    }

    syntax->start = parser->position;
    while (!ptcl_parser_match(parser, ptcl_token_right_curly_type))
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_parser_throw_except_token(
                parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_curly_type), location);
            ptcl_parser_syntax_destroy(*syntax);
            return;
        }

        syntax->tokens_count++;
        ptcl_parser_skip(parser);
    }

    if (!ptcl_parser_add_instance(parser, instance))
    {
        ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
        ptcl_parser_syntax_destroy(*syntax);
        return;
    }
}

void ptcl_parser_parse_each(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_each_type);
    ptcl_parser_match(parser, ptcl_token_left_par_type);

    ptcl_token name = ptcl_parser_except_word(parser);
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
    ptcl_expression value = ptcl_parser_parse_binary(parser, &array);
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
    for (size_t i = 0; i < value.array.count; i++)
    {
        ptcl_parser_instance variable = ptcl_parser_variable_create(name.value, *value.return_type.target, value.array.expressions[i], true, &empty);
        if (!ptcl_parser_add_instance(parser, variable))
        {
            ptcl_parser_throw_out_of_memory(parser, name.location);
            ptcl_expression_destroy(value);
            ptcl_statement_func_body_destroy(*parser->root);
            return;
        }

        ptcl_parser_parse_func_body_by_pointer(parser, &empty, true);
        if (i != value.array.count - 1)
        {
            parser->position = position;
        }

        if (parser->is_critical)
        {
            ptcl_expression_destroy(value);
            return;
        }
    }

    size_t count = parser->root->count + empty.count;
    ptcl_statement *buffer = realloc(parser->root->statements, count * sizeof(ptcl_statement));
    if (buffer == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, name.location);
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

ptcl_expression ptcl_parser_parse_binary(ptcl_parser *parser, ptcl_type *except)
{
    ptcl_expression left = ptcl_parser_parse_additive(parser, except);
    if (parser->is_critical)
    {
        return left;
    }

    while (true)
    {
        ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);

        if (type == ptcl_binary_operator_none_type)
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
            if (type == ptcl_binary_operator_equals_type)
            {
                break;
            }
        }

        ptcl_parser_skip(parser);
        ptcl_expression right = ptcl_parser_parse_binary(parser, &left.return_type);
        ptcl_expression *children = malloc(sizeof(ptcl_expression) * 2);

        if (children == NULL)
        {
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            ptcl_parser_throw_out_of_memory(parser, left.location);
            return (ptcl_expression){};
        }

        children[0] = left;
        children[1] = right;
        if (parser->is_critical)
        {
            free(children);
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            return (ptcl_expression){};
        }

        left = ptcl_expression_binary_create(type, children, left.location);
        continue;
    }

    left = ptcl_expression_binary_static_evaluate(left, left.binary);
    if (except != NULL && !ptcl_type_equals(*except, left.return_type))
    {
        char *excepted = ptcl_type_to_string_copy(*except);
        char *received = ptcl_type_to_string_copy(left.return_type);
        ptcl_parser_throw_incorrect_type(parser, excepted, received, left.location);

        free(excepted);
        free(received);
        ptcl_expression_destroy(left);
        return (ptcl_expression){};
    }

    return left;
}

ptcl_expression ptcl_parser_parse_additive(ptcl_parser *parser, ptcl_type *except)
{
    ptcl_expression left = ptcl_parser_parse_multiplicative(parser, except);
    if (parser->is_critical)
    {
        return left;
    }

    if (left.return_type.type != ptcl_value_float_type && left.return_type.type != ptcl_value_integer_type)
    {
        return left;
    }

    while (true)
    {
        ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);

        if (type != ptcl_binary_operator_plus_type && type != ptcl_binary_operator_minus_type)
        {
            break;
        }

        ptcl_parser_skip(parser);
        ptcl_expression right = ptcl_parser_parse_multiplicative(parser, &left.return_type);

        if (!parser->is_critical && right.return_type.type != ptcl_value_float_type && right.return_type.type != ptcl_value_integer_type)
        {
            char *excepted = ptcl_type_to_string_copy(ptcl_type_integer);
            char *received = ptcl_type_to_string_copy(right.return_type);
            ptcl_parser_throw_incorrect_type(parser, excepted, received, ptcl_parser_current(parser).location);
            free(excepted);
            free(received);

            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        ptcl_expression *children = malloc(sizeof(ptcl_expression) * 2);
        if (children == NULL)
        {
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            ptcl_parser_throw_out_of_memory(parser, left.location);
            return (ptcl_expression){};
        }

        children[0] = left;
        children[1] = right;
        if (parser->is_critical)
        {
            free(children);
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            return (ptcl_expression){};
        }

        left = ptcl_expression_binary_create(type, children, left.location);
    }

    left = ptcl_expression_binary_static_evaluate(left, left.binary);
    return left;
}

ptcl_expression ptcl_parser_parse_multiplicative(ptcl_parser *parser, ptcl_type *except)
{
    ptcl_expression left = ptcl_parser_parse_unary(parser, false, except);
    if (parser->is_critical)
    {
        return left;
    }

    if (left.return_type.type != ptcl_value_float_type && left.return_type.type != ptcl_value_integer_type)
    {
        return left;
    }

    while (true)
    {
        ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);

        if (type != ptcl_binary_operator_multiplicative_type && type != ptcl_binary_operator_division_type)
        {
            break;
        }

        ptcl_parser_skip(parser);

        ptcl_expression right = ptcl_parser_parse_unary(parser, false, &left.return_type);
        if (right.return_type.type != ptcl_value_float_type && right.return_type.type != ptcl_value_integer_type)
        {
            char *excepted = ptcl_type_to_string_copy(ptcl_type_integer);
            char *received = ptcl_type_to_string_copy(right.return_type);
            ptcl_parser_throw_incorrect_type(parser, excepted, received, ptcl_parser_current(parser).location);
            free(excepted);
            free(received);

            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        ptcl_expression *children = malloc(sizeof(ptcl_expression) * 2);
        if (children == NULL)
        {
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            ptcl_parser_throw_out_of_memory(parser, left.location);
            return (ptcl_expression){};
        }

        children[0] = left;
        children[1] = right;
        if (parser->is_critical)
        {
            free(children);
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
            return (ptcl_expression){};
        }

        left = ptcl_expression_binary_create(type, children, left.location);
    }

    left = ptcl_expression_binary_static_evaluate(left, left.binary);
    return left;
}

ptcl_expression ptcl_parser_parse_unary(ptcl_parser *parser, bool only_value, ptcl_type *except)
{
    ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);

    if (type != ptcl_binary_operator_none_type)
    {
        ptcl_parser_skip(parser);

        ptcl_expression *child = malloc(sizeof(ptcl_expression));
        if (child == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            return (ptcl_expression){};
        }

        ptcl_expression value = ptcl_parser_parse_unary(parser, false, NULL);
        if (parser->is_critical)
        {
            free(child);
            return (ptcl_expression){};
        }

        ptcl_type type_value = value.return_type;

        if (type == ptcl_binary_operator_multiplicative_type)
        {
            if (only_value)
            {
                free(child);
                return value;
            }

            type = ptcl_binary_operator_dereference_type;
            type_value = *value.return_type.target;
        }
        else if (type == ptcl_binary_operator_reference_type)
        {
            type_value = (ptcl_type){
                .type = ptcl_value_pointer_type,
                .identifier = NULL,
                .target = &value.return_type,
            };
        }

        if (type == ptcl_binary_operator_reference_type || type == ptcl_binary_operator_dereference_type)
        {
            bool is_not_variable = value.type != ptcl_expression_variable_type && (value.type != ptcl_expression_unary_type && value.unary.type != ptcl_binary_operator_reference_type && value.unary.type != ptcl_binary_operator_dereference_type);
            if (is_not_variable)
            {
                ptcl_parser_throw_must_be_variable(parser, value.location);
            }
        }

        *child = value;
        ptcl_expression result = ptcl_expression_unary_create(type, child, value.location);
        return ptcl_expression_unary_static_evaluate(result, result.unary);
    }

    return ptcl_parser_parse_dot(parser, except, false);
}

ptcl_expression ptcl_parser_parse_dot(ptcl_parser *parser, ptcl_type *except, bool only_dot)
{
    ptcl_expression left = ptcl_parser_parse_array_element(parser, except);
    if (parser->is_critical)
    {
        return left;
    }

    if (left.return_type.type != ptcl_value_typedata_type)
    {
        return left;
    }

    while (true)
    {
        if (!ptcl_parser_match(parser, ptcl_token_dot_type))
        {
            break;
        }

        ptcl_token name = ptcl_parser_except(parser, ptcl_token_word_type);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        ptcl_typedata_member *member;
        if (!ptcl_parser_try_get_typedata_member(parser, left.return_type.identifier, name.value, &member))
        {
            ptcl_parser_throw_unknown_member(parser, name.value, name.location);
            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        if (left.return_type.is_static)
        {
            ptcl_expression static_value = ptcl_expression_copy(left.ctor.values[member->index], name.location);
            static_value.return_type.is_static = true;
            ptcl_expression_destroy(left);
            return static_value;
        }

        ptcl_expression *child = malloc(sizeof(ptcl_expression));
        if (child == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, left.location);
            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        *child = left;
        left = (ptcl_expression){
            .type = ptcl_expression_dot_type,
            .location = left.location,
            .dot = ptcl_expression_dot_create(child, name.value)};
        continue;
    }

    return left;
}

ptcl_expression ptcl_parser_parse_array_element(ptcl_parser *parser, ptcl_type *except)
{
    ptcl_expression value = ptcl_parser_parse_value(parser, except);
    if (parser->is_critical)
    {
        return value;
    }

    if (ptcl_parser_match(parser, ptcl_token_left_square_type))
    {
        ptcl_expression index = ptcl_parser_parse_binary(parser, &ptcl_type_integer);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(value);
            return (ptcl_expression){};
        }

        ptcl_parser_except(parser, ptcl_token_right_square_type);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(value);
            ptcl_expression_destroy(index);
            return (ptcl_expression){};
        }

        ptcl_expression *children = malloc(2 * sizeof(ptcl_expression));
        if (children == NULL)
        {
            ptcl_expression_destroy(value);
            ptcl_expression_destroy(index);
            ptcl_parser_throw_out_of_memory(parser, value.location);
            return (ptcl_expression){};
        }

        children[0] = value;
        children[1] = index;
        return ptcl_expression_array_element_create(children, index.location);
    }

    return value;
}

ptcl_expression ptcl_parser_parse_value(ptcl_parser *parser, ptcl_type *except)
{
    ptcl_token current = ptcl_parser_current(parser);
    ptcl_parser_skip(parser);

    ptcl_expression result;

    switch (current.type)
    {
    case ptcl_token_word_type:
        ptcl_parser_instance *variable;
        ptcl_parser_instance *typedata;
        if (ptcl_parser_try_get_instance(parser, current.value, ptcl_parser_instance_variable_type, &variable))
        {
            if (variable->variable.is_built_in)
            {
                result = ptcl_expression_copy(variable->variable.built_in, current.location);
                result.return_type.is_static = true;
            }
            else
            {
                result = ptcl_expression_create_variable(current.value, variable->variable.type, current.location);
            }

            break;
        }
        else if (ptcl_parser_current(parser).type == ptcl_token_left_par_type)
        {
            if (ptcl_parser_try_get_instance(parser, current.value, ptcl_parser_instance_typedata_type, &typedata))
            {
                result = (ptcl_expression){
                    .type = ptcl_expression_ctor_type,
                    .return_type = ptcl_type_create_typedata(current.value),
                    .location = current.location,
                    .ctor = ptcl_parser_parse_ctor(parser, &typedata->typedata)};
            }
            else
            {
                // Comeback to function name
                parser->position--;
                ptcl_statement_func_call func_call = ptcl_parser_parse_func_call(parser);
                if (parser->is_critical)
                {
                    break;
                }

                if (func_call.is_built_in)
                {
                    result = *func_call.built_in;
                    ptcl_statement_func_call_destroy(func_call);
                }
                else
                {
                    result = (ptcl_expression){
                        .type = ptcl_expression_func_call_type,
                        .return_type = func_call.return_type,
                        .location = current.location,
                        .func_call = func_call};
                }
            }

            break;
        }

        ptcl_parser_throw_unknown_variable(parser, current.value, current.location);
        break;
    case ptcl_token_string_type:
        size_t length = strlen(current.value);
        ptcl_expression *expressions = malloc((length + 1) * sizeof(ptcl_expression));
        if (expressions == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, current.location);
            break;
        }

        for (size_t i = 0; i < length; i++)
        {
            expressions[i] = ptcl_expression_create_character(current.value[i], current.location);
        }

        expressions[length] = ptcl_expression_create_character('\0', current.location);
        result = ptcl_expression_create_characters(expressions, length + 1, current.location);
        result.return_type.is_static = true;
        break;
    case ptcl_token_number_type:
    {
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

        if (is_float_suffix)
        {
            ptcl_parser_skip(parser);
        }

        break;
    }
    case ptcl_token_character_type:
        result = ptcl_expression_create_character(current.value[0], current.location);
        break;
    default:
        parser->position--;
        ptcl_type type = ptcl_parser_parse_type(parser);
        ptcl_parser_except(parser, ptcl_token_colon_type);
        ptcl_parser_except(parser, ptcl_token_colon_type);
        if (parser->is_critical)
        {
            return (ptcl_expression){};
        }

        ptcl_parser_except(parser, ptcl_token_left_square_type);
        if (parser->is_critical)
        {
            return (ptcl_expression){};
        }

        ptcl_type *target_type = malloc(sizeof(ptcl_type));
        if (target_type == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, current.location);
            break;
        }

        *target_type = type;
        target_type->is_primitive = false;
        ptcl_type array_type = ptcl_type_create_array(target_type, 0);
        ptcl_expression_array array = ptcl_expression_array_create_empty(array_type, current.location);
        while (!ptcl_parser_match(parser, ptcl_token_right_square_type))
        {
            if (ptcl_parser_ended(parser))
            {
                ptcl_parser_throw_except_token(
                    parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), current.location);
                ptcl_expression_array_destroy(array);
                break;
            }

            ptcl_expression *buffer = realloc(array.expressions, (array.count + 1) * sizeof(ptcl_expression));
            if (buffer == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, current.location);
                ptcl_expression_array_destroy(array);
                break;
            }

            array.expressions = buffer;
            ptcl_expression expression = ptcl_parser_parse_binary(parser, &type);
            if (parser->is_critical)
            {
                ptcl_expression_array_destroy(array);
                break;
            }

            array.expressions[array.count++] = expression;
            if (expression.return_type.type == ptcl_value_array_type && expression.return_type.count > array_type.target->count)
            {
                array_type.target->count = expression.return_type.count;
            }
        }

        array_type.count = array.count;
        return ptcl_expression_create_array(array_type, array.expressions, array.count, current.location);
    }

    if (parser->is_critical)
    {
        return result;
    }

    return result;
}

ptcl_expression_ctor ptcl_parser_parse_ctor(ptcl_parser *parser, ptcl_parser_typedata *typedata)
{
    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (parser->is_critical)
    {
        return (ptcl_expression_ctor){};
    }

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_expression_ctor ctor = ptcl_expression_ctor_create(typedata->name, NULL, 0);
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser) || (ctor.count + 1) > typedata->count)
        {
            ptcl_expression_ctor_destroy(ctor);
            ptcl_parser_throw_except_token(
                parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            return (ptcl_expression_ctor){};
        }

        ptcl_expression *buffer = realloc(ctor.values, (ctor.count + 1) * sizeof(ptcl_expression));
        if (buffer == NULL)
        {
            ptcl_expression_ctor_destroy(ctor);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_expression_ctor){};
        }

        ptcl_expression value = ptcl_parser_parse_binary(parser, &typedata->members[ctor.count].type);
        if (parser->is_critical)
        {
            ptcl_expression_ctor_destroy(ctor);
            return (ptcl_expression_ctor){};
        }

        ctor.values = buffer;
        ctor.values[ctor.count++] = value;
        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    return ctor;
}

ptcl_token ptcl_parser_except_word(ptcl_parser *parser)
{
    return ptcl_parser_except(parser, ptcl_token_word_type);
}

ptcl_token ptcl_parser_except(ptcl_parser *parser, ptcl_token_type token_type)
{
    ptcl_token current = ptcl_parser_current(parser);

    if (current.type == token_type)
    {
        ptcl_parser_skip(parser);
        return current;
    }

    ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, token_type), current.location);
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

bool ptcl_parser_add_instance(ptcl_parser *parser, ptcl_parser_instance instance)
{
    ptcl_parser_instance *buffer = realloc(parser->instances, (parser->instances_count + 1) * sizeof(ptcl_parser_instance));
    if (buffer == NULL)
    {
        return false;
    }

    parser->instances = buffer;
    parser->instances[parser->instances_count++] = instance;
    return true;
}

bool ptcl_parser_try_get_instance(ptcl_parser *parser, char *name, ptcl_parser_instance_type type, ptcl_parser_instance **instance)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *target = &parser->instances[i];
        if (target->type != type)
        {
            continue;
        }

        bool is_continue;
        switch (type)
        {
        case ptcl_parser_instance_function_type:
            if (strcmp(target->function.func.name, name) != 0)
            {
                is_continue = true;
                break;
            }

            break;
        case ptcl_parser_instance_variable_type:
            if (strcmp(target->variable.name, name) != 0)
            {
                is_continue = true;
                break;
            }

            break;
        }

        if (is_continue)
        {
            continue;
        }

        if (!ptcl_func_body_can_access(target->root, parser->root))
        {
            continue;
        }

        *instance = target;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_function(ptcl_parser *parser, char *name, ptcl_expression *arguments, size_t count, ptcl_parser_function **function)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *instance = &parser->instances[i];
        if (instance->type != ptcl_parser_instance_function_type)
        {
            continue;
        }

        ptcl_parser_function *target = &instance->function;
        if (strcmp(target->func.name, name) != 0)
        {
            continue;
        }

        if (target->func.is_variadic)
        {
            if (target->func.count - 1 > count)
            {
                break;
            }
        }
        else
        {
            if (target->func.count > count)
            {
                break;
            }
        }

        if (!ptcl_func_body_can_access(instance->root, parser->root))
        {
            continue;
        }

        bool breaked = false;
        for (size_t j = 0; j < target->func.count; j++)
        {
            ptcl_argument argument = target->func.arguments[j];
            if (argument.is_variadic)
            {
                break;
            }

            if (ptcl_type_equals(argument.type, arguments[j].return_type))
            {
                continue;
            }

            breaked = true;
            break;
        }

        if (breaked)
        {
            break;
        }

        *function = target;
        return true;
    }

    return false;
}

static bool compare_syntax_nodes(ptcl_parser_syntax_node *a, ptcl_parser_syntax_node *b)
{
    switch (a->type)
    {
    case ptcl_parser_syntax_node_word_type:
        if (b->type != ptcl_parser_syntax_node_word_type)
        {
            return false;
        }

        return strcmp(a->word.value, b->word.value) == 0;
    case ptcl_parser_syntax_node_variable_type:
        return b->type == ptcl_parser_syntax_node_value_type &&
               ptcl_type_equals(a->variable.type, b->value.return_type);
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

    for (size_t i = 0; i < parser->instances_count; i++)
    {
        ptcl_parser_instance *instance = &parser->instances[i];
        if (instance->type != ptcl_parser_instance_syntax_type)
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

    if (full_match_found)
    {
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
    }
    else
    {
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
    }

    return full_match_found;
}

bool ptcl_parser_try_get_typedata_member(ptcl_parser *parser, char *name, char *member_name, ptcl_typedata_member **member)
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
        if (strcmp(typedata_member->name, member_name) != 0)
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
    /*
    for (int i = parser->variables_count - 1; i >= 0; i--)
    {
        if (parser->variables[i].root != parser->root)
        {
            continue;
        }

        parser->variables_count--;
    }
    */
}

void ptcl_parser_throw_out_of_memory(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_out_of_memory_type, true, "Out of memory", false, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_except_token(ptcl_parser *parser, char *value, ptcl_location location)
{
    char *message = ptcl_string("Except token '", value, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_except_token_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_incorrect_type(ptcl_parser *parser, char *excepted, char *received, ptcl_location location)
{
    char *message = ptcl_string("Except '", excepted, "' type, but received '", received, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_incorrect_type_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_variable_redefination(ptcl_parser *parser, char *name, ptcl_location location)
{
    char *message = ptcl_string("Redefination variable with '", name, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_variable_redefination_type, false, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_must_be_variable(ptcl_parser *parser, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_must_be_variable_type, false, "Must be variable", false, location);
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

void ptcl_parser_throw_unknown_variable(ptcl_parser *parser, char *name, ptcl_location location)
{
    char *message = ptcl_string("Unknown variable with '", name, "' name", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_variable_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_function(ptcl_parser *parser, char *name, ptcl_expression *values, size_t count, ptcl_location location)
{
    char *message = ptcl_string("Unknown function '", name, "(", NULL);
    if (message == NULL)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        char *type = ptcl_type_to_string_copy(values[i].return_type);
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

    char *new_message = ptcl_string_append(message, ")'", NULL);
    if (new_message == NULL)
    {
        free(message);
        return;
    }

    message = new_message;
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_function_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_variable_or_type(ptcl_parser *parser, char *name, ptcl_location location)
{
    char *message = ptcl_string("Unknown variable with '", name, "' name or except type", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_variable_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_throw_unknown_type(ptcl_parser *parser, char *value, ptcl_location location)
{
    char *message = ptcl_string("Unknown type '", value, "'", NULL);
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_variable_redefination_type, true, message, true, location);
    ptcl_parser_add_error(parser, error);
}

void ptcl_parser_add_error(ptcl_parser *parser, ptcl_parser_error error)
{
    ptcl_parser_error *buffer = realloc(parser->errors, (parser->errors_count + 1) * sizeof(ptcl_parser_error));
    if (buffer == NULL)
    {
        parser->is_critical = true;
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

    if (!result.is_critical)
    {
        ptcl_statement_func_body_destroy(result.body);
    }
}

void ptcl_parser_destroy(ptcl_parser *parser)
{
    free(parser);
}