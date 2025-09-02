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
    ptcl_type *return_type;
    ptcl_parser_syntax last_syntax;
    bool is_in_syntax;
    bool is_critical;
    bool add_errors;
} ptcl_parser;

// TODO: recreate type order to base type -> pointer/array -> ...
static ptcl_token *ptcl_parser_create_tokens_from_type(ptcl_parser *parser, ptcl_type type, size_t *result_count)
{
    size_t count = 0;
    ptcl_type *target = &type;
    while (target != NULL)
    {
        count++;
        if (target->type == ptcl_value_array_type)
        {
            count++;
        }

        if (target->type == ptcl_value_pointer_type && target->pointer.target != NULL)
        {
            target = target->pointer.target;
        }
        else if (target->type == ptcl_value_array_type && target->array.target != NULL)
        {
            target = target->array.target;
        }
        else if (target->type == ptcl_value_object_type_type && target->object_type.target != NULL)
        {
            target = target->object_type.target;
        }
        else
        {
            target = NULL;
        }
    }

    ptcl_token *tokens = malloc(count * sizeof(ptcl_token));
    if (tokens == NULL)
    {
        *result_count = 0;
        return NULL;
    }

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_type *base_type = &type;
    size_t modifiers_count = 0;
    while (true)
    {
        if (base_type->type == ptcl_value_pointer_type && base_type->pointer.target != NULL)
        {
            modifiers_count++;
            base_type = base_type->pointer.target;
        }
        else if (base_type->type == ptcl_value_array_type && base_type->array.target != NULL)
        {
            modifiers_count += 2;
            base_type = base_type->array.target;
        }
        else if (base_type->type == ptcl_value_object_type_type && base_type->object_type.target != NULL)
        {
            base_type = base_type->object_type.target;
        }
        else
        {
            break;
        }
    }

    size_t index = 0;
    ptcl_token_type token_type = ptcl_value_type_to_token(base_type->type);
    char *value;
    if (base_type->type == ptcl_value_typedata_type)
    {
        value = base_type->comp_type.identifier.value;
    }
    else if (base_type->type == ptcl_value_word_type)
    {
        value = base_type->typedata.value;
    }
    else
    {
        value = ptcl_lexer_configuration_get_value(parser->configuration, token_type);
    }

    tokens[index++] = ptcl_token_create(token_type, value, location, false);
    target = &type;
    while (target != base_type)
    {
        if (target->type == ptcl_value_pointer_type)
        {
            tokens[index++] = ptcl_token_create(
                ptcl_token_asterisk_type,
                ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_asterisk_type),
                location,
                false);

            if (target->pointer.target != NULL)
            {
                target = target->pointer.target;
            }
        }
        else if (target->type == ptcl_value_array_type)
        {
            tokens[index++] = ptcl_token_create(
                ptcl_token_left_square_type,
                ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_left_square_type),
                location,
                false);
            tokens[index++] = ptcl_token_create(
                ptcl_token_right_square_type,
                ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_square_type),
                location,
                false);

            if (target->array.target != NULL)
            {
                target = target->array.target;
            }
        }
        else if (target->type == ptcl_value_object_type_type && target->object_type.target != NULL)
        {
            target = target->object_type.target;
        }

        if (target == base_type)
        {
            break;
        }
    }

    *result_count = index;
    return tokens;
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
    parser->return_type = NULL;
    parser->errors = NULL;
    parser->errors_count = 0;
    parser->instances = NULL;
    parser->instances_count = 0;
    parser->is_in_syntax = false;
    parser->add_errors = true;

    ptcl_parser_result result = {
        .configuration = parser->configuration,
        .body = ptcl_parser_parse_func_body(parser, false, true),
        .errors = parser->errors,
        .count = parser->errors_count,
        .instances = parser->instances,
        .instances_count = parser->instances_count,
        .is_critical = parser->is_critical};

    parser->input->tokens = parser->tokens;
    parser->input->count = parser->count;
    return result;
}

ptcl_statement_type ptcl_parser_parse_get_statement(ptcl_parser *parser, bool *is_finded)
{
    ptcl_token current = ptcl_parser_current(parser);
    if (current.type == ptcl_token_tilde_type)
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
    case ptcl_token_type_type:
        return ptcl_statement_type_decl_type;
    case ptcl_token_typedata_type:
        return ptcl_statement_typedata_decl_type;
    case ptcl_token_prototype_type:
    case ptcl_token_function_type:
        return ptcl_statement_func_decl_type;
    default:
        *is_finded = false;
    }
}

ptcl_statement ptcl_parser_parse_statement(ptcl_parser *parser)
{
    bool placeholder;
    if (ptcl_parser_parse_try_parse_syntax_usage_here(parser, true, NULL, &placeholder))
    {
        ptcl_statement_func_body *previous = parser->root;
        parser->is_in_syntax = false;
        parser->root = previous->root;
        ptcl_statement statement = ptcl_parser_parse_statement(parser);
        parser->root = previous;
        ptcl_parser_leave_from_syntax(parser);
        return statement;
    }

    if (parser->is_critical)
    {
        return (ptcl_statement){};
    }

    ptcl_statement statement;
    ptcl_location location = ptcl_parser_current(parser).location;
    bool is_finded;
    ptcl_statement_type type = ptcl_parser_parse_get_statement(parser, &is_finded);

    if (is_finded)
    {
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
        case ptcl_statement_type_decl_type:
            statement = (ptcl_statement){
                .type = ptcl_statement_type_decl_type,
                .type_decl = ptcl_parser_parse_type_decl(parser, ptcl_parser_match(parser, ptcl_token_prototype_type)),
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
            statement = ptcl_parser_parse_if(parser, ptcl_parser_match(parser, ptcl_token_static_type));
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
        }
    }
    else
    {
        statement.type = 0;
        ptcl_parser_throw_unknown_statement(parser, location);
    }

    if (parser->is_critical)
    {
        return statement;
    }

    statement.root = parser->root;
    return statement;
}

static void ptcl_parser_replace_words(ptcl_parser *parser, size_t stop)
{
    while (parser->position < stop)
    {
        size_t current_pos = parser->position;
        ptcl_token current = parser->tokens[current_pos];
        if (current.type != ptcl_token_exclamation_mark_type)
        {
            ptcl_parser_skip(parser);
            continue;
        }

        bool succesful;
        ptcl_name name = ptcl_parser_parse_name(parser, true, &succesful);
        if (parser->is_critical)
        {
            return;
        }

        if (!succesful)
        {
            continue;
        }

        size_t tokens_processed = parser->position - current_pos;
        for (size_t i = current_pos; i < current_pos + tokens_processed; i++)
        {
            ptcl_token_destroy(parser->tokens[i]);
        }

        if (name.is_name)
        {
            parser->tokens[current_pos] = ptcl_token_create(
                ptcl_token_word_type,
                name.word.value,
                current.location,
                name.word.is_free);
            if (tokens_processed > 1)
            {
                size_t shift_count = parser->count - (current_pos + tokens_processed);
                if (shift_count > 0)
                {
                    memmove(&parser->tokens[current_pos + 1],
                            &parser->tokens[current_pos + tokens_processed],
                            shift_count * sizeof(ptcl_token));
                }

                parser->count -= tokens_processed - 1;
                stop -= tokens_processed - 1;
            }

            parser->position = current_pos + 1;
        }
        else
        {
            size_t new_tokens_count = name.tokens.count;
            ptrdiff_t size_diff = new_tokens_count - tokens_processed;

            if (size_diff != 0)
            {
                size_t tokens_to_preserve = parser->count - (current_pos + tokens_processed);
                ptcl_token *preserved_tokens = NULL;

                if (tokens_to_preserve > 0)
                {
                    preserved_tokens = malloc(tokens_to_preserve * sizeof(ptcl_token));
                    if (preserved_tokens == NULL)
                    {
                        parser->is_critical = true;
                        for (size_t i = 0; i < name.tokens.count; i++)
                        {
                            ptcl_token_destroy(name.tokens.tokens[i]);
                        }
                        free(name.tokens.tokens);
                        ptcl_parser_throw_out_of_memory(parser, current.location);
                        return;
                    }

                    memcpy(preserved_tokens,
                           &parser->tokens[current_pos + tokens_processed],
                           tokens_to_preserve * sizeof(ptcl_token));
                }

                size_t new_count = parser->count + size_diff;
                ptcl_token *new_tokens = realloc(parser->tokens, new_count * sizeof(ptcl_token));
                if (new_tokens == NULL)
                {
                    parser->is_critical = true;
                    for (size_t i = 0; i < name.tokens.count; i++)
                    {
                        ptcl_token_destroy(name.tokens.tokens[i]);
                    }

                    free(name.tokens.tokens);
                    free(preserved_tokens);
                    ptcl_parser_throw_out_of_memory(parser, current.location);
                    return;
                }

                parser->tokens = new_tokens;
                if (tokens_to_preserve > 0)
                {
                    memmove(&new_tokens[current_pos + new_tokens_count],
                            preserved_tokens,
                            tokens_to_preserve * sizeof(ptcl_token));
                    free(preserved_tokens);
                }

                parser->count = new_count;
                stop += size_diff;
            }

            for (size_t i = 0; i < new_tokens_count; i++)
            {
                parser->tokens[current_pos + i] = name.tokens.tokens[i];
            }

            free(name.tokens.tokens);
            parser->position = current_pos + new_tokens_count;
        }

        ptcl_parser_skip(parser);
    }
}

static bool ptcl_parser_insert_syntax(
    ptcl_parser *parser,
    ptcl_parser_syntax *syntax,
    size_t start_position,
    size_t end_position)
{
    const size_t number_of_tokens_to_replace = end_position - start_position;
    const int size_difference = number_of_tokens_to_replace - syntax->tokens_count;
    for (size_t token_index = start_position; token_index < end_position; token_index++)
    {
        ptcl_token_destroy(parser->tokens[token_index]);
    }

    if (size_difference < 0)
    {
        const size_t additional_space_needed = -size_difference;
        ptcl_token *new_token_buffer = realloc(parser->tokens,
                                               (parser->count + additional_space_needed) * sizeof(ptcl_token));

        if (new_token_buffer == NULL)
        {
            return false;
        }

        parser->tokens = new_token_buffer;
        parser->count += additional_space_needed;
        for (size_t token_index = parser->count - 1; token_index >= start_position; token_index--)
        {
            parser->tokens[token_index] = parser->tokens[token_index - additional_space_needed];
        }

        for (size_t insert_index = start_position;
             insert_index < start_position + additional_space_needed + number_of_tokens_to_replace;
             insert_index++)
        {
            parser->tokens[insert_index] = ptcl_token_same(
                parser->tokens[syntax->start + (insert_index - start_position)]);
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
        for (size_t copy_index = 0; copy_index < syntax->tokens_count; copy_index++)
        {
            parser->tokens[start_position + copy_index] =
                ptcl_token_same(parser->tokens[syntax->start + copy_index]);
        }
    }
    else
    {
        for (size_t replace_index = start_position; replace_index < end_position; replace_index++)
        {
            parser->tokens[replace_index] = ptcl_token_same(
                parser->tokens[syntax->start + (replace_index - start_position)]);
        }
    }

    return true;
}

bool ptcl_parser_parse_try_parse_syntax_usage_here(ptcl_parser *parser, bool is_statement,
                                                   ptcl_expression *old_expression, bool *with_expression)
{
    return ptcl_parser_parse_try_parse_syntax_usage(parser, NULL, NULL, 0, true, -1, false, is_statement, old_expression, with_expression);
}

bool ptcl_parser_parse_try_parse_syntax_usage(ptcl_parser *parser,
                                              ptcl_parser_syntax_node *nodes, ptcl_parser_syntax_node **reallocated, size_t count,
                                              bool is_free, int down_start, bool skip_first, bool is_statement,
                                              ptcl_expression *old_expression,
                                              bool *with_expression)
{
    size_t start = down_start >= 0 ? down_start : parser->position;
    size_t stop = parser->position;
    ptcl_parser_instance instance = ptcl_parser_syntax_create(NULL, nodes, count);
    ptcl_parser_syntax syntax = instance.syntax;
    ptcl_parser_syntax result;
    bool found = false;
    size_t expr_position = 0;
    *with_expression = false;

    while (true)
    {
        if (ptcl_parser_ended(parser))
        {
            break;
        }

        if (!skip_first)
        {
            ptcl_parser_syntax_node node;
            const size_t position = parser->position;
            const bool last = parser->add_errors;
            parser->add_errors = false;
            ptcl_expression value = ptcl_parser_parse_value(parser, NULL, true, is_statement);
            parser->add_errors = last;
            if (parser->is_critical)
            {
                parser->position = position;
                ptcl_token current = ptcl_parser_current(parser);
                ptcl_parser_skip(parser);
                node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(current.value, false, current.location));
                parser->is_critical = false;
            }
            else
            {
                node = ptcl_parser_syntax_node_create_value(value);
                if (!*with_expression)
                {
                    *with_expression = !is_statement && value.return_type.type != ptcl_value_word_type && syntax.count == 0;
                    if (*with_expression)
                    {
                        expr_position = parser->position;
                        *old_expression = value;
                    }
                }
            }

            ptcl_parser_syntax_node *buffer = realloc(syntax.nodes, (syntax.count + 1) * sizeof(ptcl_parser_syntax_node));
            if (buffer == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
                ptcl_parser_syntax_destroy(syntax);
                parser->position = start;
                break;
            }

            syntax.nodes = buffer;
            syntax.nodes[syntax.count++] = node;
            if (node.type == ptcl_parser_syntax_node_value_type)
            {
                bool placeholder;
                ptcl_parser_syntax_node *reallocated_value;
                if (ptcl_parser_parse_try_parse_syntax_usage(parser, syntax.nodes, &reallocated_value, syntax.count, false, start, true, true, NULL, &placeholder))
                {
                    return true;
                }

                syntax.nodes = reallocated_value;
                if (parser->is_critical)
                {
                    return false;
                }

                parser->position = position;
                if (!*with_expression)
                {
                    ptcl_parser_syntax_node_destroy(node);
                }

                ptcl_token current = ptcl_parser_current(parser);
                ptcl_parser_skip(parser);
                node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(current.value, false, current.location));
                syntax.nodes[syntax.count - 1] = node;
            }
        }
        else
        {
            skip_first = false;
        }

        ptcl_parser_syntax *target = NULL;
        bool can_continue;
        bool current_found = ptcl_parser_syntax_try_find(parser, syntax.nodes, syntax.count, &target, &can_continue);
        if (current_found)
        {
            stop = parser->position;
            // This syntax in 'parser->instances' array and we do syntax insert below we also do rellocation and the pointer to this array break
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
        parser->position = stop;
        ptcl_statement_func_body *temp = malloc(sizeof(ptcl_statement_func_body));
        *temp = ptcl_statement_func_body_create(NULL, 0, parser->root);
        parser->root = temp;

        for (size_t i = 0; i < syntax.count; i++)
        {
            ptcl_parser_syntax_node syntax_node = result.nodes[i];
            if (syntax_node.type != ptcl_parser_syntax_node_variable_type)
            {
                continue;
            }

            ptcl_expression expression = syntax.nodes[i].value;
            ptcl_parser_instance variable = ptcl_parser_variable_create(
                ptcl_name_create_fast_w(syntax_node.variable.name, false), syntax_node.variable.type, expression, expression.return_type.is_static, parser->root);
            if (expression.return_type.is_static && expression.return_type.type == ptcl_value_word_type)
            {
                // After insert it will be destroyed
                variable.variable.built_in.word.value = ptcl_string_duplicate(variable.variable.built_in.word.value);
                variable.variable.is_syntax_word = true;
            }

            if (!ptcl_parser_add_instance(parser, variable))
            {
                ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
                ptcl_parser_syntax_destroy(syntax);
                return false;
            }
        }

        ptcl_parser_insert_syntax(parser, &result, start, stop);
        if (*with_expression)
        {
            *with_expression = false;
            ptcl_expression_destroy(*old_expression);
        }

        parser->position = start;
        ptcl_parser_replace_words(parser, start + result.tokens_count);
        parser->last_syntax = syntax;
        parser->is_in_syntax = true;
        parser->position = start;
    }
    else
    {
        if (*with_expression)
        {
            parser->position = expr_position;
        }
        else
        {
            parser->position = start;
        }

        if (is_free)
        {
            ptcl_parser_syntax_destroy(syntax);
        }
        else
        {
            *reallocated = syntax.nodes;
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
    parser->is_in_syntax = false;
    free(temp);
    ptcl_parser_syntax_destroy(parser->last_syntax);
}

ptcl_statement_func_call ptcl_parser_parse_func_call(ptcl_parser *parser)
{
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name_word name = ptcl_parser_parse_name_word(parser);
    ptcl_parser_except(parser, ptcl_token_left_par_type);
    if (parser->is_critical)
    {
        return (ptcl_statement_func_call){};
    }

    ptcl_parser_instance *instance_target;
    ptcl_parser_function *target;
    if (!ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_function_type, &instance_target))
    {
        ptcl_parser_throw_unknown_function(parser, name.value, location);
        return (ptcl_statement_func_call){};
    }

    target = &instance_target->function;
    ptcl_statement_func_call func_call = ptcl_statement_func_call_create(ptcl_identifier_create_by_name(name), NULL, 0);
    bool is_variadic_now = false;
    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_statement_func_call_destroy(func_call);
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            return (ptcl_statement_func_call){};
        }

        ptcl_expression *arguments = realloc(func_call.arguments, (func_call.count + 1) * sizeof(ptcl_expression));
        if (arguments == NULL)
        {
            ptcl_statement_func_call_destroy(func_call);
            return (ptcl_statement_func_call){};
        }

        func_call.arguments = arguments;
        ptcl_argument *argument = func_call.count < target->func.count ? &target->func.arguments[func_call.count] : NULL;
        ptcl_type *excepted = NULL;
        if (argument != NULL)
        {
            excepted = &argument->type;
            is_variadic_now = argument->is_variadic;
        }

        func_call.arguments[func_call.count] = ptcl_parser_parse_binary(parser, excepted, false, true);
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
    if (target->func.is_variadic)
    {
        argument_count_mismatch = func_call.count < target->func.count - 1;
    }
    else
    {
        argument_count_mismatch = func_call.count != target->func.count;
    }

    if (argument_count_mismatch)
    {
        ptcl_parser_throw_wrong_arguments(parser, name.value, func_call.arguments,
                                          func_call.count, location);
        ptcl_statement_func_call_destroy(func_call);
        return (ptcl_statement_func_call){};
    }

    func_call.return_type = target->func.return_type;
    if (target->is_built_in)
    {
        ptcl_expression *result = malloc(sizeof(ptcl_expression));
        if (result == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_statement_func_call_destroy(func_call);
            return (ptcl_statement_func_call){};
        }

        *result = target->bind(parser, func_call.arguments, func_call.count, location);
        func_call.is_built_in = true;
        func_call.built_in = result;
    }
    else
    {
        func_call.is_built_in = false;
    }

    return func_call;
}

ptcl_statement_func_body ptcl_parser_parse_func_body(ptcl_parser *parser, bool with_brackets, bool change_root)
{
    ptcl_statement_func_body func_body = ptcl_statement_func_body_create(NULL, 0, parser->root);
    func_body.root = parser->root;

    ptcl_parser_parse_func_body_by_pointer(parser, &func_body, with_brackets, change_root);
    return func_body;
}

void ptcl_parser_parse_func_body_by_pointer(ptcl_parser *parser, ptcl_statement_func_body *func_body_pointer, bool with_brackets, bool change_root)
{
    ptcl_parser_match(parser, ptcl_token_left_curly_type);

    ptcl_statement_func_body *previous = parser->root;
    if (change_root)
    {
        parser->root = func_body_pointer;
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
    if (change_root)
    {
        parser->root = previous;
    }
}

void ptcl_parser_parse_extra_body(ptcl_parser *parser, bool is_syntax)
{
    if (!ptcl_parser_match(parser, ptcl_token_at_type))
    {
        return;
    }

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_statement_func_body body = ptcl_statement_func_body_create(
        NULL, 0,
        is_syntax ? parser->root->root : parser->root);
    ptcl_parser_parse_func_body_by_pointer(parser, &body, true, false);
    if (parser->is_critical)
    {
        ptcl_statement_func_body_destroy(body);
        return;
    }

    ptcl_statement_func_body *previous = parser->root;
    parser->root = body.root;

    ptcl_statement statement = ptcl_statement_func_body_create_stat(body, location);
    size_t new_count = parser->root->count + 1;
    ptcl_statement *new_statements = realloc(parser->root->statements,
                                             sizeof(ptcl_statement) * new_count);
    if (new_statements == NULL)
    {
        ptcl_statement_destroy(statement);
        ptcl_parser_throw_out_of_memory(parser, location);
        parser->root = previous;
        return;
    }

    size_t insert_position = parser->root->count;
    if (parser->root->count > 0)
    {
        insert_position = parser->root->count - 1;
        for (size_t i = parser->root->count; i > insert_position; i--)
        {
            new_statements[i] = new_statements[i - 1];
        }
    }

    new_statements[insert_position] = statement;
    parser->root->statements = new_statements;
    parser->root->count = new_count;
    parser->root = previous;
    ptcl_parser_parse_extra_body(parser, is_syntax);
}

ptcl_type ptcl_parser_parse_type(ptcl_parser *parser, bool with_word, bool with_any)
{
    bool placeholder;
    bool with_syntax = ptcl_parser_parse_try_parse_syntax_usage_here(parser, false, NULL, &placeholder);

    size_t position = parser->position;
    size_t original_statements_count = parser->root->count;
    size_t original_instances_count = parser->instances_count;
    ptcl_parser_parse_extra_body(parser, with_syntax);
    if (parser->is_critical)
    {
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
    case ptcl_token_any_type:
        if (with_any)
        {
            target = ptcl_type_any;
            break;
        }
    case ptcl_token_exclamation_mark_type:
        ptcl_parser_instance *typedata;
        parser->position--;
        ptcl_name_word name = ptcl_parser_parse_name_word(parser);
        if (parser->is_critical)
        {
            break;
        }

        if (ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_typedata_type, &typedata))
        {
            target = ptcl_type_create_typedata(typedata->typedata.name.value, typedata->typedata.name.is_anonymous);
            ptcl_name_word_destroy(name);
            break;
        }

        ptcl_name_word_destroy(name);
    case ptcl_token_word_type:
        if (ptcl_parser_try_get_instance(parser, ptcl_name_create_fast_w(current.value, false), ptcl_parser_instance_typedata_type, &typedata))
        {
            target = ptcl_type_create_typedata(current.value, false);
            break;
        }
        // Fallthrough
    default:
        parse_success = false;
        ptcl_parser_throw_unknown_type(parser, current.value, current.location);
        break;
    }

    if (with_syntax)
    {
        ptcl_parser_clear_scope(parser);

        ptcl_statement_func_body *temp = parser->root;
        ptcl_statement_func_body_destroy(*temp);
        parser->root = temp->root;
        free(temp);
        ptcl_parser_syntax_destroy(parser->last_syntax);
    }

    if (!parse_success)
    {
        parser->position = position;
        if (parser->root->count > original_statements_count)
        {
            for (size_t i = original_statements_count; i < parser->root->count; i++)
            {
                ptcl_statement_destroy(parser->root->statements[i]);
            }

            parser->root->count = original_statements_count;
        }

        if (parser->instances_count > original_instances_count)
        {
            for (size_t i = original_instances_count; i < parser->instances_count; i++)
            {
                ptcl_parser_instance_destroy(&parser->instances[i]);
            }

            parser->instances_count = original_instances_count;
        }

        return (ptcl_type){};
    }

    if (target.type != ptcl_value_word_type)
    {
        ptcl_type parsed_target = ptcl_parser_pointers(parser, target);
        if (parser->is_critical)
        {
            parser->position = position;
            if (parser->root->count > original_statements_count)
            {
                for (size_t i = original_statements_count; i < parser->root->count; i++)
                {
                    ptcl_statement_destroy(parser->root->statements[i]);
                }

                parser->root->count = original_statements_count;
            }

            if (parser->instances_count > original_instances_count)
            {
                for (size_t i = original_instances_count; i < parser->instances_count; i++)
                {
                    ptcl_parser_instance_destroy(&parser->instances[i]);
                }

                parser->instances_count = original_instances_count;
            }

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

    if (is_static)
    {
        target.is_static = true;
    }

    return target;
}

ptcl_statement_func_decl ptcl_parser_parse_func_decl(ptcl_parser *parser, bool is_prototype)
{
    ptcl_parser_match(parser, ptcl_token_function_type);

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name_word name = ptcl_parser_parse_name_word(parser);
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
    ptcl_statement_func_decl func_decl = ptcl_statement_func_decl_create(name, NULL, 0, func_body, ptcl_type_integer, true, false);
    bool is_variadic = false;

    while (!ptcl_parser_match(parser, ptcl_token_right_par_type))
    {
        if (ptcl_parser_ended(parser))
        {
            ptcl_statement_func_decl_destroy(func_decl);
            ptcl_parser_throw_except_token(parser, ptcl_lexer_configuration_get_value(parser->configuration, ptcl_token_right_par_type), location);
            return (ptcl_statement_func_decl){};
        }

        ptcl_argument *buffer = realloc(func_decl.arguments, (func_decl.count + 1) * sizeof(ptcl_argument));
        if (buffer == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, location);
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
            ptcl_name_word argument_name = ptcl_parser_parse_name_word(parser);
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

            ptcl_type type = ptcl_parser_parse_type(parser, false, false);
            if (parser->is_critical)
            {
                ptcl_statement_func_decl_destroy(func_decl);
                return (ptcl_statement_func_decl){};
            }

            func_decl.arguments = buffer;
            func_decl.arguments[func_decl.count++] = ptcl_argument_create(type, argument_name);
        }

        ptcl_parser_match(parser, ptcl_token_comma_type);
    }

    ptcl_parser_except(parser, ptcl_token_colon_type);
    if (parser->is_critical)
    {
        ptcl_statement_func_decl_destroy(func_decl);
        return (ptcl_statement_func_decl){};
    }

    ptcl_type return_type = ptcl_parser_parse_type(parser, false, false);
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
    ptcl_parser_instance function = ptcl_parser_function_create(parser->root, func_decl);
    size_t identifier = parser->instances_count;
    if (!ptcl_parser_add_instance(parser, function))
    {
        ptcl_statement_func_decl_destroy(func_decl);
        ptcl_parser_throw_out_of_memory(parser, location);
        return (ptcl_statement_func_decl){};
    }

    if (!is_prototype)
    {
        ptcl_statement_func_body *pointer = &func_decl.func_body;
        ptcl_type *previous_type = parser->return_type;
        parser->return_type = &return_type;
        for (size_t i = 0; i < func_decl.count; i++)
        {
            ptcl_argument argument = func_decl.arguments[i];
            ptcl_parser_instance variable = ptcl_parser_variable_create(argument.name, argument.type, (ptcl_expression){}, false, pointer);
            if (!ptcl_parser_add_instance(parser, variable))
            {
                ptcl_statement_func_decl_destroy(func_decl);
                ptcl_parser_throw_out_of_memory(parser, location);
                return (ptcl_statement_func_decl){};
            }
        }

        ptcl_parser_parse_func_body_by_pointer(parser, pointer, true, true);
        if (parser->is_critical)
        {
            ptcl_statement_func_decl_destroy(func_decl);
            parser->return_type = previous_type;
            parser->instances_count--;
            return (ptcl_statement_func_decl){};
        }

        func_decl.is_prototype = false;

        ptcl_parser_function *original = &parser->instances[identifier].function;
        original->func = func_decl;
        parser->return_type = previous_type;
    }

    return func_decl;
}

ptcl_statement_typedata_decl ptcl_parser_parse_typedata_decl(ptcl_parser *parser, bool is_prototype)
{
    ptcl_parser_match(parser, ptcl_token_typedata_type);

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name_word name = ptcl_parser_parse_name_word(parser);
    if (parser->is_critical)
    {
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
        ptcl_name_word member_name = ptcl_parser_parse_name_word(parser);
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

        ptcl_type type = ptcl_parser_parse_type(parser, false, false);
        if (parser->is_critical)
        {
            ptcl_statement_typedata_decl_destroy(decl);
            return (ptcl_statement_typedata_decl){};
        }

        decl.members[decl.count] = ptcl_typedata_member_create(member_name, type, decl.count);
        decl.count++; // UB
    }

    ptcl_parser_instance instance = ptcl_parser_typedata_create(name, decl.members, decl.count);
    if (!ptcl_parser_add_instance(parser, instance))
    {
        ptcl_parser_throw_out_of_memory(parser, location);
        ptcl_statement_typedata_decl_destroy(decl);
        return (ptcl_statement_typedata_decl){};
    }

    return decl;
}

ptcl_statement_type_decl ptcl_parser_parse_type_decl(ptcl_parser *parser, bool is_prototype)
{
    ptcl_parser_match(parser, ptcl_token_type_type);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name_word name = ptcl_parser_parse_name_word(parser);
    if (parser->is_critical)
    {
        return (ptcl_statement_type_decl){};
    }

    ptcl_parser_except(parser, ptcl_token_colon_type);
    if (parser->is_critical)
    {
        ptcl_name_word_destroy(name);
        return (ptcl_statement_type_decl){};
    }

    ptcl_statement_type_decl decl = ptcl_statement_type_decl_create(name, NULL, 0, NULL, 0, is_prototype);
    while (true)
    {
        ptcl_type *buffer = realloc(decl.types, (decl.types_count + 1) * sizeof(ptcl_type));
        if (buffer == NULL)
        {
            ptcl_statement_type_decl_destroy(decl);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_type_decl){};
        }

        ptcl_type type = ptcl_parser_parse_type(parser, false, false);
        if (parser->is_critical)
        {
            ptcl_statement_type_decl_destroy(decl);
            return (ptcl_statement_type_decl){};
        }

        if (decl.types_count > 0)
        {
            ptcl_type except = decl.types[decl.types_count - 1];
            if (!ptcl_type_equals(except, type))
            {
                ptcl_parser_throw_fast_incorrect_type(parser, except, type, location);
                ptcl_statement_type_decl_destroy(decl);
                return (ptcl_statement_type_decl){};
            }
        }

        decl.types = buffer;
        decl.types[decl.types_count++] = type;
        if (!ptcl_parser_match(parser, ptcl_token_comma_type))
        {
            break;
        }
    }

    return decl;
}

ptcl_statement_assign ptcl_parser_parse_assign(ptcl_parser *parser)
{
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name_word name = ptcl_parser_parse_name_word(parser);
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
        if (ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_variable_type, &empty))
        {
            ptcl_parser_throw_variable_redefination(parser, name.value, location);
        }

        if (ptcl_parser_current(parser).type == ptcl_token_equals_type)
        {
            with_type = false;
        }
        else
        {
            type = ptcl_parser_parse_type(parser, true, false);
            if (parser->is_critical)
            {
                return (ptcl_statement_assign){};
            }
        }
    }
    else
    {
        if (!ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_variable_type, &created))
        {
            ptcl_parser_throw_unknown_variable_or_type(parser, name.value, location);
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

    ptcl_expression value = ptcl_parser_parse_binary(parser, with_type ? &type : NULL, false, true);
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
        if (define)
        {
            type.is_static = false;
        }
    }
    else
    {
        ptcl_parser_set_arrays_length(&type, &value.return_type);
    }

    ptcl_statement_assign assign = ptcl_statement_assign_create(ptcl_identifier_create_by_name(name), type, with_type, value, define);
    if (created == NULL)
    {
        ptcl_parser_instance variable = ptcl_parser_variable_create(name, type, value, type.is_static, parser->root);
        if (!ptcl_parser_add_instance(parser, variable))
        {
            ptcl_statement_assign_destroy(assign);
            ptcl_parser_throw_out_of_memory(parser, location);
            return (ptcl_statement_assign){};
        }
    }
    else
    {

        created->variable.built_in = value;
        ptcl_expression_destroy(created->variable.built_in);
    }

    return assign;
}

ptcl_statement_return ptcl_parser_parse_return(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_return_type);
    return ptcl_statement_return_create(ptcl_parser_parse_binary(parser, parser->return_type, false, true));
}

ptcl_statement ptcl_parser_parse_if(ptcl_parser *parser, bool is_static)
{
    ptcl_parser_match(parser, ptcl_token_if_type);

    ptcl_type type = ptcl_type_integer;
    type.is_static = is_static;
    ptcl_expression condition = ptcl_parser_parse_binary(parser, &type, false, true);
    if (parser->is_critical)
    {
        return (ptcl_statement){};
    }

    if (is_static)
    {
        ptcl_statement_func_body result;
        if (condition.integer_n)
        {
            result = ptcl_parser_parse_func_body(parser, true, false);
            if (parser->is_critical)
            {
                return (ptcl_statement){};
            }

            if (ptcl_parser_match(parser, ptcl_token_else_type))
            {
                ptcl_parser_except(parser, ptcl_token_left_curly_type);
                if (parser->is_critical)
                {
                    ptcl_statement_func_body_destroy(result);
                    return (ptcl_statement){};
                }

                ptcl_parser_skip_block_or_expression(parser);
                if (parser->is_critical)
                {
                    ptcl_statement_func_body_destroy(result);
                    return (ptcl_statement){};
                }
            }
        }
        else
        {
            ptcl_parser_skip_block_or_expression(parser);
            if (parser->is_critical)
            {
                return (ptcl_statement){};
            }

            if (ptcl_parser_match(parser, ptcl_token_else_type))
            {
                result = ptcl_parser_parse_func_body(parser, true, false);
                if (parser->is_critical)
                {
                    return (ptcl_statement){};
                }
            }
            else
            {
                result = ptcl_statement_func_body_create(NULL, 0, parser->root);
            }
        }

        return ptcl_statement_func_body_create_stat(result, condition.location);
    }

    ptcl_statement_func_body body = ptcl_parser_parse_func_body(parser, true, true);
    if (parser->is_critical)
    {
        ptcl_expression_destroy(condition);
        return (ptcl_statement){};
    }

    ptcl_statement_if if_stat = ptcl_statement_if_create(condition, body, false, (ptcl_statement_func_body){0});
    if (ptcl_parser_match(parser, ptcl_token_else_type))
    {
        if_stat.else_body = ptcl_parser_parse_func_body(parser, true, true);
        if_stat.with_else = !parser->is_critical;
        if (parser->is_critical)
        {
            ptcl_statement_if_destroy(if_stat);
            return (ptcl_statement){};
        }
    }

    return (ptcl_statement){
        .type = ptcl_statement_if_type,
        .location = condition.location,
        .if_stat = if_stat};
}

ptcl_expression ptcl_parser_parse_if_expression(ptcl_parser *parser, bool is_static)
{
    ptcl_parser_match(parser, ptcl_token_if_type);
    ptcl_type type = ptcl_type_integer;
    type.is_static = is_static;
    ptcl_expression condition = ptcl_parser_parse_binary(parser, &type, false, true);
    if (parser->is_critical)
    {
        return (ptcl_expression){};
    }

    ptcl_parser_except(parser, ptcl_token_left_curly_type);
    if (parser->is_critical)
    {
        return (ptcl_expression){};
    }

    if (is_static)
    {
        ptcl_expression result;

        if (condition.integer_n)
        {
            result = ptcl_parser_parse_binary(parser, NULL, false, true);
            if (parser->is_critical)
            {
                return (ptcl_expression){};
            }

            ptcl_parser_except(parser, ptcl_token_right_curly_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return (ptcl_expression){};
            }

            ptcl_parser_except(parser, ptcl_token_else_type);
            ptcl_parser_except(parser, ptcl_token_left_curly_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return (ptcl_expression){};
            }

            ptcl_parser_skip_block_or_expression(parser);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return (ptcl_expression){};
            }
        }
        else
        {
            ptcl_parser_skip_block_or_expression(parser);
            if (parser->is_critical)
            {
                return (ptcl_expression){};
            }

            ptcl_parser_except(parser, ptcl_token_else_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return (ptcl_expression){};
            }

            ptcl_parser_except(parser, ptcl_token_left_curly_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return (ptcl_expression){};
            }

            result = ptcl_parser_parse_binary(parser, NULL, false, true);
            if (parser->is_critical)
            {
                return (ptcl_expression){};
            }

            ptcl_parser_except(parser, ptcl_token_right_curly_type);
            if (parser->is_critical)
            {
                ptcl_expression_destroy(result);
                return (ptcl_expression){};
            }
        }

        return result;
    }

    ptcl_expression *children = malloc(3 * sizeof(ptcl_expression));
    if (children == NULL)
    {
        ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
        return (ptcl_expression){};
    }

    ptcl_expression body = ptcl_parser_parse_binary(parser, NULL, false, true);
    if (parser->is_critical)
    {
        free(children);
        ptcl_expression_destroy(condition);
        return (ptcl_expression){};
    }

    ptcl_parser_except(parser, ptcl_token_else_type);
    if (parser->is_critical)
    {
        free(children);
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return (ptcl_expression){};
    }

    ptcl_expression_if if_expr = ptcl_expression_if_create(children);
    if_expr.children[0] = condition;
    if_expr.children[1] = body;
    if_expr.children[2] = ptcl_parser_parse_binary(parser, &body.return_type, false, true);
    if (parser->is_critical)
    {
        free(children);
        ptcl_expression_destroy(condition);
        ptcl_expression_destroy(body);
        return (ptcl_expression){};
    }

    return (ptcl_expression){
        .type = ptcl_expression_if_type,
        .location = condition.location,
        .return_type = if_expr.children[0].return_type,
        .if_expr = if_expr};
}

void ptcl_parser_parse_syntax(ptcl_parser *parser)
{
    ptcl_parser_match(parser, ptcl_token_syntax_type);
    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_parser_instance instance = ptcl_parser_syntax_create(NULL, NULL, 0);
    ptcl_parser_syntax *syntax = &instance.syntax;

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
        if (strcmp(current.value, "end") == 0)
        {
            break;
        }

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
                    ptcl_type type = ptcl_parser_parse_type(parser, true, true);
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

                parser->position--;
                break;
            case ptcl_token_exclamation_mark_type:
                size_t start = parser->position--; // comeback to '!'
                ptcl_name_word word = ptcl_parser_parse_name_word(parser);
                if (parser->is_critical)
                {
                    ptcl_parser_syntax_destroy(*syntax);
                    return;
                }

                if (ptcl_parser_try_get_instance(parser, word, ptcl_parser_instance_variable_type, &variable) && variable->variable.type.type == ptcl_expression_object_type_type)
                {
                    node = ptcl_parser_syntax_node_create_object_type(*ptcl_type_get_target(variable->variable.type));
                    ptcl_name_word_destroy(word);
                    break;
                }

                ptcl_name_word_destroy(word);
                parser->position = start;
                break;
            default:
                parser->position--; // comeback to start
                ptcl_expression expression = ptcl_parser_parse_binary(parser, NULL, false, true);
                if (parser->is_critical)
                {
                    ptcl_parser_syntax_destroy(*syntax);
                    return;
                }

                ptcl_parser_except(parser, ptcl_token_right_square_type);
                if (parser->is_critical)
                {
                    ptcl_expression_destroy(expression);
                    ptcl_parser_syntax_destroy(*syntax);
                    return;
                }

                node = ptcl_parser_syntax_node_create_object_type(expression.return_type);
                expression.type = ptcl_expression_variable_type; // Avoid type destroying
                ptcl_expression_destroy(expression);
                break;
            }

            break;
        default:
            node = ptcl_parser_syntax_node_create_word(current.type, ptcl_name_create(current.value, false, current.location));
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
    ptcl_parser_skip_block_or_expression(parser);
    if (parser->is_critical)
    {
        ptcl_parser_syntax_destroy(*syntax);
        return;
    }

    // -1 because of curly
    syntax->tokens_count = (parser->position - syntax->start) - 1;
    parser->position = syntax->start;
    ptcl_parser_replace_words(parser, (syntax->start + syntax->tokens_count) + 1);
    size_t old_count = syntax->tokens_count;
    syntax->tokens_count = (parser->position - syntax->start) - 1;
    if (parser->is_critical)
    {
        ptcl_parser_syntax_destroy(*syntax);
        return;
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

    ptcl_location location = ptcl_parser_current(parser).location;
    ptcl_name_word name = ptcl_parser_parse_name_word(parser);
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
    ptcl_expression value = ptcl_parser_parse_binary(parser, &array, false, true);
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
        ptcl_parser_instance variable = ptcl_parser_variable_create(name, *ptcl_type_get_target(value.return_type), value.array.expressions[i], true, &empty);
        if (!ptcl_parser_add_instance(parser, variable))
        {
            ptcl_parser_throw_out_of_memory(parser, location);
            ptcl_expression_destroy(value);
            ptcl_statement_func_body_destroy(*parser->root);
            return;
        }

        ptcl_parser_parse_func_body_by_pointer(parser, &empty, true, true);
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

ptcl_expression ptcl_parser_parse_binary(ptcl_parser *parser, ptcl_type *except, bool with_word, bool with_syntax)
{
    ptcl_expression syntax_expression;
    bool with_expression = false;
    if (with_syntax && ptcl_parser_parse_try_parse_syntax_usage_here(parser, false, &syntax_expression, &with_expression))
    {
        ptcl_expression expression = ptcl_parser_parse_binary(parser, except, with_word, true);
        ptcl_parser_leave_from_syntax(parser);
        return expression;
    }

    ptcl_expression left;
    if (with_expression)
    {
        left = syntax_expression;
    }
    else
    {
        left = ptcl_parser_parse_additive(parser, except, with_word, false);
        if (parser->is_critical || left.return_type.type == ptcl_value_word_type)
        {
            return left;
        }
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
            if (type == ptcl_binary_operator_equals_type || type == ptcl_binary_operator_negation_type)
            {
                break;
            }
        }

        ptcl_parser_skip(parser);
        ptcl_expression right = ptcl_parser_parse_binary(parser, &left.return_type, false, true);
        if (parser->is_critical)
        {
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
    if (except != NULL && !ptcl_type_equals(*except, left.return_type))
    {
        ptcl_parser_throw_fast_incorrect_type(parser, *except, left.return_type, left.location);
        ptcl_expression_destroy(left);
        return (ptcl_expression){};
    }

    return left;
}

ptcl_expression ptcl_parser_parse_additive(ptcl_parser *parser, ptcl_type *except, bool with_word, bool with_syntax)
{
    ptcl_expression left = ptcl_parser_parse_multiplicative(parser, except, with_word, with_syntax);
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
        ptcl_expression right = ptcl_parser_parse_multiplicative(parser, &left.return_type, false, true);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        if (right.return_type.type != ptcl_value_float_type && right.return_type.type != ptcl_value_integer_type)
        {
            ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_integer, right.return_type, left.location);
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

ptcl_expression ptcl_parser_parse_multiplicative(ptcl_parser *parser, ptcl_type *except, bool with_word, bool with_syntax)
{
    ptcl_expression left = ptcl_parser_parse_unary(parser, false, except, with_word, with_syntax);
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
        ptcl_expression right = ptcl_parser_parse_unary(parser, false, &left.return_type, false, true);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        if (right.return_type.type != ptcl_value_float_type && right.return_type.type != ptcl_value_integer_type)
        {
            ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_integer, right.return_type, left.location);
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

ptcl_expression ptcl_parser_parse_unary(ptcl_parser *parser, bool only_value, ptcl_type *except, bool with_word, bool with_syntax)
{
    ptcl_binary_operator_type type = ptcl_binary_operator_type_from_token(ptcl_parser_current(parser).type);

    if (type != ptcl_binary_operator_none_type && type != ptcl_binary_operator_equals_type && type != ptcl_binary_operator_greater_than_type && type != ptcl_binary_operator_less_than_type && type != ptcl_binary_operator_greater_equals_than_type && type != ptcl_binary_operator_less_equals_than_type)
    {
        ptcl_parser_skip(parser);
        ptcl_expression *child = malloc(sizeof(ptcl_expression));
        if (child == NULL)
        {
            ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
            return (ptcl_expression){};
        }

        ptcl_expression value = ptcl_parser_parse_unary(parser, false, NULL, false, false);
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
            type_value = *ptcl_type_get_target(value.return_type);
        }
        else if (type == ptcl_binary_operator_reference_type)
        {
            ptcl_type *target = malloc(sizeof(ptcl_type));
            if (target == NULL)
            {
                free(child);
                ptcl_expression_destroy(value);
                ptcl_parser_throw_out_of_memory(parser, ptcl_parser_current(parser).location);
                return (ptcl_expression){};
            }

            *target = value.return_type;
            target->is_primitive = false;
            type_value = ptcl_type_create_pointer(target);
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
        result.return_type = type_value;
        return ptcl_expression_unary_static_evaluate(result, result.unary);
    }

    return ptcl_parser_parse_dot(parser, except, false, with_word, false);
}

ptcl_expression ptcl_parser_parse_dot(ptcl_parser *parser, ptcl_type *except, bool only_dot, bool with_word, bool with_syntax)
{
    ptcl_expression left = ptcl_parser_parse_array_element(parser, except, with_word, with_syntax);
    if (parser->is_critical || left.return_type.type != ptcl_value_typedata_type)
    {
        return left;
    }

    while (true)
    {
        if (!ptcl_parser_match(parser, ptcl_token_dot_type))
        {
            break;
        }

        ptcl_location location = ptcl_parser_current(parser).location;
        ptcl_name_word name = ptcl_parser_parse_name_word(parser);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        ptcl_typedata_member *member;
        if (!ptcl_parser_try_get_typedata_member(parser, left.return_type.typedata, name.value, &member))
        {
            ptcl_parser_throw_unknown_member(parser, name.value, location);
            ptcl_expression_destroy(left);
            return (ptcl_expression){};
        }

        if (left.return_type.is_static)
        {
            ptcl_expression static_value = ptcl_expression_copy(left.ctor.values[member->index], location);
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
            .dot = ptcl_expression_dot_create(child, name)};
    }

    return left;
}

ptcl_expression ptcl_parser_parse_array_element(ptcl_parser *parser, ptcl_type *except, bool with_word, bool with_syntax)
{
    ptcl_expression value = ptcl_parser_parse_value(parser, except, with_word, with_syntax);
    if (parser->is_critical || (value.return_type.type != ptcl_value_array_type && value.return_type.type != ptcl_value_pointer_type))
    {
        return value;
    }

    if (ptcl_parser_match(parser, ptcl_token_left_square_type))
    {
        ptcl_expression index = ptcl_parser_parse_binary(parser, &ptcl_type_integer, false, with_syntax);
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

ptcl_expression ptcl_parser_parse_value(ptcl_parser *parser, ptcl_type *except, bool with_word, bool with_syntax)
{
    const bool is_anonymous = ptcl_parser_match(parser, ptcl_token_tilde_type);
    ptcl_token current = ptcl_parser_current(parser);
    if (except != NULL && except->type == ptcl_value_word_type)
    {
        ptcl_name_word word_value = ptcl_parser_parse_name_word(parser);
        if (parser->is_critical)
        {
            return (ptcl_expression){};
        }

        ptcl_expression word = ptcl_expression_word_create(word_value, current.location);
        word.return_type.is_static = true;
        return word;
    }

    ptcl_parser_skip(parser);
    ptcl_expression result;

    switch (current.type)
    {
    case ptcl_token_at_type:
        parser->position--;
        ptcl_parser_parse_extra_body(parser, parser->is_in_syntax);
        if (!parser->is_critical)
        {
            return ptcl_parser_parse_binary(parser, except, with_word, true);
        }
    case ptcl_token_word_type:
        ptcl_parser_instance *variable;
        ptcl_parser_instance *typedata;
        ptcl_name_word word_name = ptcl_name_create_fast_w(current.value, is_anonymous);
        if (ptcl_parser_try_get_instance(parser, word_name, ptcl_parser_instance_variable_type, &variable))
        {
            if (variable->variable.is_built_in)
            {
                result = ptcl_expression_copy(variable->variable.built_in, current.location);
                result.return_type.is_static = true;
            }
            else
            {
                result = ptcl_expression_create_variable(word_name, variable->variable.type, current.location);
            }

            break;
        }
        else if (ptcl_parser_current(parser).type == ptcl_token_left_par_type)
        {
            if (ptcl_parser_try_get_instance(parser, word_name, ptcl_parser_instance_typedata_type, &typedata))
            {
                result = (ptcl_expression){
                    .type = ptcl_expression_ctor_type,
                    .return_type = ptcl_type_create_typedata(word_name.value, word_name.is_anonymous),
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

        if (with_word)
        {
            ptcl_expression word = ptcl_expression_word_create(ptcl_name_create(current.value, false, current.location).word, current.location);
            word.return_type.is_static = true;
            return word;
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
    case ptcl_token_static_type:
        ptcl_parser_except(parser, ptcl_token_if_type);
        if (parser->is_critical)
        {
            break;
        }
    case ptcl_token_if_type:
        bool is_static = current.type == ptcl_token_static_type;
        result = ptcl_parser_parse_if_expression(parser, is_static);
        break;
    case ptcl_token_left_par_type:
        ptcl_expression inside = ptcl_parser_parse_binary(parser, except, with_word, true);
        if (parser->is_critical)
        {
            return (ptcl_expression){};
        }

        ptcl_parser_except(parser, ptcl_token_right_par_type);
        if (parser->is_critical)
        {
            ptcl_expression_destroy(inside);
        }

        result = inside;
        break;
    case ptcl_token_new_type:
        parser->position--;
        ptcl_type type = ptcl_parser_parse_type(parser, false, false);
        if (parser->is_critical)
        {
            return (ptcl_expression){};
        }

        ptcl_parser_except(parser, ptcl_token_colon_type);
        if (parser->is_critical)
        {
            return (ptcl_expression){};
        }

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
            ptcl_expression expression = ptcl_parser_parse_binary(parser, &type, false, true);
            if (parser->is_critical)
            {
                ptcl_expression_array_destroy(array);
                break;
            }

            array.expressions[array.count++] = expression;
            if (expression.return_type.type == ptcl_value_array_type && expression.return_type.array.count > target_type->array.count)
            {
                target_type->array.count = expression.return_type.array.count;
            }
        }

        target_type->array.count = array.count;
        result = ptcl_expression_create_array(array_type, array.expressions, array.count, current.location);
        break;
    case ptcl_token_greater_than_type:
        ptcl_type object_type = ptcl_parser_parse_type(parser, false, false);
        if (parser->is_critical)
        {
            return (ptcl_expression){};
        }

        ptcl_type *target = malloc(sizeof(ptcl_type));
        if (target == NULL)
        {
            ptcl_type_destroy(object_type);
            ptcl_parser_throw_out_of_memory(parser, current.location);
            break;
        }

        *target = object_type;
        target->is_primitive = false;
        result = ptcl_expression_create_object_type(ptcl_type_create_object_type(target), object_type, current.location);
        result.return_type.is_static = true;
        break;
    default:
        ptcl_parser_throw_unknown_expression(parser, current.location);
        return (ptcl_expression){};
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

        ptcl_expression value = ptcl_parser_parse_binary(parser, &typedata->members[ctor.count].type, false, true);
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

ptcl_name_word ptcl_parser_parse_name_word(ptcl_parser *parser)
{
    bool succesful;
    ptcl_name name = ptcl_parser_parse_name(parser, false, &succesful);
    if (!succesful)
    {
        ptcl_parser_throw_except_token(parser, "variable, function or type", ptcl_parser_current(parser).location);
    }

    return name.word;
}

ptcl_name ptcl_parser_parse_name(ptcl_parser *parser, bool with_tokens, bool *succesful)
{
    *succesful = true;
    if (ptcl_parser_match(parser, ptcl_token_exclamation_mark_type))
    {
        const bool is_anonymous = ptcl_parser_match(parser, ptcl_token_tilde_type);
        ptcl_token current = ptcl_parser_current(parser);
        ptcl_parser_skip(parser);
        switch (current.type)
        {
        case ptcl_token_less_than_type:
            bool left_succesful;
            ptcl_name left = ptcl_parser_parse_name(parser, false, &left_succesful);
            if (parser->is_critical)
            {
                return (ptcl_name){};
            }

            bool right_succesful;
            ptcl_name right = ptcl_parser_parse_name(parser, false, &right_succesful);
            if (parser->is_critical)
            {
                return (ptcl_name){};
            }

            *succesful = left_succesful && right_succesful;
            ptcl_parser_except(parser, ptcl_token_greater_than_type);
            if (parser->is_critical)
            {
            destroy: // goto...
                if (left_succesful)
                {
                    ptcl_name_destroy(left);
                }

                if (right_succesful)
                {
                    ptcl_name_destroy(right);
                }
                return (ptcl_name){};
            }

            if (!left_succesful || !right_succesful)
            {
                goto destroy;
            }

            char *concatenated = ptcl_string(left.word.value, right.word.value, NULL);
            if (concatenated == NULL)
            {
                ptcl_parser_throw_out_of_memory(parser, left.location);
                goto destroy;
            }

            ptcl_name_destroy(left);
            ptcl_name_destroy(right);
            if (with_tokens)
            {
                ptcl_token *tokens = malloc(sizeof(ptcl_token));
                if (tokens == NULL)
                {
                    free(concatenated);
                    ptcl_parser_throw_out_of_memory(parser, current.location);
                    return (ptcl_name){};
                }

                tokens[0] = ptcl_token_create(ptcl_token_word_type, concatenated, current.location, true);
                return ptcl_name_create_tokens(tokens, 1, current.location);
            }

            return ptcl_name_create(concatenated, true, left.location);
        }

        ptcl_parser_instance *instance;
        ptcl_name_word name = ptcl_name_create_fast_w(current.value, is_anonymous);
        bool found = ptcl_parser_try_get_instance(parser, name, ptcl_parser_instance_variable_type, &instance);
        if (!found)
        {
            *succesful = false;
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
            case ptcl_value_word_type:
                ptcl_name_word result = instance->variable.built_in.word;
                return ptcl_name_create(result.value, result.is_free, current.location);
            case ptcl_value_object_type_type:
                if (!with_tokens)
                {
                    incorrect_type = true;
                    break;
                }

                size_t count;
                ptcl_token *tokens = ptcl_parser_create_tokens_from_type(parser, instance->variable.built_in.object_type.type, &count);
                if (tokens == NULL)
                {
                    ptcl_parser_throw_out_of_memory(parser, current.location);
                    return (ptcl_name){};
                }

                return ptcl_name_create_tokens(tokens, count, current.location);
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
    else if (ptcl_parser_match(parser, ptcl_token_word_word_type))
    {
        ptcl_location location = ptcl_parser_current(parser).location;
        ptcl_parser_except(parser, ptcl_token_left_par_type);
        if (parser->is_critical)
        {
            return (ptcl_name){};
        }

        ptcl_name_word target = ptcl_parser_parse_name_word(parser);
        ptcl_parser_except(parser, ptcl_token_right_par_type);
        if (parser->is_critical)
        {
            ptcl_name_word_destroy(target);
            return (ptcl_name){};
        }

        ptcl_parser_instance *variable;
        if (!ptcl_parser_try_get_instance(parser, target, ptcl_parser_instance_variable_type, &variable))
        {
            *succesful = false;
            ptcl_name_word_destroy(target);
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
                value = ptcl_type_to_word_copy(variable->variable.built_in.object_type.type);
                break;
            case ptcl_value_word_type:
                value = variable->variable.built_in.word.value;
                break;
            default:
                incorrect_type = true;
                break;
            }
        }

        if (incorrect_type)
        {
            ptcl_parser_throw_fast_incorrect_type(parser, ptcl_type_word, variable->variable.type, location);
            ptcl_name_word_destroy(target);
            return (ptcl_name){};
        }

        ptcl_name_word_destroy(target);
        return ptcl_name_create(value, is_free, location);
    }

    const bool is_anonymous = ptcl_parser_match(parser, ptcl_token_tilde_type);
    ptcl_token token = ptcl_parser_except(parser, ptcl_token_word_type);
    if (parser->is_critical)
    {
        *succesful = false;
        return (ptcl_name){};
    }

    return is_anonymous ? ptcl_anonymous_name_create(token.value, false, token.location) : ptcl_name_create(token.value, false, token.location);
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

bool ptcl_parser_try_get_instance(ptcl_parser *parser, ptcl_name_word name, ptcl_parser_instance_type type, ptcl_parser_instance **instance)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *target = &parser->instances[i];
        if (target->type != type || target->is_out_of_scope)
        {
            continue;
        }

        ptcl_name_word target_name;
        switch (type)
        {
        case ptcl_parser_instance_typedata_type:
            target_name = target->typedata.name;
            break;
        case ptcl_parser_instance_function_type:
            target_name = target->function.func.name;
            break;
        case ptcl_parser_instance_variable_type:
            target_name = target->variable.name;
            break;
        }

        if (!ptcl_name_word_compare(target_name, name) || !ptcl_func_body_can_access(target->root, parser->root))
        {
            continue;
        }

        *instance = target;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_instance_in_root(ptcl_parser *parser, ptcl_name_word name, ptcl_parser_instance_type type, ptcl_parser_instance **instance)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *target = &parser->instances[i];
        if (target->type != type || target->is_out_of_scope)
        {
            continue;
        }

        ptcl_name_word target_name;
        switch (type)
        {
        case ptcl_parser_instance_typedata_type:
            target_name = target->typedata.name;
            break;
        case ptcl_parser_instance_function_type:
            target_name = target->function.func.name;
            break;
        case ptcl_parser_instance_variable_type:
            target_name = target->variable.name;
            break;
        }

        if (target->root != parser->root || !ptcl_name_word_compare(target_name, name))
        {
            continue;
        }

        *instance = target;
        return true;
    }

    return false;
}

bool ptcl_parser_try_get_function(ptcl_parser *parser, ptcl_name_word name, ptcl_parser_function **function)
{
    for (int i = parser->instances_count - 1; i >= 0; i--)
    {
        ptcl_parser_instance *instance = &parser->instances[i];
        if (instance->type != ptcl_parser_instance_function_type)
        {
            continue;
        }

        ptcl_parser_function *target = &instance->function;
        if (!ptcl_name_word_compare(target->func.name, name) || !ptcl_func_body_can_access(instance->root, parser->root))
        {
            continue;
        }

        *function = target;
        return true;
    }

    return false;
}

bool ptcl_parser_check_arguments(ptcl_parser *parser, ptcl_parser_function *function, ptcl_expression *arguments, size_t count)
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

        if (ptcl_type_equals(argument.type, arguments[j].return_type))
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

        return strcmp(left->word.name.word.value, right->word.name.word.value) == 0;
    case ptcl_parser_syntax_node_variable_type:
        return right->type == ptcl_parser_syntax_node_value_type &&
               ptcl_type_equals(left->variable.type, right->value.return_type);
    case ptcl_parser_syntax_node_object_type_type:
        if (right->type != ptcl_parser_syntax_node_value_type)
        {
            return false;
        }

        return ptcl_type_equals(left->object_type, right->value.return_type);
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

bool ptcl_parser_try_get_typedata_member(ptcl_parser *parser, ptcl_name_word name, char *member_name, ptcl_typedata_member **member)
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

void ptcl_parser_throw_fast_incorrect_type(ptcl_parser *parser, ptcl_type excepted, ptcl_type received, ptcl_location location)
{
    char *excepted_message = ptcl_type_to_present_string_copy(excepted);
    char *received_message = ptcl_type_to_present_string_copy(received);
    ptcl_parser_throw_incorrect_type(parser, excepted_message, received_message, location);

    free(excepted_message);
    free(received_message);
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

void ptcl_parser_throw_wrong_arguments(ptcl_parser *parser, char *name, ptcl_expression *values, size_t count, ptcl_location location)
{
    char *message = ptcl_string("Wrong arguments '", name, "(", NULL);
    if (message == NULL)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        char *type = ptcl_type_to_present_string_copy(values[i].return_type);
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

    char *new_message = ptcl_string_append(message, ")', except (", NULL);
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

void ptcl_parser_throw_unknown_function(ptcl_parser *parser, char *name, ptcl_location location)
{
    ptcl_parser_error error = ptcl_parser_error_create(
        ptcl_parser_error_unknown_function_type, true, ptcl_string("Unknown function '", name, "(", NULL), true, location);
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

    if (!result.is_critical)
    {
        ptcl_statement_func_body_destroy(result.body);
    }
}

void ptcl_parser_destroy(ptcl_parser *parser)
{
    free(parser);
}
