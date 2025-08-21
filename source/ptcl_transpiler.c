#include <string.h>
#include <ptcl_transpiler.h>
#include <ptcl_string_buffer.h>

typedef struct ptcl_transpiler
{
    ptcl_parser_result result;
    ptcl_string_buffer *string_buffer;
} ptcl_transpiler;

static void ptcl_transpiler_add_arrays_length_arguments(ptcl_transpiler *transpiler, char *name, ptcl_type type, size_t count)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    ptcl_transpiler_append_character(transpiler, ',');
    ptcl_transpiler_append_word_s(transpiler, "size_t");

    char *length = ptcl_from_long(count);
    char *copy = ptcl_string(name, "_length_", length, NULL);
    ptcl_transpiler_append_word_s(transpiler, copy);
    free(length);
    free(copy);

    ptcl_transpiler_add_arrays_length_arguments(transpiler, name, *type.target, ++count);
}

static void ptcl_transpiler_add_arrays_length(ptcl_transpiler *transpiler, ptcl_type type)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    size_t count = type.count;
    ptcl_transpiler_append_character(transpiler, ',');
    char *length = ptcl_from_long(count);
    ptcl_transpiler_append_word_s(transpiler, length);
    free(length);

    if (type.target->type == ptcl_value_array_type)
    {
        ptcl_transpiler_add_arrays_length(transpiler, *type.target);
    }
}

static void ptcl_transpiler_add_array_dimensional(ptcl_transpiler *transpiler, ptcl_type type)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    ptcl_transpiler_append_character(transpiler, '[');
    char *number = ptcl_from_long(type.count);
    if (number != NULL)
    {
        ptcl_transpiler_append_word_s(transpiler, number);
        free(number);
    }

    ptcl_transpiler_append_character(transpiler, ']');
    ptcl_transpiler_add_array_dimensional(transpiler, *type.target);
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
    return transpiler;
}

char *ptcl_transpiler_transpile(ptcl_transpiler *transpiler)
{
    ptcl_transpiler_add_func_body(transpiler, transpiler->result.body, false);
    char *result = ptcl_string_buffer_copy_and_clear(transpiler->string_buffer);

    return result;
}

bool ptcl_transpiler_append_word_s(ptcl_transpiler *transpiler, char *word)
{
    ptcl_transpiler_append_word(transpiler, word);
    ptcl_transpiler_append_character(transpiler, ' ');
}

bool ptcl_transpiler_append_word(ptcl_transpiler *transpiler, char *word)
{
    ptcl_string_buffer_append_str(transpiler->string_buffer, word, strlen(word));
}

bool ptcl_transpiler_append_character(ptcl_transpiler *transpiler, char character)
{
    ptcl_string_buffer_append(transpiler->string_buffer, character);
}

void ptcl_transpiler_add_func_body(ptcl_transpiler *transpiler, ptcl_statement_func_body func_body, bool with_brackets)
{
    if (with_brackets)
    {
        ptcl_transpiler_append_character(transpiler, '{');
    }

    for (size_t i = 0; i < func_body.count; i++)
    {
        ptcl_transpiler_add_statement(transpiler, &func_body.statements[i]);
    }

    if (with_brackets)
    {
        ptcl_transpiler_append_character(transpiler, '}');
    }
}

void ptcl_transpiler_add_statement(ptcl_transpiler *transpiler, ptcl_statement *statement)
{
    switch (statement->type)
    {
    case ptcl_statement_func_body_type:
        ptcl_transpiler_add_func_body(transpiler, statement->body, false);
        break;
    case ptcl_statement_func_call_type:
        ptcl_transpiler_add_func_call(transpiler, statement->func_call);
        break;
    case ptcl_statement_assign_type:
        if (statement->assign.is_define)
        {
            ptcl_transpiler_add_type(transpiler, statement->assign.type, false);
            ptcl_transpiler_add_identifier(transpiler, statement->assign.identifier);
            ptcl_transpiler_add_array_dimensional(transpiler, statement->assign.type);
        }
        else
        {
            ptcl_transpiler_add_identifier(transpiler, statement->assign.identifier);
        }

        ptcl_transpiler_append_character(transpiler, '=');
        ptcl_transpiler_add_expression(transpiler, statement->assign.value, false);
        ptcl_transpiler_append_character(transpiler, ';');

        break;
    case ptcl_statement_func_decl_type:
        ptcl_transpiler_add_func_decl(transpiler, statement->func_decl.name, statement->func_decl, false);
        break;
    case ptcl_statement_typedata_decl_type:
        ptcl_transpiler_append_word_s(transpiler, "typedef struct");
        ptcl_transpiler_append_word_s(transpiler, statement->typedata_decl.name);
        ptcl_transpiler_append_character(transpiler, '{');
        for (size_t i = 0; i < statement->typedata_decl.count; i++)
        {
            ptcl_typedata_member member = statement->typedata_decl.members[i];
            ptcl_transpiler_add_type(transpiler, member.type, false);
            ptcl_transpiler_append_word_s(transpiler, member.name);
            ptcl_transpiler_append_character(transpiler, ';');
        }

        ptcl_transpiler_append_character(transpiler, '}');
        ptcl_transpiler_append_word_s(transpiler, statement->typedata_decl.name);
        ptcl_transpiler_append_character(transpiler, ';');
        break;
    case ptcl_statement_return_type:
        ptcl_transpiler_append_word_s(transpiler, "return");
        ptcl_transpiler_add_expression(transpiler, statement->ret.value, false);
        ptcl_transpiler_append_character(transpiler, ';');
        break;
    case ptcl_statement_if_type:
        ptcl_transpiler_append_word_s(transpiler, "if");
        ptcl_transpiler_append_character(transpiler, '(');
        ptcl_transpiler_add_expression(transpiler, statement->if_stat.condition, false);
        ptcl_transpiler_append_character(transpiler, ')');
        ptcl_transpiler_add_func_body(transpiler, statement->if_stat.body, true);
        if (statement->if_stat.with_else)
        {
            ptcl_transpiler_add_func_body(transpiler, statement->if_stat.else_body, true);
        }
        break;
    }
}

void ptcl_transpiler_add_func_decl(ptcl_transpiler *transpiler, char *name, ptcl_statement_func_decl func_decl, bool is_prototype)
{
    ptcl_transpiler_add_type(transpiler, func_decl.return_type, false);
    ptcl_transpiler_append_word_s(transpiler, name);
    ptcl_transpiler_append_character(transpiler, '(');
    for (size_t i = 0; i < func_decl.count; i++)
    {
        ptcl_argument argument = func_decl.arguments[i];
        if (argument.is_variadic)
        {
            ptcl_transpiler_append_word_s(transpiler, "...");
            break;
        }
        else
        {
            ptcl_transpiler_add_type(transpiler, argument.type, false);
            ptcl_transpiler_append_word_s(transpiler, argument.name);
            ptcl_transpiler_add_arrays_length_arguments(transpiler, argument.name, argument.type, 0);

            if (i != func_decl.count - 1)
            {
                ptcl_transpiler_append_character(transpiler, ',');
            }
        }
    }

    ptcl_transpiler_append_character(transpiler, ')');

    if (func_decl.is_prototype || is_prototype)
    {
        ptcl_transpiler_append_character(transpiler, ';');
    }
    else
    {
        ptcl_transpiler_add_func_body(transpiler, func_decl.func_body, true);
    }
}

void ptcl_transpiler_add_func_call(ptcl_transpiler *transpiler, ptcl_statement_func_call func_call)
{
    ptcl_transpiler_add_identifier(transpiler, func_call.identifier);
    ptcl_transpiler_append_character(transpiler, '(');
    for (size_t i = 0; i < func_call.count; i++)
    {
        ptcl_expression argument = func_call.arguments[i];
        ptcl_transpiler_add_expression(transpiler, argument, true);
        // ptcl_transpiler_add_arrays_length(transpiler, argument.return_type);

        if (i != func_call.count - 1)
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }
    }

    ptcl_transpiler_append_character(transpiler, ')');
    ptcl_transpiler_append_character(transpiler, ';');
}

void ptcl_transpiler_add_expression(ptcl_transpiler *transpiler, ptcl_expression expression, bool specify_type)
{
    switch (expression.type)
    {
    case ptcl_expression_array_type:
        if (expression.return_type.is_static && expression.return_type.target->type == ptcl_value_character_type)
        {
            ptcl_transpiler_append_character(transpiler, '\"');
            // -1 because of end of line symbol
            for (size_t i = 0; i < expression.array.count - 1; i++)
            {
                ptcl_transpiler_append_character(transpiler, expression.array.expressions[i].character);
            }

            ptcl_transpiler_append_character(transpiler, '\"');
            break;
        }

        if (specify_type)
        {
            ptcl_transpiler_append_character(transpiler, '(');
            ptcl_transpiler_add_type(transpiler, expression.return_type, false);
            ptcl_transpiler_add_array_dimensional(transpiler, expression.return_type);
            ptcl_transpiler_append_character(transpiler, ')');
        }

        ptcl_transpiler_append_character(transpiler, '{');
        for (size_t i = 0; i < expression.array.count; i++)
        {
            ptcl_transpiler_add_expression(transpiler, expression.array.expressions[i], false);

            if (i != expression.array.count - 1)
            {
                ptcl_transpiler_append_character(transpiler, ',');
            }
        }

        ptcl_transpiler_append_character(transpiler, '}');
        break;
    case ptcl_expression_variable_type:
        ptcl_transpiler_append_word_s(transpiler, expression.variable.name);
        break;
    case ptcl_expression_word_type:
        ptcl_transpiler_append_character(transpiler, '\"');
        ptcl_transpiler_append_word(transpiler, expression.word);
        ptcl_transpiler_append_character(transpiler, '\"');
        break;
    case ptcl_expression_character_type:
        ptcl_transpiler_append_character(transpiler, '\'');
        if (expression.character == '\0')
        {
            ptcl_transpiler_append_character(transpiler, '\\');
            ptcl_transpiler_append_character(transpiler, '0');
        }
        else
        {
            ptcl_transpiler_append_character(transpiler, expression.character);
        }

        ptcl_transpiler_append_character(transpiler, '\'');
        break;
    case ptcl_expression_binary_type:
        ptcl_transpiler_add_expression(transpiler, expression.binary.children[0], false);
        ptcl_transpiler_add_binary_type(transpiler, expression.binary.type);
        ptcl_transpiler_add_expression(transpiler, expression.binary.children[1], false);
        break;
    case ptcl_expression_unary_type:
        ptcl_transpiler_add_binary_type(transpiler, expression.unary.type);
        ptcl_transpiler_add_expression(transpiler, *expression.unary.child, false);
        break;
    case ptcl_expression_array_element_type:
        ptcl_transpiler_add_expression(transpiler, expression.array_element.children[0], false);
        ptcl_transpiler_append_character(transpiler, '[');
        ptcl_transpiler_add_expression(transpiler, expression.array_element.children[1], false);
        ptcl_transpiler_append_character(transpiler, ']');
        break;
    case ptcl_expression_double_type:
        char *double_n = ptcl_from_double(expression.double_n);
        if (double_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, double_n);
            free(double_n);
        }

        break;
    case ptcl_expression_float_type:
        char *float_n = ptcl_from_float(expression.float_n);
        if (float_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, float_n);
            free(float_n);
        }

        break;
    case ptcl_expression_integer_type:
        char *integer_n = ptcl_from_int(expression.integer_n);
        if (integer_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, integer_n);
            free(integer_n);
        }

        break;
    case ptcl_expression_ctor_type:
        if (specify_type)
        {
            ptcl_transpiler_append_character(transpiler, '(');
            ptcl_transpiler_append_word_s(transpiler, expression.ctor.name);
            ptcl_transpiler_append_character(transpiler, ')');
        }

        ptcl_transpiler_append_character(transpiler, '{');
        for (size_t i = 0; i < expression.ctor.count; i++)
        {
            ptcl_transpiler_add_expression(transpiler, expression.ctor.values[i], true);
        }

        ptcl_transpiler_append_character(transpiler, '}');

        break;
    case ptcl_expression_dot_type:
        ptcl_transpiler_add_expression(transpiler, *expression.dot.left, false);
        ptcl_transpiler_append_character(transpiler, '.');
        ptcl_transpiler_append_word_s(transpiler, expression.dot.name);
        break;
    case ptcl_expression_func_call_type:
        ptcl_transpiler_add_func_call(transpiler, expression.func_call);
        break;
    }
}

void ptcl_transpiler_add_identifier(ptcl_transpiler *transpiler, ptcl_identifier identifier)
{
    if (identifier.is_name)
    {
        ptcl_transpiler_append_word_s(transpiler, identifier.name);
    }
}

void ptcl_transpiler_add_type(ptcl_transpiler *transpiler, ptcl_type type, bool with_array)
{
    switch (type.type)
    {
    case ptcl_value_typedata_type:
        ptcl_transpiler_append_word_s(transpiler, type.identifier);
        break;
    case ptcl_value_array_type:
        ptcl_transpiler_add_type(transpiler, *type.target, with_array);

        if (with_array)
        {
            ptcl_transpiler_append_character(transpiler, '[');
            ptcl_transpiler_append_character(transpiler, ']');
        }

        break;
    case ptcl_value_pointer_type:
        ptcl_transpiler_add_type(transpiler, *type.target, with_array);
        ptcl_transpiler_append_character(transpiler, '*');
        break;
    case ptcl_value_word_type:
        ptcl_transpiler_append_word_s(transpiler, "char*");
        break;
    case ptcl_value_character_type:
        ptcl_transpiler_append_word_s(transpiler, "char");
        break;
    case ptcl_value_double_type:
        ptcl_transpiler_append_word_s(transpiler, "double");
        break;
    case ptcl_value_float_type:
        ptcl_transpiler_append_word_s(transpiler, "float");
        break;
    case ptcl_value_integer_type:
        ptcl_transpiler_append_word_s(transpiler, "int");
        break;
    case ptcl_value_void_type:
        ptcl_transpiler_append_word_s(transpiler, "void");
        break;
    }
}

void ptcl_transpiler_add_binary_type(ptcl_transpiler *transpiler, ptcl_binary_operator_type type)
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

    ptcl_transpiler_append_word_s(transpiler, value);
}

void ptcl_transpiler_destroy(ptcl_transpiler *transpiler)
{
    ptcl_string_buffer_destroy(transpiler->string_buffer);
    free(transpiler);
}