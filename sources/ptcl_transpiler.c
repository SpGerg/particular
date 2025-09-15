#include <string.h>
#include <ptcl_transpiler.h>
#include <ptcl_string_buffer.h>

typedef struct ptcl_transpiler
{
    ptcl_parser_result result;
    ptcl_string_buffer *string_buffer;
    ptcl_transpiler_variable *variables;
    size_t variables_count;
    ptcl_transpiler_function *inner_functions;
    size_t inner_functions_count;
    ptcl_statement_func_body *root;
    ptcl_statement_func_body *main_root;
    ptcl_transpiler_anonymous *anonymouses;
    size_t anonymous_count;
    bool from_position;
    bool in_inner;
    bool add_stdlib;
    int start;
    size_t length;
} ptcl_transpiler;

static void ptcl_transpiler_add_arrays_length_arguments(ptcl_transpiler *transpiler, ptcl_name_word name, ptcl_type type, size_t count)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    ptcl_transpiler_append_character(transpiler, ',');
    ptcl_transpiler_append_word_s(transpiler, "size_t");

    char *length = ptcl_from_long(count);
    ptcl_transpiler_add_name(transpiler, name, false);
    ptcl_transpiler_append_word(transpiler, "_length_");
    ptcl_transpiler_append_word(transpiler, length);
    free(length);

    ptcl_transpiler_add_arrays_length_arguments(transpiler, name, *ptcl_type_get_target(type), ++count);
}

static void ptcl_transpiler_add_arrays_length(ptcl_transpiler *transpiler, ptcl_type type, bool from_position)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    size_t count = type.array.count;
    ptcl_transpiler_append_character(transpiler, ',');
    char *length = ptcl_from_long(count);
    ptcl_transpiler_append_word_s(transpiler, length);
    free(length);

    if (type.array.target->type == ptcl_value_array_type)
    {
        ptcl_transpiler_add_arrays_length(transpiler, *type.array.target, from_position);
    }
}

static void ptcl_transpiler_add_array_dimensional(ptcl_transpiler *transpiler, ptcl_type type)
{
    if (type.type != ptcl_value_array_type)
    {
        return;
    }

    ptcl_transpiler_append_character(transpiler, '[');
    char *number = ptcl_from_long(type.array.count);
    if (number != NULL)
    {
        ptcl_transpiler_append_word_s(transpiler, number);
        free(number);
    }

    ptcl_transpiler_append_character(transpiler, ']');
    ptcl_transpiler_add_array_dimensional(transpiler, *type.array.target);
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
    transpiler->in_inner = false;
    transpiler->variables = NULL;
    transpiler->variables_count = 0;
    transpiler->inner_functions = NULL;
    transpiler->inner_functions_count = 0;
    transpiler->root = NULL;
    transpiler->anonymouses = NULL;
    transpiler->anonymous_count = 0;
    transpiler->start = -1;
    transpiler->length = 0;
    transpiler->add_stdlib = false;
    return transpiler;
}

char *ptcl_transpiler_transpile(ptcl_transpiler *transpiler)
{
    transpiler->main_root = &transpiler->result.body;
    ptcl_transpiler_add_func_body(transpiler, transpiler->result.body, false, false);

    if (transpiler->add_stdlib)
    {
        ptcl_string_buffer_set_position(transpiler->string_buffer, 0);
        transpiler->from_position = true;
        ptcl_transpiler_append_word_s(transpiler, "#include <stdlib.h>");
    }

    char *result = ptcl_string_buffer_copy_and_clear(transpiler->string_buffer);
    for (size_t i = 0; i < transpiler->anonymous_count; i++)
    {
        free(transpiler->anonymouses[i].alias);
    }

    return result;
}

bool ptcl_transpiler_append_word_s(ptcl_transpiler *transpiler, char *word)
{
    if (transpiler->from_position)
    {
        ptcl_string_buffer_insert_str(transpiler->string_buffer, word, strlen(word));
        return ptcl_string_buffer_insert(transpiler->string_buffer, ' ');
    }
    else
    {
        ptcl_transpiler_append_word(transpiler, word);
        return ptcl_transpiler_append_character(transpiler, ' ');
    }
}

bool ptcl_transpiler_append_word(ptcl_transpiler *transpiler, char *word)
{
    if (transpiler->from_position)
    {
        return ptcl_string_buffer_insert_str(transpiler->string_buffer, word, strlen(word));
    }
    else
    {
        return ptcl_string_buffer_append_str(transpiler->string_buffer, word, strlen(word));
    }
}

bool ptcl_transpiler_append_character(ptcl_transpiler *transpiler, char character)
{
    if (transpiler->from_position)
    {
        return ptcl_string_buffer_insert(transpiler->string_buffer, character);
    }
    else
    {
        return ptcl_string_buffer_append(transpiler->string_buffer, character);
    }
}

bool ptcl_transpiler_is_inner(ptcl_transpiler *transpiler, ptcl_name_word name)
{
    for (int i = transpiler->variables_count - 1; i >= 0; i--)
    {
        ptcl_transpiler_variable variable = transpiler->variables[i];
        if (variable.root == transpiler->main_root)
        {
            break;
        }

        if (!ptcl_name_word_compare(variable.name, name))
        {
            continue;
        }

        if (variable.is_inner)
        {
            return true;
        }
    }

    return false;
}

bool ptcl_transpiler_is_inner_function(ptcl_transpiler *transpiler, ptcl_name_word name)
{
    for (int i = transpiler->inner_functions_count - 1; i >= 0; i--)
    {
        ptcl_transpiler_function function = transpiler->inner_functions[i];
        if (function.root == transpiler->main_root)
        {
            break;
        }

        if (!ptcl_name_word_compare(function.name, name))
        {
            continue;
        }

        return true;
    }

    return false;
}

bool ptcl_transpiler_add_variable(ptcl_transpiler *transpiler, ptcl_name_word name, ptcl_type type, bool is_inner, ptcl_statement_func_body *root)
{
    ptcl_transpiler_variable *buffer = realloc(transpiler->variables, (transpiler->variables_count + 1) * sizeof(ptcl_transpiler_variable));
    if (buffer == NULL)
    {
        return false;
    }

    transpiler->variables = buffer;
    transpiler->variables[transpiler->variables_count++] = ptcl_transpiler_variable_create(name, type, is_inner, root);
    return true;
}

bool ptcl_transpiler_add_inner_func(ptcl_transpiler *transpiler, ptcl_name_word name, ptcl_statement_func_body *root)
{
    ptcl_transpiler_function *buffer = realloc(transpiler->inner_functions, (transpiler->inner_functions_count + 1) * sizeof(ptcl_transpiler_function));
    if (buffer == NULL)
    {
        return false;
    }

    transpiler->inner_functions = buffer;
    transpiler->inner_functions[transpiler->inner_functions_count++] = ptcl_transpiler_function_create(name, root);
    return true;
}

bool ptcl_transpiler_add_anonymous(ptcl_transpiler *transpiler, char *original_name, char *alias, ptcl_statement_func_body *root)
{
    ptcl_transpiler_anonymous *buffer = realloc(transpiler->anonymouses, (transpiler->anonymous_count + 1) * sizeof(ptcl_transpiler_anonymous));
    if (buffer == NULL)
    {
        return false;
    }

    transpiler->anonymouses = buffer;
    transpiler->anonymouses[transpiler->anonymous_count++] = ptcl_transpiler_anonymous_create(original_name, alias, root);
    return true;
}

void ptcl_transpiler_add_func_body(ptcl_transpiler *transpiler, ptcl_statement_func_body func_body, bool with_brackets, bool is_func_body)
{
    ptcl_statement_func_body *previous = transpiler->root;
    if (with_brackets)
    {
        func_body.root = transpiler->root;
        transpiler->root = &func_body;
    }

    if (with_brackets)
    {
        ptcl_transpiler_append_character(transpiler, '{');
    }

    for (size_t i = 0; i < func_body.count; i++)
    {
        ptcl_transpiler_add_statement(transpiler, func_body.statements[i], is_func_body);
    }

    if (with_brackets)
    {
        ptcl_transpiler_append_character(transpiler, '}');
    }

    if (with_brackets)
    {
        ptcl_transpiler_clear_scope(transpiler);
        transpiler->root = previous;
    }
}

void ptcl_transpiler_add_statement(ptcl_transpiler *transpiler, ptcl_statement *statement, bool is_func_body)
{
    size_t start = ptcl_string_buffer_get_position(transpiler->string_buffer);
    transpiler->from_position = transpiler->in_inner ? transpiler->from_position : false;

    switch (statement->type)
    {
    case ptcl_statement_func_body_type:
        ptcl_transpiler_add_func_body(transpiler, statement->body, false, false);
        return;
    case ptcl_statement_func_call_type:
        ptcl_transpiler_add_func_call(transpiler, statement->func_call);
        ptcl_transpiler_append_character(transpiler, ';');
        break;
    case ptcl_statement_assign_type:
        if (statement->assign.is_define && statement->assign.type.is_static)
        {
            return;
        }

        if (ptcl_transpiler_is_inner(transpiler, ptcl_identifier_get_name(statement->assign.identifier)))
        {
            ptcl_transpiler_append_character(transpiler, '*');
        }

        if (statement->assign.is_define)
        {
            ptcl_transpiler_add_type(transpiler, statement->assign.type, false);
            ptcl_transpiler_add_identifier(transpiler, statement->assign.identifier, true);
            ptcl_transpiler_add_array_dimensional(transpiler, statement->assign.type);
            ptcl_transpiler_add_variable(
                transpiler, ptcl_identifier_get_name(statement->assign.identifier), statement->assign.type, false, transpiler->root);
        }
        else
        {
            ptcl_transpiler_add_identifier(transpiler, statement->assign.identifier, false);
        }

        ptcl_transpiler_append_character(transpiler, '=');
        ptcl_transpiler_add_expression(transpiler, statement->assign.value, false);
        ptcl_transpiler_append_character(transpiler, ';');
        break;
    case ptcl_statement_func_decl_type:
        ptcl_transpiler_add_func_decl(transpiler, statement->func_decl, statement->func_decl.name, NULL);
        break;
    case ptcl_statement_type_decl_type:
        for (size_t i = 0; i < statement->type_decl.functions_count; i++)
        {
            ptcl_statement_func_decl function = statement->type_decl.functions[i];
            char *name = ptcl_string("ptcl_t_", statement->type_decl.name.value, "_", function.name.value, NULL);
            ptcl_transpiler_add_func_decl(transpiler, function, ptcl_name_create_fast_w(name, false), &ptcl_type_integer);
            free(name);
        }
        break;
    case ptcl_statement_typedata_decl_type:
        const bool last = transpiler->from_position;
        if (transpiler->in_inner)
        {
            transpiler->from_position = true;
        }

        int previous_start = -1;
        if (transpiler->start != -1)
        {
            previous_start = (int)ptcl_string_buffer_get_position(transpiler->string_buffer);
            ptcl_string_buffer_set_position(transpiler->string_buffer, transpiler->start);
        }

        const size_t position = ptcl_string_buffer_get_position(transpiler->string_buffer);
        const size_t length = ptcl_string_buffer_length(transpiler->string_buffer);
        ptcl_transpiler_append_word_s(transpiler, "typedef struct");
        ptcl_transpiler_add_name(transpiler, statement->typedata_decl.name, true);
        ptcl_transpiler_append_character(transpiler, '{');
        for (size_t i = 0; i < statement->typedata_decl.count; i++)
        {
            ptcl_typedata_member member = statement->typedata_decl.members[i];
            ptcl_transpiler_add_type(transpiler, member.type, false);
            ptcl_transpiler_add_name(transpiler, member.name, false);
            ptcl_transpiler_append_character(transpiler, ';');
        }

        ptcl_transpiler_append_character(transpiler, '}');
        ptcl_transpiler_add_name(transpiler, statement->typedata_decl.name, false);
        ptcl_transpiler_append_character(transpiler, ';');

        if (previous_start != -1)
        {
            const size_t current_length = ptcl_string_buffer_length(transpiler->string_buffer);
            const size_t length_before_body = length;
            const size_t offset = current_length - length_before_body;
            ptcl_string_buffer_set_position(transpiler->string_buffer, previous_start + offset);
            transpiler->start += offset;
        }

        if (transpiler->from_position)
        {
            transpiler->from_position = last;
            return;
        }

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
        ptcl_transpiler_add_func_body(transpiler, statement->if_stat.body, false, is_func_body);
        if (statement->if_stat.with_else)
        {
            ptcl_transpiler_add_func_body(transpiler, statement->if_stat.else_body, false, is_func_body);
        }

        break;
    }

    if (!is_func_body)
    {
        ptcl_string_buffer_set_position(transpiler->string_buffer, start);
    }
}

static void ptcl_transpiler_add_argument(ptcl_transpiler *transpiler, ptcl_argument argument)
{
    if (argument.is_variadic)
    {
        ptcl_transpiler_append_word_s(transpiler, "...");
    }
    else
    {
        ptcl_transpiler_add_type(transpiler, argument.type, false);
        ptcl_transpiler_add_name(transpiler, argument.name, false);
        ptcl_transpiler_add_arrays_length_arguments(transpiler, argument.name, argument.type, 0);
    }
}

static void ptcl_transpiler_add_func_signature(ptcl_transpiler *transpiler, ptcl_statement_func_decl func_decl, ptcl_name_word name, ptcl_type *self)
{
    ptcl_transpiler_add_type(transpiler, func_decl.return_type, false);
    ptcl_transpiler_add_name(transpiler, name, true);
    ptcl_transpiler_append_character(transpiler, '(');

    bool added = false;
    if (transpiler->root != transpiler->main_root)
    {
        for (int i = 0; i < transpiler->variables_count; i++)
        {
            ptcl_transpiler_variable variable = transpiler->variables[i];
            if (variable.root == transpiler->main_root)
            {
                continue;
            }

            if (variable.is_inner)
            {
                continue;
            }

            if (i != 0)
            {
                ptcl_transpiler_append_character(transpiler, ',');
            }

            ptcl_type pointer = ptcl_type_create_pointer(&variable.type);
            ptcl_argument argument = ptcl_argument_create(pointer, variable.name);
            ptcl_transpiler_add_argument(transpiler, argument);

            bool breaked = false;
            for (int j = transpiler->variables_count - 1; j >= 0; j--)
            {
                ptcl_transpiler_variable target = transpiler->variables[j];
                if (target.root == transpiler->main_root)
                {
                    break;
                }

                if (!target.is_inner)
                {
                    continue;
                }

                if (!ptcl_name_word_compare(variable.name, target.name))
                {
                    continue;
                }

                breaked = true;
                break;
            }

            if (!breaked)
            {
                ptcl_transpiler_add_variable(transpiler, variable.name, variable.type, true, variable.root);
                added = true;
            }
        }

        if (added && func_decl.count > 0)
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }
    }

    if (self != NULL)
    {
        ptcl_argument argument = ptcl_argument_create(*self, ptcl_name_create_fast_w("self", false));
        ptcl_transpiler_add_argument(transpiler, argument);
        if (func_decl.count > 0)
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }
    }

    for (size_t i = 0; i < func_decl.count; i++)
    {
        ptcl_argument argument = func_decl.arguments[i];
        ptcl_transpiler_add_argument(transpiler, argument);

        if (i != func_decl.count - 1)
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }
    }

    ptcl_transpiler_append_character(transpiler, ')');
}

static void ptcl_transpiler_add_func_decl_body(ptcl_transpiler *transpiler, ptcl_statement_func_decl func_decl, size_t start, size_t position, size_t length, size_t previous_start)
{
    bool is_root = false;
    if (!transpiler->in_inner)
    {
        ptcl_string_buffer_set_position(transpiler->string_buffer, start);
        transpiler->in_inner = true;
        is_root = true;
    }
    else
    {
        transpiler->start = position;
        transpiler->length = length;
    }

    if (func_decl.is_prototype)
    {
        ptcl_transpiler_append_character(transpiler, ';');
    }
    else
    {
        ptcl_transpiler_add_func_body(transpiler, *func_decl.func_body, true, !is_root);
    }

    if (!is_root)
    {
        transpiler->start = previous_start;
    }
    else
    {
        transpiler->in_inner = false;
    }

    if (previous_start != -1)
    {
        const size_t current_length = ptcl_string_buffer_length(transpiler->string_buffer);
        const size_t length_before_body = transpiler->length;
        const size_t offset = current_length - length_before_body;
        ptcl_string_buffer_set_position(transpiler->string_buffer, previous_start + offset);
    }
    else
    {
        transpiler->from_position = false;
    }
}

void ptcl_transpiler_add_func_decl(ptcl_transpiler *transpiler, ptcl_statement_func_decl func_decl, ptcl_name_word name, ptcl_type *self)
{
    if (transpiler->in_inner)
    {
        transpiler->from_position = true;
        ptcl_transpiler_add_inner_func(transpiler, func_decl.name, transpiler->root);
    }

    int previous_start = -1;
    if (transpiler->start != -1)
    {
        previous_start = (int)ptcl_string_buffer_get_position(transpiler->string_buffer);
        ptcl_string_buffer_set_position(transpiler->string_buffer, transpiler->start);
    }

    const size_t start = ptcl_string_buffer_length(transpiler->string_buffer);
    const size_t position = ptcl_string_buffer_get_position(transpiler->string_buffer);
    const size_t length = ptcl_string_buffer_length(transpiler->string_buffer);
    ptcl_transpiler_add_func_signature(transpiler, func_decl, name, self);
    ptcl_transpiler_add_func_decl_body(transpiler, func_decl, start, position, length, previous_start);
}

void ptcl_transpiler_add_func_call(ptcl_transpiler *transpiler, ptcl_statement_func_call func_call)
{
    ptcl_transpiler_add_identifier(transpiler, func_call.identifier, false);
    ptcl_transpiler_append_character(transpiler, '(');

    if (ptcl_transpiler_is_inner_function(transpiler, ptcl_identifier_get_name(func_call.identifier)))
    {
        bool added = false;
        for (size_t i = 0; i < transpiler->variables_count; i++)
        {
            ptcl_transpiler_variable variable = transpiler->variables[i];
            if (variable.root == transpiler->main_root)
            {
                continue;
            }

            if (!variable.is_inner)
            {
                continue;
            }

            if (added)
            {
                ptcl_transpiler_append_character(transpiler, ',');
            }

            ptcl_transpiler_append_character(transpiler, '&');
            ptcl_transpiler_add_name(transpiler, variable.name, false);
            added = true;
        }

        if (added && func_call.count > 0)
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }
    }

    for (size_t i = 0; i < func_call.count; i++)
    {
        ptcl_expression *argument = func_call.arguments[i];
        ptcl_transpiler_add_expression(transpiler, argument, true);
        // ptcl_transpiler_add_arrays_length(transpiler, argument.return_type);

        if (i != func_call.count - 1)
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }
    }

    ptcl_transpiler_append_character(transpiler, ')');
}

void ptcl_transpiler_add_expression(ptcl_transpiler *transpiler, ptcl_expression *expression, bool specify_type)
{
    switch (expression->type)
    {
    case ptcl_expression_array_type:
        if (expression->return_type.is_static && ptcl_type_get_target(expression->return_type)->type == ptcl_value_character_type)
        {
            ptcl_transpiler_append_character(transpiler, '\"');
            // -1 because of end of line symbol
            for (size_t i = 0; i < expression->array.count - 1; i++)
            {
                ptcl_transpiler_append_character(transpiler, expression->array.expressions[i]->character);
            }

            ptcl_transpiler_append_character(transpiler, '\"');
            break;
        }

        if (specify_type)
        {
            ptcl_transpiler_append_character(transpiler, '(');
            ptcl_transpiler_add_type(transpiler, expression->return_type, false);
            ptcl_transpiler_add_array_dimensional(transpiler, expression->return_type);
            ptcl_transpiler_append_character(transpiler, ')');
        }

        ptcl_transpiler_append_character(transpiler, '{');
        for (size_t i = 0; i < expression->array.count; i++)
        {
            ptcl_transpiler_add_expression(transpiler, expression->array.expressions[i], false);

            if (i != expression->array.count - 1)
            {
                ptcl_transpiler_append_character(transpiler, ',');
            }
        }

        ptcl_transpiler_append_character(transpiler, '}');
        break;
    case ptcl_expression_variable_type:
        if (ptcl_transpiler_is_inner(transpiler, expression->variable.name))
        {
            ptcl_transpiler_append_character(transpiler, '*');
        }

        ptcl_transpiler_add_name(transpiler, expression->variable.name, false);
        break;
    case ptcl_expression_word_type:
        ptcl_transpiler_append_character(transpiler, '\"');
        ptcl_transpiler_add_name(transpiler, expression->word, false);
        ptcl_transpiler_append_character(transpiler, '\"');
        break;
    case ptcl_expression_character_type:
        ptcl_transpiler_append_character(transpiler, '\'');
        if (expression->character == '\0')
        {
            ptcl_transpiler_append_character(transpiler, '\\');
            ptcl_transpiler_append_character(transpiler, '0');
        }
        else
        {
            ptcl_transpiler_append_character(transpiler, expression->character);
        }

        ptcl_transpiler_append_character(transpiler, '\'');
        break;
    case ptcl_expression_binary_type:
        ptcl_transpiler_add_expression(transpiler, expression->binary.left, false);
        ptcl_transpiler_add_binary_type(transpiler, expression->binary.type);
        ptcl_transpiler_add_expression(transpiler, expression->binary.right, false);
        break;
    case ptcl_expression_cast_type:
        ptcl_transpiler_append_character(transpiler, '(');
        ptcl_transpiler_add_type(transpiler, expression->cast.type, false);
        ptcl_transpiler_append_character(transpiler, ')');
        ptcl_transpiler_add_expression(transpiler, expression->cast.value, false);
        break;
    case ptcl_expression_unary_type:
        ptcl_transpiler_add_binary_type(transpiler, expression->unary.type);
        ptcl_transpiler_add_expression(transpiler, expression->unary.child, false);
        break;
    case ptcl_expression_array_element_type:
        ptcl_transpiler_add_expression(transpiler, expression->array_element.value, false);
        ptcl_transpiler_append_character(transpiler, '[');
        ptcl_transpiler_add_expression(transpiler, expression->array_element.index, false);
        ptcl_transpiler_append_character(transpiler, ']');
        break;
    case ptcl_expression_double_type:
        char *double_n = ptcl_from_double(expression->double_n);
        if (double_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, double_n);
            free(double_n);
        }

        break;
    case ptcl_expression_float_type:
        char *float_n = ptcl_from_float(expression->float_n);
        if (float_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, float_n);
            free(float_n);
        }

        break;
    case ptcl_expression_integer_type:
        char *integer_n = ptcl_from_int(expression->integer_n);
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
            ptcl_transpiler_add_name(transpiler, expression->ctor.name, false);
            ptcl_transpiler_append_character(transpiler, ')');
        }

        ptcl_transpiler_append_character(transpiler, '{');
        for (size_t i = 0; i < expression->ctor.count; i++)
        {
            ptcl_transpiler_add_expression(transpiler, expression->ctor.values[i], true);
        }

        ptcl_transpiler_append_character(transpiler, '}');
        break;
    case ptcl_expression_dot_type:
        ptcl_transpiler_add_expression(transpiler, expression->dot.left, false);
        ptcl_transpiler_append_character(transpiler, '.');
        ptcl_transpiler_add_name(transpiler, expression->dot.name, false);
        break;
    case ptcl_expression_func_call_type:
        ptcl_transpiler_add_func_call(transpiler, expression->func_call);
        break;
    case ptcl_expression_null_type:
        ptcl_transpiler_append_word_s(transpiler, "NULL");
        transpiler->add_stdlib = true;
        break;
    }
}

void ptcl_transpiler_add_identifier(ptcl_transpiler *transpiler, ptcl_identifier identifier, bool is_declare)
{
    if (identifier.is_name)
    {
        ptcl_transpiler_add_name(transpiler, identifier.name, is_declare);
    }
}

void ptcl_transpiler_add_name(ptcl_transpiler *transpiler, ptcl_name_word name, bool is_declare)
{
    if (name.is_anonymous)
    {
        if (is_declare)
        {
            char *generated = ptcl_transpiler_generate_anonymous(transpiler);
            ptcl_transpiler_append_word_s(transpiler, generated);
            ptcl_transpiler_add_anonymous(transpiler, name.value, generated, transpiler->root);
        }
        else
        {
            for (size_t i = transpiler->anonymous_count - 1; i >= 0; i--)
            {
                ptcl_transpiler_anonymous anonymous = transpiler->anonymouses[i];
                if (strcmp(anonymous.original_name, name.value) != 0 || !ptcl_func_body_can_access(anonymous.root, transpiler->root))
                {
                    continue;
                }

                ptcl_transpiler_append_word_s(transpiler, anonymous.alias);
                break;
            }
        }

        return;
    }

    ptcl_transpiler_append_word_s(transpiler, name.value);
}

void ptcl_transpiler_add_type(ptcl_transpiler *transpiler, ptcl_type type, bool with_array)
{
    switch (type.type)
    {
    case ptcl_value_typedata_type:
        ptcl_transpiler_add_name(transpiler, type.typedata, false);
        break;
    case ptcl_value_array_type:
        ptcl_transpiler_add_type(transpiler, *type.array.target, with_array);

        if (with_array)
        {
            ptcl_transpiler_append_character(transpiler, '[');
            ptcl_transpiler_append_character(transpiler, ']');
        }

        break;
    case ptcl_value_pointer_type:
        if (type.pointer.is_any)
        {
            ptcl_transpiler_append_word_s(transpiler, "void*");
            break;
        }

        ptcl_transpiler_add_type(transpiler, *type.pointer.target, with_array);
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

char *ptcl_transpiler_generate_anonymous(ptcl_transpiler *transpiler)
{
    const int max_digits = 22; // Cant be more than int
    char *anonymous_name = malloc(max_digits * sizeof(char));
    if (anonymous_name == NULL)
    {
        return NULL;
    }

    snprintf(anonymous_name, max_digits, "__ptcl_t_anonymous_%d", transpiler->anonymous_count);
    return anonymous_name;
}

void ptcl_transpiler_clear_scope(ptcl_transpiler *transpiler)
{
    for (int i = transpiler->variables_count - 1; i >= 0; i--)
    {
        if (transpiler->variables[i].root != transpiler->root)
        {
            break;
        }

        transpiler->variables_count--;
    }

    for (int i = transpiler->inner_functions_count - 1; i >= 0; i--)
    {
        if (transpiler->inner_functions[i].root != transpiler->root)
        {
            break;
        }

        transpiler->inner_functions_count--;
    }
}

void ptcl_transpiler_destroy(ptcl_transpiler *transpiler)
{
    ptcl_string_buffer_destroy(transpiler->string_buffer);
    free(transpiler->anonymouses);
    free(transpiler->variables);
    free(transpiler->inner_functions);
    free(transpiler);
}
