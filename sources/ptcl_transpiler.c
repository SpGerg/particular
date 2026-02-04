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
    ptcl_func_body *root;
    ptcl_func_body *main_root;
    ptcl_transpiler_anonymous *anonymouses;
    size_t anonymous_count;
    ptcl_transpiler_replaced *replaced;
    size_t replaced_count;
    size_t temp_count;
    bool from_position;
    bool in_inner;
    bool add_stdlib;
    int start;
    size_t length;
    size_t last_stat_position;
    bool is_inserted_body;
} ptcl_transpiler;

static bool ptcl_transpiler_add_closure_arguments(ptcl_transpiler *transpiler, ptcl_identifier identifier)
{
    bool added = false;
    if (ptcl_transpiler_is_inner_function(transpiler, ptcl_identifier_get_name(identifier)))
    {
        for (size_t i = 0; i < transpiler->variables_count; i++)
        {
            ptcl_transpiler_variable variable = transpiler->variables[i];
            if (variable.root == transpiler->main_root)
            {
                continue;
            }

            if (!variable.is_inner || variable.is_self)
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
    }

    return added;
}

static void ptcl_transpiler_add_arrays_length_arguments(ptcl_transpiler *transpiler, ptcl_name name, ptcl_type type, size_t count)
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

static void ptcl_transpiler_add_variable_name(ptcl_transpiler *transpiler, ptcl_name name)
{
    ptcl_transpiler_variable *variable;
    if (strcmp(name.value, "self") == 0)
    {
        if (ptcl_transpiler_try_get_variable(transpiler, ptcl_name_create_fast_w("this", false), &variable))
        {
            if (variable->type.is_static)
            {
                ptcl_transpiler_append_word_s(transpiler, "self");
            }
            else
            {
                ptcl_transpiler_append_character(transpiler, '(');
                ptcl_transpiler_append_character(transpiler, '*');
                ptcl_transpiler_append_word_s(transpiler, "self");
                ptcl_transpiler_append_character(transpiler, ')');
            }

            return;
        }
    }

    if (ptcl_transpiler_try_get_variable(transpiler, name, &variable))
    {
        if (variable->is_self)
        {
            if (variable->type.is_static)
            {
                ptcl_transpiler_append_word_s(transpiler, "self");
            }
            else
            {
                ptcl_transpiler_append_character(transpiler, '(');
                ptcl_transpiler_append_character(transpiler, '*');
                ptcl_transpiler_append_word_s(transpiler, "self");
                ptcl_transpiler_append_character(transpiler, ')');
            }

            return;
        }
        else if (variable->is_inner)
        {
            ptcl_transpiler_append_character(transpiler, '*');
        }
    }

    ptcl_transpiler_add_name(transpiler, name, false);
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
    if (type.array.count >= 0)
    {
        char *number = ptcl_from_long(type.array.count);
        if (number != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, number);
            free(number);
        }
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
    transpiler->temp_count = 0;
    transpiler->start = -1;
    transpiler->length = 0;
    transpiler->add_stdlib = false;
    transpiler->from_position = false;
    transpiler->last_stat_position = 0;
    transpiler->is_inserted_body = false;
    transpiler->replaced = NULL;
    transpiler->replaced_count = 0;
    return transpiler;
}

char *ptcl_transpiler_transpile(ptcl_transpiler *transpiler)
{
    transpiler->main_root = &transpiler->result.body;
    ptcl_transpiler_add_func_body(transpiler, NULL, transpiler->result.body, false, false);

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

bool ptcl_transpiler_try_get_variable(ptcl_transpiler *transpiler, ptcl_name name, ptcl_transpiler_variable **variable)
{
    for (int i = transpiler->variables_count - 1; i >= 0; i--)
    {
        ptcl_transpiler_variable *target = &transpiler->variables[i];
        if (target->root == transpiler->main_root)
        {
            break;
        }

        if (!ptcl_name_compare(target->name, name))
        {
            continue;
        }

        *variable = target;
        return true;
    }

    return false;
}

bool ptcl_transpiler_is_inner(ptcl_transpiler *transpiler, ptcl_name name)
{
    ptcl_transpiler_variable *variable;
    if (!ptcl_transpiler_try_get_variable(transpiler, name, &variable))
    {
        return false;
    }

    return variable->is_inner;
}

bool ptcl_transpiler_is_inner_function(ptcl_transpiler *transpiler, ptcl_name name)
{
    for (int i = transpiler->inner_functions_count - 1; i >= 0; i--)
    {
        ptcl_transpiler_function function = transpiler->inner_functions[i];
        if (function.root == transpiler->main_root)
        {
            break;
        }

        if (!ptcl_name_compare(function.name, name))
        {
            continue;
        }

        return true;
    }

    return false;
}

bool ptcl_transpiler_add_variable(ptcl_transpiler *transpiler, ptcl_transpiler_variable variable)
{
    ptcl_transpiler_variable *buffer = realloc(transpiler->variables, (transpiler->variables_count + 1) * sizeof(ptcl_transpiler_variable));
    if (buffer == NULL)
    {
        return false;
    }

    transpiler->variables = buffer;
    transpiler->variables[transpiler->variables_count++] = variable;
    return true;
}

bool ptcl_transpiler_add_variable_f(ptcl_transpiler *transpiler, ptcl_name name, ptcl_type type, bool is_inner, ptcl_func_body *root)
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

bool ptcl_transpiler_add_inner_func(ptcl_transpiler *transpiler, ptcl_name name, ptcl_func_body *root)
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

bool ptcl_transpiler_add_anonymous(ptcl_transpiler *transpiler, char *original_name, char *alias, ptcl_func_body *root)
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

bool ptcl_transpiler_add_replaced(ptcl_transpiler *transpiler, ptcl_transpiler_replaced replaced)
{
    ptcl_transpiler_replaced *buffer = realloc(transpiler->replaced, (transpiler->replaced_count + 1) * sizeof(ptcl_transpiler_replaced));
    if (buffer == NULL)
    {
        return false;
    }

    transpiler->replaced = buffer;
    transpiler->replaced[transpiler->replaced_count++] = replaced;
    return true;
}

void ptcl_transpiler_add_func_body(ptcl_transpiler *transpiler, ptcl_statement_func_body *statement, ptcl_func_body func_body, bool with_brackets, bool is_func_body)
{
    ptcl_func_body *previous = transpiler->root;
    if (with_brackets)
    {
        func_body.root = transpiler->root;
#pragma GCC diagnostic ignored "-Wdangling-pointer="
        transpiler->root = &func_body;
    }

    if (with_brackets)
    {
        ptcl_transpiler_append_character(transpiler, '{');
    }
    
    // Inserted. TODO: isolated will conflict
    const bool is_inserted = statement != NULL && statement->func_call.func_decl != NULL;
    size_t last_count = transpiler->replaced_count;
    bool last_state = transpiler->is_inserted_body;
    if (is_inserted)
    {
        transpiler->is_inserted_body = true;
        // TODO: if it is not static, it will be not null
        if (statement->self != NULL)
        {
            ptcl_name temp = ptcl_transpiler_add_temp_variable(transpiler, statement->self);
            ptcl_transpiler_replaced replaced = ptcl_transpiler_replaced_name_create(ptcl_name_self, temp);
            ptcl_transpiler_add_replaced(transpiler, replaced);
        }

        for (size_t i = 0; i < statement->arguments_count; i++)
        {
            ptcl_name temp = ptcl_transpiler_add_temp_variable(transpiler, statement->func_call.arguments[i]);
            ptcl_transpiler_replaced replaced = ptcl_transpiler_replaced_name_create(statement->arguments[i].name, temp);
            ptcl_transpiler_add_replaced(transpiler, replaced);
        }
    }

    for (size_t i = 0; i < func_body.count; i++)
    {
        ptcl_transpiler_add_statement(transpiler, func_body.statements[i], is_func_body);
    }

    if (is_inserted)
    {
        transpiler->is_inserted_body = last_state;
        for (size_t i = last_count; i < transpiler->replaced_count; i++)
        {
            ptcl_name_destroy(transpiler->replaced[i].replaced_name);
        }

        transpiler->replaced_count = last_count;
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
    transpiler->from_position = transpiler->in_inner ? transpiler->from_position : false;
    transpiler->last_stat_position = ptcl_string_buffer_get_position(transpiler->string_buffer);

    switch (statement->type)
    {
    case ptcl_statement_func_body_type:
        ptcl_transpiler_add_func_body(transpiler, &statement->body, statement->body.body, false, false);
        break;
    case ptcl_statement_func_call_type:
        if (ptcl_transpiler_add_func_call(transpiler, statement->func_call))
        {
            ptcl_transpiler_append_character(transpiler, ';');
        }

        break;
    case ptcl_statement_assign_type:
        if (statement->assign.type.is_static)
        {
            break;
        }

        ptcl_transpiler_variable *variable = NULL;
        ptcl_name name = ptcl_identifier_get_name(statement->assign.identifier);
        if (ptcl_transpiler_try_get_variable(transpiler, name, &variable))
        {
            if (variable->is_self)
            {
                if (!variable->type.is_static)
                {
                    ptcl_transpiler_append_character(transpiler, '(');
                    ptcl_transpiler_append_character(transpiler, '*');
                }

                ptcl_transpiler_append_word_s(transpiler, "self");
                if (!variable->type.is_static)
                {
                    ptcl_transpiler_append_character(transpiler, ')');
                }
            }
            else if (variable->is_inner)
            {
                ptcl_transpiler_append_character(transpiler, '*');
            }
        }

        if (statement->assign.is_define)
        {
            if (transpiler->is_inserted_body)
            {
                ptcl_name anonymous = ptcl_name_create_l(ptcl_transpiler_generate_temp_and_add(transpiler), false, true, (ptcl_location){0});
                if (anonymous.value != NULL)
                {
                    ptcl_transpiler_add_type_and_name(transpiler, statement->assign.type, anonymous, NULL, false, true);
                    ptcl_transpiler_add_replaced(transpiler, ptcl_transpiler_replaced_name_create(statement->assign.identifier.name, anonymous));
                }
            }
            else
            {
                char *name = ptcl_transpiler_add_type_and_name(transpiler, statement->assign.type, statement->assign.identifier.name, NULL, false, true);
                ptcl_name generated_name = name == NULL ? statement->assign.identifier.name : ptcl_name_create_l(name, true, false, statement->assign.identifier.name.location);
                // ptcl_transpiler_add_array_dimensional(transpiler, statement->assign.type);
                ptcl_transpiler_add_variable_f(
                    transpiler, generated_name, statement->assign.type, false, transpiler->root);
            }
        }
        else
        {
            if (variable == NULL)
            {
                ptcl_transpiler_add_identifier(transpiler, statement->assign.identifier, false);
            }
            else
            {
                if (!statement->assign.identifier.is_name)
                {
                    ptcl_transpiler_add_expression(transpiler, statement->assign.identifier.value, false);
                }
                else
                {
                    ptcl_transpiler_add_identifier(transpiler, statement->assign.identifier, false);
                }
            }
        }

        ptcl_transpiler_append_character(transpiler, '=');
        ptcl_transpiler_add_expression(transpiler, statement->assign.value, false);
        ptcl_transpiler_append_character(transpiler, ';');
        break;
    case ptcl_statement_func_decl_type:
        ptcl_transpiler_add_func_decl(transpiler, statement->func_decl, statement->func_decl.name, NULL);
        break;
    case ptcl_statement_type_decl_type:
    {
        ptcl_type base_type = statement->type_decl.types[0].type;
        if (statement->type_decl.functions != NULL)
        {
            for (size_t i = 0; i < statement->type_decl.functions->count; i++)
            {
                ptcl_statement_func_decl function = statement->type_decl.functions->statements[i]->func_decl;
                char *name = ptcl_transpiler_get_func_name_in_type(statement->type_decl.name.value, function.name.value, base_type.is_static);
                ptcl_transpiler_add_func_decl(transpiler, function, ptcl_name_create_fast_w(name, false), &base_type);
                free(name);
            }
        }

        break;
    }
    case ptcl_statement_typedata_decl_type:
    {
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

        const size_t length = ptcl_string_buffer_length(transpiler->string_buffer);
        ptcl_transpiler_append_word_s(transpiler, "typedef struct");
        ptcl_transpiler_add_name(transpiler, statement->typedata_decl.name, true);
        ptcl_transpiler_append_character(transpiler, '{');
        for (size_t i = 0; i < statement->typedata_decl.count; i++)
        {
            ptcl_argument member = statement->typedata_decl.members[i];
            ptcl_transpiler_add_type_and_name(transpiler, member.type, member.name, NULL, false, false);
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
    }
    case ptcl_statement_return_type:
        ptcl_transpiler_append_word_s(transpiler, "return");
        if (statement->ret.value != NULL)
        {
            ptcl_transpiler_add_expression(transpiler, statement->ret.value, false);
        }

        ptcl_transpiler_append_character(transpiler, ';');
        break;
    case ptcl_statement_if_type:
        ptcl_transpiler_append_word_s(transpiler, "if");
        ptcl_transpiler_append_character(transpiler, '(');
        ptcl_transpiler_add_expression(transpiler, statement->if_stat.condition, false);
        ptcl_transpiler_append_character(transpiler, ')');
        ptcl_transpiler_add_func_body(transpiler, NULL, statement->if_stat.body, true, is_func_body);
        if (statement->if_stat.with_else)
        {
            ptcl_transpiler_append_word_s(transpiler, "else");
            ptcl_transpiler_add_func_body(transpiler, NULL, statement->if_stat.else_body, true, is_func_body);
        }

        break;
    case ptcl_statement_each_type:
    case ptcl_statement_syntax_type:
    case ptcl_statement_unsyntax_type:
    case ptcl_statement_undefine_type:
    case ptcl_statement_import_type:
    case ptcl_statement_none_type:
        break;
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
        ptcl_transpiler_add_type_and_name(transpiler, argument.type, argument.name, NULL, false, false);
        // ptcl_transpiler_add_arrays_length_arguments(transpiler, argument.name, argument.type, 0);
    }
}

static void ptcl_transpiler_add_func_signature(ptcl_transpiler *transpiler, ptcl_statement_func_decl func_decl, ptcl_name name, ptcl_type *self)
{
    ptcl_transpiler_add_type_and_name(transpiler, func_decl.return_type, ptcl_name_create_fast_w(NULL, false), &func_decl, false, true);
    ptcl_transpiler_add_name(transpiler, name, true);
    ptcl_transpiler_append_character(transpiler, '(');

    bool added = false;
    if (transpiler->root != transpiler->main_root)
    {
        for (size_t i = 0; i < transpiler->variables_count; i++)
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

            if (added)
            {
                ptcl_transpiler_append_character(transpiler, ',');
            }

            ptcl_type pointer = ptcl_type_create_pointer(&variable.type, false);
            ptcl_argument argument = ptcl_argument_create(pointer, variable.name);
            ptcl_transpiler_add_argument(transpiler, argument);
            added = true;

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

                if (!ptcl_name_compare(variable.name, target.name))
                {
                    continue;
                }

                breaked = true;
                break;
            }

            if (!breaked)
            {
                ptcl_transpiler_add_variable_f(transpiler, variable.name, variable.type, true, variable.root);
            }
        }

        if (added && (func_decl.count > 0 || self != NULL))
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }
    }

    if (self != NULL)
    {
        ptcl_type pointer = self->is_static ? *self : ptcl_type_create_pointer(self, false);
        pointer.is_const = func_decl.is_self_const;
        ptcl_argument argument = ptcl_argument_create(pointer, ptcl_name_create_fast_w("self", false));
        ptcl_transpiler_add_argument(transpiler, argument);
        if (func_decl.count > 0)
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }

        ptcl_transpiler_variable this = ptcl_transpiler_variable_create_this(*self, func_decl.func_body);
        ptcl_transpiler_add_variable(transpiler, this);
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
    ptcl_type_functon_pointer_type placeholder;
    // TODO: avoid double calling here and in type_name
    if (ptcl_type_is_function(func_decl.return_type, &placeholder))
    {
        ptcl_transpiler_append_character(transpiler, ')');
        ptcl_type_functon_pointer_type function;
        ptcl_type_is_function(func_decl.return_type, &function);
        ptcl_transpiler_add_func_type_args(transpiler, function);
    }
}

static void ptcl_transpiler_add_func_decl_body(ptcl_transpiler *transpiler, ptcl_statement_func_decl func_decl, size_t start, size_t position, size_t length, size_t previous_start)
{
    bool is_root = false;
    const size_t original_buffer_pos = ptcl_string_buffer_get_position(transpiler->string_buffer);
    const bool original_in_inner = transpiler->in_inner;
    const size_t original_start = transpiler->start;
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

    if (ptcl_statement_modifiers_flags_prototype(func_decl.modifiers))
    {
        ptcl_transpiler_append_character(transpiler, ';');
    }
    else
    {
        ptcl_transpiler_add_func_body(transpiler, NULL, *func_decl.func_body, true, !is_root);
    }

    if (!is_root)
    {
        transpiler->start = original_start;
    }
    else
    {
        transpiler->in_inner = original_in_inner;
        ptcl_string_buffer_set_position(transpiler->string_buffer, original_buffer_pos);
    }

    if (previous_start != (size_t)-1)
    {
        const size_t current_length = ptcl_string_buffer_length(transpiler->string_buffer);
        const size_t offset = current_length - length;
        ptcl_string_buffer_set_position(transpiler->string_buffer, previous_start + offset);
    }
    else
    {
        transpiler->from_position = false;
    }
}

void ptcl_transpiler_add_func_decl(ptcl_transpiler *transpiler, ptcl_statement_func_decl func_decl, ptcl_name name, ptcl_type *self)
{
    if (func_decl.return_type.is_static)
    {
        return;
    }

    const size_t original_buffer_pos = ptcl_string_buffer_get_position(transpiler->string_buffer);
    if (transpiler->in_inner)
    {
        transpiler->from_position = true;
        ptcl_transpiler_add_inner_func(transpiler, func_decl.name, transpiler->root);
    }

    int previous_start = -1;
    if (transpiler->start != -1)
    {
        previous_start = (int)original_buffer_pos;
        ptcl_string_buffer_set_position(transpiler->string_buffer, transpiler->start);
    }

    const size_t start = ptcl_string_buffer_length(transpiler->string_buffer);
    const size_t position = ptcl_string_buffer_get_position(transpiler->string_buffer);
    const size_t length = ptcl_string_buffer_length(transpiler->string_buffer);
    ptcl_transpiler_add_func_signature(transpiler, func_decl, name, self);
    ptcl_transpiler_add_func_decl_body(transpiler, func_decl, start, position, length, previous_start);
}

bool ptcl_transpiler_add_func_call(ptcl_transpiler *transpiler, ptcl_statement_func_call func_call)
{
    if (func_call.identifier.is_name && strcmp(func_call.identifier.name.value, PTCL_PARSER_ERROR_FUNC_NAME) == 0)
    {
        return false;
    }

    if (func_call.identifier.is_name)
    {
        ptcl_transpiler_add_variable_name(transpiler, func_call.identifier.name);
    }
    else
    {
        ptcl_transpiler_add_identifier(transpiler, func_call.identifier, false);
        return true;
    }

    ptcl_transpiler_append_character(transpiler, '(');
    if (ptcl_transpiler_add_closure_arguments(transpiler, func_call.identifier) && func_call.count > 0)
    {
        ptcl_transpiler_append_character(transpiler, ',');
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
    return true;
}

static void ptcl_transpiler_add_dot_expression(ptcl_transpiler *transpiler, ptcl_expression *expression)
{
    if (expression->type != ptcl_expression_dot_type)
    {
        ptcl_transpiler_add_expression(transpiler, expression, false);
        return;
    }

    bool is_special_func_call = expression->dot.left->return_type.type == ptcl_value_type_type;
    if (is_special_func_call)
    {
        ptcl_statement_func_call func_call = expression->dot.right->func_call;
        char *name = ptcl_transpiler_get_func_name_in_type(
            expression->dot.left->return_type.comp_type->identifier.value,
            func_call.identifier.name.value, expression->dot.left->return_type.is_static);

        ptcl_transpiler_append_word_s(transpiler, name);
        ptcl_transpiler_append_character(transpiler, '(');
        if (ptcl_transpiler_add_closure_arguments(transpiler, func_call.identifier))
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }

        if (!expression->dot.left->return_type.is_static)
        {
            ptcl_transpiler_append_character(transpiler, '&');
        }

        if (expression->dot.left->type == ptcl_expression_variable_type)
        {
            ptcl_transpiler_add_dot_expression(transpiler, expression->dot.left);
        }
        else
        {
            ptcl_name anonymous = ptcl_transpiler_add_temp_variable(transpiler, expression->dot.left);
            if (anonymous.value != NULL)
            {
                ptcl_transpiler_add_name(transpiler, anonymous, false);
            }

            free(anonymous.value);
        }

        if (func_call.count > 0)
        {
            ptcl_transpiler_append_character(transpiler, ',');
            for (size_t i = 0; i < func_call.count; i++)
            {
                ptcl_transpiler_add_expression(transpiler, func_call.arguments[i], false);
                if (i != func_call.count - 1)
                {
                    ptcl_transpiler_append_character(transpiler, ',');
                }
            }
        }

        ptcl_transpiler_append_character(transpiler, ')');
        free(name);
    }
    else
    {
        ptcl_transpiler_add_dot_expression(transpiler, expression->dot.left);
        ptcl_transpiler_append_character(transpiler, '.');
        ptcl_transpiler_add_name(transpiler, expression->dot.name, false);
    }
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
            ptcl_transpiler_add_type_and_name(transpiler, expression->return_type, ptcl_name_create_fast_w(NULL, false), NULL, false, false);
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
    case ptcl_expression_if_type:
        ptcl_transpiler_add_expression(transpiler, expression->if_expr.condition, false);
        ptcl_transpiler_append_character(transpiler, '?');
        ptcl_transpiler_add_expression(transpiler, expression->if_expr.body, false);
        ptcl_transpiler_append_character(transpiler, ':');
        ptcl_transpiler_add_expression(transpiler, expression->if_expr.else_body, false);
        break;
    case ptcl_expression_variable_type:
    {
        ptcl_name name = expression->variable.name;
        ptcl_transpiler_replaced replaced = {0};
        if (!expression->variable.is_syntax_variable && ptcl_transpiler_try_get_replaced(transpiler, name, &replaced))
        {
            ptcl_transpiler_add_variable_name(transpiler, replaced.replaced_name);
        }
        else
        {
            ptcl_transpiler_add_variable_name(transpiler, name);
        }

        break;
    }
    case ptcl_expression_word_type:
        ptcl_transpiler_append_character(transpiler, '\"');
        ptcl_transpiler_append_word(transpiler, expression->word.value);
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
        ptcl_transpiler_add_type_and_name(transpiler, expression->cast.type, ptcl_name_empty, NULL, false, false);
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
    {
        char *double_n = ptcl_from_double(expression->double_n);
        if (double_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, double_n);
            free(double_n);
        }

        break;
    }
    case ptcl_expression_float_type:
    {
        char *float_n = ptcl_from_float(expression->float_n);
        if (float_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, float_n);
            free(float_n);
        }

        break;
    }
    case ptcl_expression_integer_type:
    {
        char *integer_n = ptcl_from_int(expression->integer_n);
        if (integer_n != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, integer_n);
            free(integer_n);
        }

        break;
    }
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
            if (i != expression->ctor.count - 1)
            {
                ptcl_transpiler_append_character(transpiler, ',');
            }
        }

        ptcl_transpiler_append_character(transpiler, '}');
        break;
    case ptcl_expression_dot_type:
        ptcl_transpiler_add_dot_expression(transpiler, expression);
        break;
    case ptcl_expression_func_call_type:
        ptcl_transpiler_add_func_call(transpiler, expression->func_call);
        break;
    case ptcl_expression_null_type:
        ptcl_transpiler_append_word_s(transpiler, "NULL");
        transpiler->add_stdlib = true;
        break;
    case ptcl_expression_object_type_type:
    case ptcl_expression_lated_func_body_type:
    case ptcl_expression_in_statement_type:
    case ptcl_expression_in_expression_type:
    case ptcl_expression_in_token_type:
        break;
    }
}

void ptcl_transpiler_add_identifier(ptcl_transpiler *transpiler, ptcl_identifier identifier, bool is_declare)
{
    if (identifier.is_name)
    {
        ptcl_transpiler_add_name(transpiler, identifier.name, is_declare);
    }
    else
    {
        ptcl_transpiler_add_expression(transpiler, identifier.value, false);
    }
}

char *ptcl_transpiler_add_name(ptcl_transpiler *transpiler, ptcl_name name, bool is_declare)
{
    if (name.is_anonymous)
    {
        if (is_declare)
        {
            char *generated = ptcl_transpiler_generate_anonymous(transpiler);
            ptcl_transpiler_append_word_s(transpiler, generated);
            ptcl_transpiler_add_anonymous(transpiler, name.value, generated, transpiler->root);
            return generated;
        }
        else
        {
            for (int i = transpiler->anonymous_count - 1; i >= 0; i--)
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
    }
    else
    {
        ptcl_transpiler_append_word_s(transpiler, name.value);
    }

    return NULL;
}

void ptcl_transpiler_add_func_type_args(ptcl_transpiler *transpiler, ptcl_type_functon_pointer_type type)
{
    ptcl_transpiler_append_character(transpiler, '(');
    for (size_t i = 0; i < type.count; i++)
    {
        ptcl_argument argument = type.arguments[i];
        if (argument.is_variadic)
        {
            ptcl_transpiler_append_word_s(transpiler, "...");
        }
        else
        {
            ptcl_transpiler_add_type_and_name(transpiler, argument.type, ptcl_name_create_fast_w(NULL, false), NULL, true, false);
        }

        if (i != type.count - 1)
        {
            ptcl_transpiler_append_character(transpiler, ',');
        }
    }

    ptcl_transpiler_append_character(transpiler, ')');
}

char *ptcl_transpiler_add_type_and_name(ptcl_transpiler *transpiler, ptcl_type type, ptcl_name name, ptcl_statement_func_decl *func_decl, bool with_array, bool is_define)
{
    ptcl_type_functon_pointer_type function;
    if (ptcl_type_is_function(type, &function))
    {
        ptcl_name empty = ptcl_name_create_fast_w(NULL, false);
        char *func = ptcl_transpiler_add_type_and_name(transpiler, *function.return_type, empty, NULL, true, is_define);
        ptcl_transpiler_append_character(transpiler, '(');
        ptcl_transpiler_append_character(transpiler, '*');
        ptcl_type next = type;
        while (next.type == ptcl_value_pointer_type || next.type == ptcl_value_array_type)
        {
            if (next.type == ptcl_value_pointer_type)
            {
                ptcl_transpiler_append_character(transpiler, '*');
                next = *type.pointer.target;
            }
            else
            {
                ptcl_transpiler_append_character(transpiler, '[');
                ptcl_transpiler_append_character(transpiler, ']');
                next = *type.array.target;
            }
        }

        if (func_decl == NULL)
        {
            if (name.value == NULL)
            {
                ptcl_transpiler_append_character(transpiler, '*');
            }
            else
            {
                ptcl_transpiler_add_name(transpiler, name, false);
            }

            ptcl_transpiler_append_character(transpiler, ')');
            ptcl_transpiler_add_func_type_args(transpiler, function);
        }

        return func;
    }

    if (type.is_const)
    {
        ptcl_transpiler_append_word_s(transpiler, "const");
    }

    switch (type.type)
    {
    case ptcl_value_typedata_type:
        ptcl_transpiler_add_name(transpiler, type.typedata, false);
        break;
    case ptcl_value_type_type:
        return ptcl_transpiler_add_type_and_name(transpiler, type.comp_type->types[0].type, name, NULL, false, is_define);
    case ptcl_value_array_type:
    {
        char *array = ptcl_transpiler_add_type_and_name(transpiler, *type.array.target, ptcl_name_create_fast_w(NULL, false), NULL, with_array, is_define);
        if (with_array)
        {
            ptcl_transpiler_append_character(transpiler, '[');
            ptcl_transpiler_append_character(transpiler, ']');
        }
        else
        {
            ptcl_transpiler_append_character(transpiler, '*');
        }

        if (name.value != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, name.value);
        }

        return array;
    }
    case ptcl_value_pointer_type:
        if (type.pointer.is_any || type.pointer.is_null)
        {
            ptcl_transpiler_append_word_s(transpiler, "void*");
            break;
        }

        char *pointer = ptcl_transpiler_add_type_and_name(transpiler, *type.pointer.target, ptcl_name_create_fast_w(NULL, false), NULL, with_array, is_define);
        ptcl_transpiler_append_character(transpiler, '*');
        if (type.is_const)
        {
            ptcl_transpiler_append_word_s(transpiler, "const");
        }

        if (name.value != NULL)
        {
            ptcl_transpiler_append_word_s(transpiler, name.value);
        }

        return pointer;
    case ptcl_value_any_type:
        ptcl_transpiler_append_word_s(transpiler, "void*");
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
    case ptcl_value_function_pointer_type:
    case ptcl_value_object_type_type:
        break;
    }

    if (name.value != NULL &&
        (type.type == ptcl_value_typedata_type ||
         type.type == ptcl_value_pointer_type ||
         type.type == ptcl_value_any_type ||
         (ptcl_type_is_primitive(type.type))))
    {
        return ptcl_transpiler_add_name(transpiler, name, is_define);
    }

    return NULL;
}

void ptcl_transpiler_add_binary_type(ptcl_transpiler *transpiler, ptcl_binary_operator_type type)
{
    char *value = "";
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
    case ptcl_binary_operator_type_equals_type:
    case ptcl_binary_operator_none_type:
        break;
    }

    ptcl_transpiler_append_word_s(transpiler, value);
}

ptcl_name ptcl_transpiler_add_temp_variable(ptcl_transpiler *transpiler, ptcl_expression *expression)
{
    // TODO: sometimes doesn't work
    const size_t length = ptcl_string_buffer_length(transpiler->string_buffer);
    int previous_start = -1;
    if (transpiler->start != -1)
    {
        previous_start = (int)ptcl_string_buffer_get_position(transpiler->string_buffer);
        ptcl_string_buffer_set_position(transpiler->string_buffer, transpiler->start);
    }

    const bool last_state = transpiler->from_position;
    transpiler->from_position = true;

    ptcl_string_buffer_set_position(transpiler->string_buffer, transpiler->last_stat_position);
    ptcl_name anonymous = ptcl_name_create_l(ptcl_transpiler_generate_temp_and_add(transpiler), false, true, (ptcl_location){0});
    if (anonymous.value != NULL)
    {
        ptcl_transpiler_add_type_and_name(transpiler, expression->return_type, anonymous, NULL, false, true);
    }

    ptcl_transpiler_append_character(transpiler, '=');
    ptcl_transpiler_add_expression(transpiler, expression, false);
    ptcl_transpiler_append_character(transpiler, ';');
    if (previous_start != -1)
    {
        const size_t current_length = ptcl_string_buffer_length(transpiler->string_buffer);
        const size_t length_before_body = length;
        const size_t offset = current_length - length_before_body;
        ptcl_string_buffer_set_position(transpiler->string_buffer, previous_start + offset);
        transpiler->start += offset;
    }

    transpiler->from_position = last_state;
    return anonymous;
}

char *ptcl_transpiler_generate_anonymous(ptcl_transpiler *transpiler)
{
    const int prefix_length = strlen(PTCL_TRANSPILER_ANONYMOUS_PREFIX); // Length of "__ptcl_t_anonymous_"
    const size_t length = prefix_length + PTCL_TRANSPILER_SIZE_T_MAX_DIGITS;
    char *anonymous_name = malloc(length + 1);
    if (anonymous_name == NULL)
    {
        return NULL;
    }

    snprintf(anonymous_name, sizeof(char) * (length + 1), PTCL_TRANSPILER_ANONYMOUS_PREFIX "%zu", transpiler->anonymous_count);
    return anonymous_name;
}

char *ptcl_transpiler_generate_temp_and_add(ptcl_transpiler *transpiler)
{
    const int prefix_length = strlen(PTCL_TRANSPILER_TEMP_PREFIX); // Length of "__ptcl_t_temp_"
    const size_t length = prefix_length + PTCL_TRANSPILER_SIZE_T_MAX_DIGITS;
    char *temp_name = malloc(length + 1);
    if (temp_name == NULL)
    {
        return NULL;
    }

    snprintf(temp_name, sizeof(char) * (length + 1), PTCL_TRANSPILER_TEMP_PREFIX "%zu", transpiler->temp_count++);
    return temp_name;
}

char *ptcl_transpiler_get_func_name_in_type(char *type, char *function, bool is_static)
{
    return ptcl_string("ptcl_t_", is_static ? "st_" : "", type, "_", function, NULL);
}

bool ptcl_transpiler_try_get_replaced(ptcl_transpiler *transpiler, ptcl_name name, ptcl_transpiler_replaced *replaced)
{
    for (int i = transpiler->replaced_count - 1; i >= 0; i--)
    {
        *replaced = transpiler->replaced[i];
        if (!ptcl_name_compare(replaced->name, name))
        {
            continue;
        }

        return true;
    }

    return false;
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
    free(transpiler->replaced);
    free(transpiler);
}
