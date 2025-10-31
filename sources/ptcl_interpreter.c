#include <ptcl_interpreter.h>

typedef struct ptcl_interpreter_variable
{
    ptcl_name name;
    ptcl_expression *value;
    bool is_destroy;
} ptcl_interpreter_variable;

typedef struct ptcl_interpreter_var_index
{
    ptcl_interpreter_variable variable;
    size_t index;
} ptcl_interpreter_var_index;

typedef struct ptcl_interpreter
{
    ptcl_parser *parser;
    ptcl_name stack_trace[PTCL_INTERPRETER_MAX_STACK_TRACE];
    size_t stack_trace_count;
    ptcl_interpreter_variable *variables;
    size_t variables_count;
    size_t variables_capacity;
    bool is_busy;
} ptcl_interpreter;

ptcl_interpreter *ptcl_interpreter_create(ptcl_parser *parser)
{
    ptcl_interpreter *interpreter = malloc(sizeof(ptcl_interpreter));
    if (interpreter == NULL)
    {
        return NULL;
    }

    interpreter->parser = parser;
    interpreter->variables_capacity = PTCL_PARSER_DEFAULT_INSTANCE_CAPACITY;
    interpreter->variables = malloc(interpreter->variables_capacity * sizeof(ptcl_interpreter_variable));
    if (interpreter->variables == NULL)
    {
        free(interpreter);
        return NULL;
    }

    interpreter->stack_trace_count = 0;
    interpreter->is_busy = false;
    return interpreter;
}

ptcl_expression *ptcl_interpreter_evaluate_func_body(ptcl_interpreter *interpreter, ptcl_statement_func_body func_body, ptcl_location location)
{
    ptcl_expression *result = NULL;
    for (size_t i = 0; i < func_body.count; i++)
    {
        result = ptcl_interpreter_evaluate_statement(interpreter, func_body.statements[i], location);
        if (ptcl_parser_critical(interpreter->parser))
        {
            ptcl_interpreter_reset(interpreter);
            break;
        }

        if (result != NULL)
        {
            break;
        }
    }

    return result;
}

ptcl_expression *ptcl_interpreter_evaluate_statement(ptcl_interpreter *interpreter, ptcl_statement *statement, ptcl_location location)
{
    switch (statement->type)
    {
    case ptcl_statement_func_body_type:
    case ptcl_statement_func_call_type:
        return ptcl_interpreter_evaluate_function_call(interpreter, statement->func_call, true, location);
    case ptcl_statement_assign_type:
    case ptcl_statement_if_type:
        ptcl_expression *condition = ptcl_interpreter_evaluate_expression(interpreter, statement->if_stat.condition, location);
        if (condition == NULL)
        {
            break;
        }

        int condition_value = condition->integer_n;
        free(condition);
        if (condition_value)
        {
            return ptcl_interpreter_evaluate_func_body(interpreter, statement->if_stat.body, location);
        }
        else
        {
            if (statement->if_stat.with_else)
            {
                return ptcl_interpreter_evaluate_func_body(interpreter, statement->if_stat.else_body, location);
            }
        }

        break;
    case ptcl_statement_return_type:
        return ptcl_interpreter_evaluate_expression(interpreter, statement->ret.value, location);
    case ptcl_statement_import_type:
    case ptcl_statement_undefine_type:
    case ptcl_statement_unsyntax_type:
    case ptcl_statement_syntax_type:
    case ptcl_statement_each_type:
    case ptcl_statement_func_decl_type:
    case ptcl_statement_type_decl_type:
    case ptcl_statement_typedata_decl_type:
        break;
    }

    return NULL;
}

ptcl_expression *ptcl_interpreter_evaluate_expression(ptcl_interpreter *interpreter, ptcl_expression *expression, ptcl_location location)
{
    ptcl_expression *result;
    switch (expression->type)
    {
    case ptcl_expression_lated_func_body_type:
    case ptcl_expression_in_statement_type:
    case ptcl_expression_in_expression_type:
    case ptcl_expression_in_token_type:
    case ptcl_expression_array_type:
    case ptcl_expression_null_type:
    case ptcl_expression_object_type_type:
    case ptcl_expression_word_type:
    case ptcl_expression_func_call_type:
    {
        result = ptcl_interpreter_evaluate_function_call(interpreter, expression->func_call, true, location);
        if (result == NULL)
        {
            return NULL;
        }

        break;
    }
    case ptcl_expression_if_type:
    case ptcl_expression_ctor_type:
    case ptcl_expression_dot_type:
    case ptcl_expression_array_element_type:
    case ptcl_expression_cast_type:
    case ptcl_expression_binary_type:
        ptcl_binary_operator_type binary_type = expression->binary.type;
        if (binary_type == ptcl_binary_operator_reference_type || binary_type == ptcl_binary_operator_dereference_type)
        {
            return NULL;
        }

        ptcl_expression *left = ptcl_interpreter_evaluate_expression(interpreter, expression->binary.left, location);
        if (left == NULL)
        {
            return NULL;
        }

        ptcl_expression *right = ptcl_interpreter_evaluate_expression(interpreter, expression->binary.right, location);
        if (right == NULL)
        {
            ptcl_expression_destroy(left);
            return NULL;
        }

        result = ptcl_expression_binary_static_evaluate(expression->binary.type, left, right);
        break;
    case ptcl_expression_unary_type:
        ptcl_binary_operator_type unary_type = expression->binary.type;
        if (unary_type == ptcl_binary_operator_reference_type || unary_type == ptcl_binary_operator_dereference_type)
        {
            return NULL;
        }

        ptcl_expression *unary_value = ptcl_interpreter_evaluate_expression(interpreter, expression->unary.child, location);
        if (unary_value == NULL)
        {
            return NULL;
        }

        result = ptcl_expression_unary_static_evaluate(expression->unary.type, unary_value);
        break;
    case ptcl_expression_variable_type:
        result = ptcl_interpreter_get_value(interpreter, expression->variable.name);
        if (result == NULL)
        {
            return NULL;
        }

        result = ptcl_interpreter_evaluate_expression(interpreter, result, location);
        break;
    case ptcl_expression_character_type:
        if (!expression->return_type.is_static)
        {
            return NULL;
        }

        result = ptcl_expression_create_character(expression->character, location);
        break;
    case ptcl_expression_double_type:
        if (!expression->return_type.is_static)
        {
            return NULL;
        }

        result = ptcl_expression_create_double(expression->double_n, location);
        break;
    case ptcl_expression_float_type:
        if (!expression->return_type.is_static)
        {
            return NULL;
        }

        result = ptcl_expression_create_float(expression->float_n, location);
        break;
    case ptcl_expression_integer_type:
        if (!expression->return_type.is_static)
        {
            return NULL;
        }

        result = ptcl_expression_create_integer(expression->integer_n, location);
        break;
    }

    if (result == NULL)
    {
        ptcl_parser_throw_out_of_memory(interpreter->parser, location);
    }

    return result;
}

ptcl_expression *ptcl_interpreter_evaluate_function_call(ptcl_interpreter *interpreter, ptcl_statement_func_call func_call, bool evaluate_arguments, ptcl_location location)
{
    if (func_call.identifier.is_name && strcmp(func_call.identifier.name.value, PTCL_PARSER_ERROR_FUNC_NAME) == 0)
    {
        ptcl_parser_throw_user(interpreter->parser, ptcl_string_from_array(func_call.arguments[0]->array), location);
        return NULL;
    }

    if (func_call.func_decl->is_prototype || func_call.func_decl->func_body == NULL)
    {
        return NULL;
    }

    const bool is_root = !interpreter->is_busy;
    if (is_root)
    {
        interpreter->is_busy = true;
        size_t count = ptcl_parser_variables_count(interpreter->parser);
        ptcl_parser_variable *variables = ptcl_parser_variables(interpreter->parser);
        for (size_t i = 0; i < count; i++)
        {
            ptcl_parser_variable variable = variables[i];
            if (variable.is_out_of_scope || !variable.is_built_in || !ptcl_func_body_can_access(variable.root, ptcl_parser_root(interpreter->parser)))
            {
                continue;
            }

            if (!ptcl_interpreter_add_variable(interpreter, variable.name, variable.built_in, false))
            {
                ptcl_parser_throw_out_of_memory(interpreter->parser, location);
                return NULL;
            }
        }
    }

    if (ptcl_interpreter_was_called(interpreter, func_call.func_decl->name))
    {
        ptcl_interpreter_var_index arguments_array[PTCL_INTERPRETER_DEFAULT_ARGUMENTS_CAPACITY];
        ptcl_interpreter_var_index *arguments = arguments_array;
        bool needs_free = (func_call.func_decl->count >= PTCL_INTERPRETER_MAX_ARGUMENTS);
        if (needs_free)
        {
            arguments = malloc(func_call.func_decl->count * sizeof(ptcl_interpreter_var_index));
            if (arguments == NULL)
            {
                ptcl_parser_throw_out_of_memory(interpreter->parser, location);
                return NULL;
            }
        }

        for (size_t i = 0; i < func_call.func_decl->count; i++)
        {
            for (int j = interpreter->variables_count - 1; j >= 0; j--)
            {
                ptcl_interpreter_variable *variable = &interpreter->variables[i];
                if (!ptcl_name_compare(variable->name, func_call.func_decl->arguments[i].name))
                {
                    continue;
                }

                ptcl_interpreter_var_index *pair = &arguments[i];
                pair->variable = *variable;
                pair->index = j;
                ptcl_expression *value = evaluate_arguments
                                             ? ptcl_interpreter_evaluate_expression(interpreter, func_call.arguments[i], location)
                                             : func_call.arguments[i];
                variable->value = value;
                variable->is_destroy = evaluate_arguments;
            }
        }

        ptcl_expression *result = ptcl_interpreter_evaluate_func_body(interpreter, *func_call.func_decl->func_body, location);
        for (size_t i = 0; i < func_call.func_decl->count; i++)
        {
            ptcl_interpreter_var_index pair = arguments[i];
            ptcl_interpreter_variable *variable = &interpreter->variables[pair.index];
            if (variable->is_destroy)
            {
                ptcl_expression_destroy(variable->value);
            }

            *variable = pair.variable;
        }

        if (is_root)
        {
            interpreter->is_busy = false;
            interpreter->stack_trace_count = 0;
        }

        if (needs_free)
        {
            free(arguments);
        }

        return result;
    }

    interpreter->stack_trace[interpreter->stack_trace_count++] = func_call.func_decl->name;
    size_t variables_count = interpreter->variables_count;
    for (size_t i = 0; i < func_call.count; i++)
    {
        ptcl_argument argument = func_call.func_decl->arguments[i];
        if (argument.is_variadic)
        {
            continue;
        }

        ptcl_expression *value = evaluate_arguments
                                     ? ptcl_interpreter_evaluate_expression(interpreter, func_call.arguments[i], location)
                                     : func_call.arguments[i];
        if (!ptcl_interpreter_add_variable(interpreter, argument.name, value, evaluate_arguments))
        {
            ptcl_parser_throw_out_of_memory(interpreter->parser, location);
            return NULL;
        }
    }

    ptcl_expression *result = ptcl_interpreter_evaluate_func_body(interpreter, *func_call.func_decl->func_body, location);
    for (size_t i = variables_count; i < interpreter->variables_count; i++)
    {
        ptcl_interpreter_variable variable = interpreter->variables[i];
        if (variable.is_destroy)
        {
            ptcl_expression_destroy(variable.value);
        }
    }

    interpreter->variables_count = variables_count;
    if (is_root)
    {
        interpreter->is_busy = false;
        interpreter->stack_trace_count = 0;
    }

    return result;
}

ptcl_expression *ptcl_interpreter_get_value(ptcl_interpreter *interpreter, ptcl_name name)
{
    for (int i = interpreter->variables_count - 1; i >= 0; i--)
    {
        ptcl_interpreter_variable variable = interpreter->variables[i];
        if (!ptcl_name_compare(variable.name, name))
        {
            continue;
        }

        return variable.value;
    }

    return NULL;
}

bool ptcl_interpreter_add_variable(ptcl_interpreter *interpreter, ptcl_name name, ptcl_expression *value, bool is_destroy)
{
    if (interpreter->variables_count >= interpreter->variables_capacity)
    {
        interpreter->variables_capacity *= 2;
        ptcl_interpreter_variable *buffer = realloc(interpreter->variables, interpreter->variables_capacity * sizeof(ptcl_interpreter_variable));
        if (buffer == NULL)
        {
            interpreter->variables_capacity /= 2;
            return false;
        }

        interpreter->variables = buffer;
    }

    interpreter->variables[interpreter->variables_count++] = (ptcl_interpreter_variable){
        .name = name,
        .value = value,
        .is_destroy = is_destroy};
    return true;
}

bool ptcl_interpreter_was_called(ptcl_interpreter *interpreter, ptcl_name name)
{
    for (size_t i = 0; i < interpreter->stack_trace_count; i++)
    {
        if (!ptcl_name_compare(interpreter->stack_trace[i], name))
        {
            continue;
        }

        return true;
    }

    return false;
}

void ptcl_interpreter_reset(ptcl_interpreter *interpreter)
{
    interpreter->variables_count = 0;
}

void ptcl_interpreter_destroy(ptcl_interpreter *interpreter)
{
    free(interpreter->variables);
    free(interpreter);
}
