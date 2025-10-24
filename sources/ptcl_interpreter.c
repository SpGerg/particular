#include <ptcl_interpreter.h>

typedef struct ptcl_interpreter_variable
{
    ptcl_name name;
    ptcl_expression *value;
} ptcl_interpreter_variable;

typedef struct ptcl_interpreter
{
    ptcl_parser *parser;
    ptcl_interpreter_variable *variables;
    size_t variables_count;
    size_t variables_capacity;
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
        ptcl_interpreter_evaluate_function_call(interpreter, statement->func_call, location);
        break;
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
        ptcl_expression *arguments_array[PTCL_INTERPRETER_DEFAULT_ARGUMENTS_CAPACITY];
        ptcl_expression **arguments = arguments_array;
        bool needs_free = (expression->func_call.count >= PTCL_INTERPRETER_MAX_ARGUMENTS);
        if (needs_free)
        {
            arguments = malloc(expression->func_call.count * sizeof(ptcl_expression *));
            if (arguments == NULL)
            {
                ptcl_parser_throw_out_of_memory(interpreter->parser, location);
                return NULL;
            }
        }

        for (size_t i = 0; i < expression->func_call.count; i++)
        {
            ptcl_expression *argument = ptcl_interpreter_evaluate_expression(interpreter, expression->func_call.arguments[i], location);
            if (ptcl_parser_critical(interpreter->parser))
            {
                ptcl_expression_by_count_destroy(arguments, expression->func_call.count);
                if (needs_free)
                {
                    free(arguments);
                }

                return NULL;
            }

            arguments[i] = argument;
        }

        ptcl_statement_func_call func_call = expression->func_call;
        func_call.arguments = arguments;
        result = ptcl_interpreter_evaluate_function_call(interpreter, func_call, location);
        ptcl_expression_by_count_destroy(arguments, expression->func_call.count);
        if (needs_free)
        {
            free(arguments);
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
        result = ptcl_interpreter_try_get_value(interpreter, expression->variable.name);
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

ptcl_expression *ptcl_interpreter_evaluate_function_call(ptcl_interpreter *interpreter, ptcl_statement_func_call func_call, ptcl_location location)
{
    size_t variables_count = interpreter->variables_count;
    for (size_t i = 0; i < func_call.count; i++)
    {
        ptcl_argument argument = func_call.func_decl.arguments[i];
        if (argument.is_variadic)
        {
            continue;
        }

        ptcl_expression *call_argument = func_call.arguments[i];
        if (!ptcl_interpreter_add_variable(interpreter, argument.name, call_argument))
        {
            ptcl_parser_throw_out_of_memory(interpreter->parser, location);
            return NULL;
        }
    }

    ptcl_expression *result = ptcl_interpreter_evaluate_func_body(interpreter, *func_call.func_decl.func_body, location);
    interpreter->variables_count = variables_count;
    return result;
}

ptcl_expression *ptcl_interpreter_try_get_value(ptcl_interpreter *interpreter, ptcl_name name)
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

bool ptcl_interpreter_add_variable(ptcl_interpreter *interpreter, ptcl_name name, ptcl_expression *value)
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
        .value = value};
    return true;
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