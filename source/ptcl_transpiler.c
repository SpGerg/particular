#include <string.h>
#include <ptcl_transpiler.h>
#include <ptcl_string_buffer.h>

typedef struct ptcl_transpiler
{
    ptcl_parser_result result;
    ptcl_string_buffer *string_buffer;
    size_t depth;
} ptcl_transpiler;

static void ptcl_transpiler_add_arrays_length_arguments(ptcl_transpiler *transpiler, char *name, ptcl_type type, size_t count, bool from_position)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    ptcl_transpiler_append_character(transpiler, ',', from_position);
    ptcl_transpiler_append_word_s(transpiler, "size_t", from_position);

    char *length = ptcl_from_long(count);
    char *copy = ptcl_string(name, "_length_", length, NULL);
    ptcl_transpiler_append_word_s(transpiler, copy, from_position);
    free(length);
    free(copy);

    ptcl_transpiler_add_arrays_length_arguments(transpiler, name, *type.target, ++count, from_position);
}

static void ptcl_transpiler_add_arrays_length(ptcl_transpiler *transpiler, ptcl_type type, bool from_position)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    size_t count = type.count;
    ptcl_transpiler_append_character(transpiler, ',', from_position);
    char *length = ptcl_from_long(count);
    ptcl_transpiler_append_word_s(transpiler, length, from_position);
    free(length);

    if (type.target->type == ptcl_value_array_type)
    {
        ptcl_transpiler_add_arrays_length(transpiler, *type.target, from_position);
    }
}

static void ptcl_transpiler_add_array_dimensional(ptcl_transpiler *transpiler, ptcl_type type, bool from_position)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    ptcl_transpiler_append_character(transpiler, '[', from_position);
    char *number = ptcl_from_long(type.count);
    if (number != NULL)
    {
        ptcl_transpiler_append_word_s(transpiler, number, from_position);
        free(number);
    }

    ptcl_transpiler_append_character(transpiler, ']', from_position);
    ptcl_transpiler_add_array_dimensional(transpiler, *type.target, from_position);
}

ptcl_transpiler *ptcl_transpiler_create(ptcl_parser_result result)
{
    ptcl_transpiler *transpiler = malloc(sizeof(ptcl_transpiler));
    if (transpiler == NULL)
    {
        return NULL;
    }

    transpiler->string_buffer = ptcl_string_buffer_create();
    if (transpiler->string_buffer == NULL)
    {
        free(transpiler);
        return NULL;
    }

    transpiler->result = result;
    transpiler->depth = 0;
    return transpiler;
}

char *ptcl_transpiler_transpile(ptcl_transpiler *transpiler)
{
    ptcl_transpiler_add_func_body(transpiler, transpiler->result.body, false, false, false);
    char *result = ptcl_string_buffer_copy_and_clear(transpiler->string_buffer);

    return result;
}

bool ptcl_transpiler_append_word_s(ptcl_transpiler *transpiler, char *word, bool from_position)
{
    if (from_position)
    {
        ptcl_string_buffer_insert_str(transpiler->string_buffer, word, strlen(word));
        ptcl_string_buffer_insert(transpiler->string_buffer, ' ');
    }
    else
    {
        ptcl_transpiler_append_word(transpiler, word, from_position);
        ptcl_transpiler_append_character(transpiler, ' ', from_position);
    }
}

bool ptcl_transpiler_append_word(ptcl_transpiler *transpiler, char *word, bool from_position)
{
    if (from_position)
    {
        ptcl_string_buffer_insert_str(transpiler->string_buffer, word, strlen(word));
    }
    else
    {
        ptcl_string_buffer_append_str(transpiler->string_buffer, word, strlen(word));
    }
}

bool ptcl_transpiler_append_character(ptcl_transpiler *transpiler, char character, bool from_position)
{
    if (from_position)
    {
        ptcl_string_buffer_insert(transpiler->string_buffer, character);
    }
    else
    {
        ptcl_string_buffer_append(transpiler->string_buffer, character);
    }
}

void ptcl_transpiler_add_func_body(ptcl_transpiler *transpiler, ptcl_statement_func_body func_body, bool with_brackets, bool from_position, bool is_setter)
{
    if (with_brackets)
    {
        ptcl_transpiler_append_character(transpiler, '{', is_setter);
    }

    for (size_t i = 0; i < func_body.count; i++)
    {
        ptcl_transpiler_add_statement(transpiler, &func_body.statements[i], from_position, from_position);
    }

    if (with_brackets)
    {
        ptcl_transpiler_append_character(transpiler, '}', is_setter);
    }
}

void ptcl_transpiler_add_statement(ptcl_transpiler *transpiler, ptcl_statement *statement, bool from_position, bool is_setter)
{
    from_position = transpiler->depth != 1 ? false : from_position;

    switch (statement->type)
    {
    case ptcl_statement_func_body_type:
        ptcl_transpiler_add_func_body(transpiler, statement->body, false, from_position, from_position);
        break;
    case ptcl_statement_func_call_type:
        ptcl_transpiler_add_func_call(transpiler, statement->func_call, from_position);
        break;
    case ptcl_statement_assign_type:
        if (statement->assign.is_define)
        {
            ptcl_transpiler_add_type(transpiler, statement->assign.type, false, from_position);
            ptcl_transpiler_add_identifier(transpiler, statement->assign.identifier, from_position);
            ptcl_transpiler_add_array_dimensional(transpiler, statement->assign.type, from_position);
        }
        else
        {
            ptcl_transpiler_add_identifier(transpiler, statement->assign.identifier, from_position);
        }

        ptcl_transpiler_append_character(transpiler, '=', from_position);
        ptcl_transpiler_add_expression(transpiler, statement->assign.value, false, from_position);
        ptcl_transpiler_append_character(transpiler, ';', from_position);
        break;
    case ptcl_statement_func_decl_type:
        ptcl_transpiler_add_func_decl(transpiler, statement->func_decl.name, statement->func_decl, false, from_position);
        break;
    case ptcl_statement_typedata_decl_type:
        ptcl_transpiler_append_word_s(transpiler, "typedef struct", from_position);
        ptcl_transpiler_append_word_s(transpiler, statement->typedata_decl.name, from_position);
        ptcl_transpiler_append_character(transpiler, '{', from_position);
        for (size_t i = 0; i < statement->typedata_decl.count; i++)
        {
            ptcl_typedata_member member = statement->typedata_decl.members[i];
            ptcl_transpiler_add_type(transpiler, member.type, false, from_position);
            ptcl_transpiler_append_word_s(transpiler, member.name, from_position);
            ptcl_transpiler_append_character(transpiler, ';', from_position);
        }

        ptcl_transpiler_append_character(transpiler, '}', from_position);
        ptcl_transpiler_append_word_s(transpiler, statement->typedata_decl.name, from_position);
        ptcl_transpiler_append_character(transpiler, ';', from_position);
        break;
    case ptcl_statement_return_type:
        ptcl_transpiler_append_word_s(transpiler, "return", from_position);
        ptcl_transpiler_add_expression(transpiler, statement->ret.value, false, from_position);
        ptcl_transpiler_append_character(transpiler, ';', from_position);
        break;
    case ptcl_statement_if_type:
        ptcl_transpiler_append_word_s(transpiler, "if", from_position);
        ptcl_transpiler_append_character(transpiler, '(', from_position);
        ptcl_transpiler_add_expression(transpiler, statement->if_stat.condition, false, from_position);
        ptcl_transpiler_append_character(transpiler, ')', from_position);
        ptcl_transpiler_add_func_body(transpiler, statement->if_stat.body, true, false, from_position);
        if (statement->if_stat.with_else)
        {
            ptcl_transpiler_add_func_body(transpiler, statement->if_stat.else_body, true, false, from_position);
        }

        break;
    }
}

void ptcl_transpiler_add_func_decl(ptcl_transpiler *transpiler, char *name, ptcl_statement_func_decl func_decl, bool is_prototype, bool from_position)
{
    size_t start = ptcl_string_buffer_length(transpiler->string_buffer);
    ptcl_transpiler_add_type(transpiler, func_decl.return_type, false, from_position);
    ptcl_transpiler_append_word_s(transpiler, name, from_position);
    ptcl_transpiler_append_character(transpiler, '(', from_position);
    for (size_t i = 0; i < func_decl.count; i++)
    {
        ptcl_argument argument = func_decl.arguments[i];
        if (argument.is_variadic)
        {
            ptcl_transpiler_append_word_s(transpiler, "...", from_position);
            break;
        }
        else
        {
            ptcl_transpiler_add_type(transpiler, argument.type, false, from_position);
            ptcl_transpiler_append_word_s(transpiler, argument.name, from_position);
            ptcl_transpiler_add_arrays_length_arguments(transpiler, argument.name, argument.type, 0, from_position);

            if (i != func_decl.count - 1)
            {
                ptcl_transpiler_append_character(transpiler, ',', from_position);
            }
        }
    }

    ptcl_transpiler_append_character(transpiler, ')', from_position);
    if (func_decl.is_prototype || is_prototype)
    {
        ptcl_transpiler_append_character(transpiler, ';', from_position);
    }
    else
    {
        if (transpiler->depth == 0)
        {
            size_t position = ptcl_string_buffer_get_position(transpiler->string_buffer);
            ptcl_string_buffer_set_position(transpiler->string_buffer, start);
            transpiler->depth++;
        }

        ptcl_transpiler_add_func_body(transpiler, func_decl.func_body, true, true, from_position);
        if (transpiler->depth > 0)
        {
            transpiler->depth--;
        }
    }
}

void ptcl_transpiler_add_func_call(ptcl_transpiler *transpiler, ptcl_statement_func_call func_call, bool from_position)
{
    ptcl_transpiler_add_identifier(transpiler, func_call.identifier, from_position);
    ptcl_transpiler_append_character(transpiler, '(', from_position);
    for (size_t i = 0; i < func_call.count; i++)
    {
        ptcl_expression argument = func_call.arguments[i];
        ptcl_transpiler_add_expression(transpiler, argument, true, from_position);
        // ptcl_transpiler_add_arrays_length(transpiler, argument.return_type);

        if (i != func_call.count - 1)
        {
            ptcl_transpiler_append_character(transpiler, ',', from_position);
        }
    }

    ptcl_transpiler_append_character(transpiler, ')', from_position);
    ptcl_transpiler_append_character(transpiler, ';', from_position);
}

void ptcl_transpiler_add_expression(ptcl_transpiler *transpiler, ptcl_expression expression, bool specify_type, bool from_position)
{
    switch (expression.type)
    {
    case ptcl_expression_array_type:
        if (expression.return_type.is_static && expression.return_type.target->type == ptcl_value_character_type)
        {
            ptcl_transpiler_append_character(transpiler, '\"', from_position);
            // -1 because of end of line symbol
            for (size_t i = 0; i < expression.array.count - 1; i++)
            {
                ptcl_transpiler_append_character(transpiler, expression.array.expressions[i].character, from_position);
            }

            ptcl_transpiler_append_character(transpiler, '\"', from_position);
            break;
        }

        if (specify_type)
        {
            ptcl_transpiler_append_character(transpiler, '(', from_position);
            ptcl_transpiler_add_type(transpiler, expression.return_type, false, from_position);
            ptcl_transpiler_add_array_dimensional(transpiler, expression.return_type, from_position);
            ptcl_transpiler_append_character(transpiler, ')', from_position);
        }

        ptcl_transpiler_append_character(transpiler, '{', from_position);
        for (size_t i = 0; i < expression.array.count; i++)
        {
            ptcl_transpiler_add_expression(transpiler, expression.array.expressions[i], false, from_position);

            if (i != expression.array.count - 1)
            {
                ptcl_transpiler_append_character(transpiler, ',', from_position);
            }
        }

        ptcl_transpiler_append_character(transpiler, '}', from_position);
        break;
    case ptcl_expression_variable_type:
        ptcl_transpiler_append_word_s(transpiler, expression.variable.name, from_position);
        break;
    case ptcl_expression_word_type:
        ptcl_transpiler_append_character(transpiler, '\"', from_position);
        ptcl_transpiler_append_word(transpiler, expression.word, from_position);
        ptcl_transpiler_append_character(transpiler, '\"', from_position);
        break;
    case ptcl_expression_character_type:
        ptcl_transpiler_append_character(transpiler, '\'', from_position);
        if (expression.character == '\0')
        {
            ptcl_transpiler_append_character(transpiler, '\\', from_position);
            ptcl_transpiler_append_character(transpiler, '0', from_position);
        }
        else
        {
            ptcl_transpiler_append_character(transpiler, expression.character, from_position);
        }

        ptcl_transpiler_append_character(transpiler, '\'', from_position);
        break;
    case ptcl_expression_binary_type:
        ptcl_transpiler_add_expression(transpiler, expression.binary.children[0], false, from_position);
        ptcl_transpiler_add_binary_type(transpiler, expression.binary.type, from_position);
        ptcl_transpiler_add_expression(transpiler, expression.binary.children[1], false, from_position);
        break;
    case ptcl_expression_unary_type:
        ptcl_transpiler_add_binary_type(transpiler, expression.unary.type, from_position);
        ptcl_transpiler_add_expression(transpiler, *expression.unary.child, false, from_position);
        break;
    case ptcl_expression_array_element_type:
        ptcl_transpiler_add_expression(transpiler, expression.array_element.children[0], false, from_position);
        ptcl_transpiler_append_character(transpiler, '[', from_position);
        ptcl_transpiler_add_expression(transpiler, expression.array_element.children[1], false, from_position);
        ptcl_transpiler_append_character(transpiler, ']', from_position);
        break;
    case ptcl_expression_double_type:
        char *double_n = ptcl_from_double(expression.double_n);
        if (double_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, double_n, from_position);
            free(double_n);
        }

        break;
    case ptcl_expression_float_type:
        char *float_n = ptcl_from_float(expression.float_n);
        if (float_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, float_n, from_position);
            free(float_n);
        }

        break;
    case ptcl_expression_integer_type:
        char *integer_n = ptcl_from_int(expression.integer_n);
        if (integer_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, integer_n, from_position);
            free(integer_n);
        }

        break;
    case ptcl_expression_ctor_type:
        if (specify_type)
        {
            ptcl_transpiler_append_character(transpiler, '(', from_position);
            ptcl_transpiler_append_word_s(transpiler, expression.ctor.name, from_position);
            ptcl_transpiler_append_character(transpiler, ')', from_position);
        }

        ptcl_transpiler_append_character(transpiler, '{', from_position);
        for (size_t i = 0; i < expression.ctor.count; i++)
        {
            ptcl_transpiler_add_expression(transpiler, expression.ctor.values[i], true, from_position);
        }

        ptcl_transpiler_append_character(transpiler, '}', from_position);

        break;
    case ptcl_expression_dot_type:
        ptcl_transpiler_add_expression(transpiler, *expression.dot.left, false, from_position);
        ptcl_transpiler_append_character(transpiler, '.', from_position);
        ptcl_transpiler_append_word_s(transpiler, expression.dot.name, from_position);
        break;
    case ptcl_expression_func_call_type:
        ptcl_transpiler_add_func_call(transpiler, expression.func_call, from_position);
        break;
    }
}

void ptcl_transpiler_add_identifier(ptcl_transpiler *transpiler, ptcl_identifier identifier, bool from_position)
{
    if (identifier.is_name)
    {
        ptcl_transpiler_append_word_s(transpiler, identifier.name, from_position);
    }
}

void ptcl_transpiler_add_type(ptcl_transpiler *transpiler, ptcl_type type, bool with_array, bool from_position)
{
    switch (type.type)
    {
    case ptcl_value_typedata_type:
        ptcl_transpiler_append_word_s(transpiler, type.identifier, from_position);
        break;
    case ptcl_value_array_type:
        ptcl_transpiler_add_type(transpiler, *type.target, with_array, from_position);

        if (with_array)
        {
            ptcl_transpiler_append_character(transpiler, '[', from_position);
            ptcl_transpiler_append_character(transpiler, ']', from_position);
        }

        break;
    case ptcl_value_pointer_type:
        ptcl_transpiler_add_type(transpiler, *type.target, with_array, from_position);
        ptcl_transpiler_append_character(transpiler, '*', from_position);
        break;
    case ptcl_value_word_type:
        ptcl_transpiler_append_word_s(transpiler, "char*", from_position);
        break;
    case ptcl_value_any_pointer_type:
        ptcl_transpiler_append_word_s(transpiler, "void*", from_position);
        break;
    case ptcl_value_character_type:
        ptcl_transpiler_append_word_s(transpiler, "char", from_position);
        break;
    case ptcl_value_double_type:
        ptcl_transpiler_append_word_s(transpiler, "double", from_position);
        break;
    case ptcl_value_float_type:
        ptcl_transpiler_append_word_s(transpiler, "float", from_position);
        break;
    case ptcl_value_integer_type:
        ptcl_transpiler_append_word_s(transpiler, "int", from_position);
        break;
    case ptcl_value_void_type:
        ptcl_transpiler_append_word_s(transpiler, "void", from_position);
        break;
    }
}

void ptcl_transpiler_add_binary_type(ptcl_transpiler *transpiler, ptcl_binary_operator_type type, bool from_position)
{
    char *value;

    switch (type)
    {
    case ptcl_binary_operator_plus_type:
        value = "+";
        break;
    case ptcl_binary_operator_minus_type:
        value = "-";
        break;
    case ptcl_binary_operator_multiplicative_type:
        value = "*";
        break;
    case ptcl_binary_operator_division_type:
        value = "/";
        break;
    case ptcl_binary_operator_negation_type:
        value = "!";
        break;
    case ptcl_binary_operator_and_type:
        value = "&&";
        break;
    case ptcl_binary_operator_or_type:
        value = "||";
        break;
    case ptcl_binary_operator_not_equals_type:
        value = "!=";
        break;
    case ptcl_binary_operator_equals_type:
        value = "==";
        break;
    case ptcl_binary_operator_reference_type:
        value = "&";
        break;
    case ptcl_binary_operator_dereference_type:
        value = "*";
        break;
    case ptcl_binary_operator_greater_than_type:
        value = ">";
        break;
    case ptcl_binary_operator_less_than_type:
        value = "<";
        break;
    case ptcl_binary_operator_greater_equals_than_type:
        value = ">=";
        break;
    case ptcl_binary_operator_less_equals_than_type:
        value = "<=";
        break;
    }

    ptcl_transpiler_append_word_s(transpiler, value, from_position);
}

void ptcl_transpiler_destroy(ptcl_transpiler *transpiler)
{
    ptcl_string_buffer_destroy(transpiler->string_buffer);
    free(transpiler);
}