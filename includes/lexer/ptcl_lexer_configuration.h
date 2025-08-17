#ifndef PTCL_LEXER_CONFIGURATION_H
#define PTCL_LEXER_CONFIGURATION_H

#include <string.h>
#include <ptcl_token.h>

#define PTCL_LEXER_CONFIGURATION_TOKENS_COUNT 36
#define PTCL_LEXER_CONFIGURATION_ADD_TOKEN(type, value) ptcl_lexer_configuration_add_token(configuration_pointer, type, value); // internal

typedef struct ptcl_lexer_token_config
{
    ptcl_token_type type;
    char *value;
} ptcl_lexer_token_config;

typedef struct ptcl_lexer_configuration
{
    ptcl_lexer_token_config tokens[PTCL_LEXER_CONFIGURATION_TOKENS_COUNT];
    size_t count;
} ptcl_lexer_configuration;

static ptcl_lexer_token_config ptcl_lexer_configuration_create_token(ptcl_token_type type, char *value)
{
    return (ptcl_lexer_token_config){.type = type, .value = value};
}

static void ptcl_lexer_configuration_add_token(ptcl_lexer_configuration *configuration, ptcl_token_type type, char *value)
{
    if (configuration->count >= PTCL_LEXER_CONFIGURATION_TOKENS_COUNT)
    {
        return;
    }

    configuration->tokens[configuration->count++] = ptcl_lexer_configuration_create_token(type, value);
}

static ptcl_lexer_configuration ptcl_lexer_configuration_default()
{
    ptcl_lexer_configuration configuration = {.count = 0};
    ptcl_lexer_configuration *configuration_pointer = &configuration;

    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_typedata_type, "typedata");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_function_type, "function");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_prototype_type, "prototype");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_return_type, "return");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_each_type, "each");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_syntax_type, "syntax");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_static_type, "static");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_character_word_type, "character");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_word_word_type, "word");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_double_type, "double");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_float_type, "float");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_integer_type, "integer");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_void_type, "void");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_if_type, "if");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_else_type, "else");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_left_par_type, "(");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_right_par_type, ")");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_left_curly_type, "{");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_right_curly_type, "}");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_left_square_type, "[");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_right_square_type, "]");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_plus_type, "+");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_minus_type, "-");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_slash_type, "/");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_greater_than_type, ">");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_less_than_type, "<");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_exclamation_mark_type, "!");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_equals_type, "=");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_dot_type, ".");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_asterisk_type, "*");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_ampersand_type, "&");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_semicolon_type, ";");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_colon_type, ":");
    PTCL_LEXER_CONFIGURATION_ADD_TOKEN(ptcl_token_comma_type, ",");

    return configuration;
}

static bool ptcl_lexer_configuration_try_get_token(ptcl_lexer_configuration *configuration, char *name, ptcl_token_type *type)
{
    for (size_t i = 0; i < configuration->count; i++)
    {
        ptcl_lexer_token_config token = configuration->tokens[i];

        if (strcmp(token.value, name) != 0)
        {
            continue;
        }

        *type = token.type;
        return true;
    }

    return false;
}

static bool ptcl_lexer_configuration_try_get_token_char(ptcl_lexer_configuration *configuration, char name, ptcl_token_type *type)
{
    for (size_t i = 0; i < configuration->count; i++)
    {
        ptcl_lexer_token_config token = configuration->tokens[i];

        if (token.value == NULL || token.value[0] != name || token.value[1] != '\0')
        {
            continue;
        }

        *type = token.type;
        return true;
    }

    return false;
}

static char *ptcl_lexer_configuration_get_value(ptcl_lexer_configuration *configuration, ptcl_token_type type)
{
    for (size_t i = 0; i < configuration->count; i++)
    {
        ptcl_lexer_token_config token = configuration->tokens[i];

        if (token.type != type)
        {
            continue;
        }

        return token.value;
    }

    return NULL;
}

#endif // PTCL_LEXER_CONFIGURATION_H