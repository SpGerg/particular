#ifndef PTCL_TOKEN_H
#define PTCL_TOKEN_H

#include <stdlib.h>
#include <stdbool.h>
#include <ptcl_string.h>

typedef enum ptcl_token_type
{
    ptcl_token_word_type,
    ptcl_token_string_type,
    ptcl_token_character_type,
    ptcl_token_number_type,
    ptcl_token_return_type,
    ptcl_token_static_type,
    ptcl_token_pointer_type,
    ptcl_token_double_type,
    ptcl_token_float_type,
    ptcl_token_integer_type,
    ptcl_token_character_word_type,
    ptcl_token_word_word_type,
    ptcl_token_each_type,
    ptcl_token_syntax_type,
    ptcl_token_new_type,
    ptcl_token_typedata_type,
    ptcl_token_function_type,
    ptcl_token_prototype_type,
    ptcl_token_void_type,
    ptcl_token_if_type,
    ptcl_token_else_type,
    ptcl_token_not_type,
    ptcl_token_and_type,
    ptcl_token_or_type,
    ptcl_token_left_par_type,
    ptcl_token_right_par_type,
    ptcl_token_left_curly_type,
    ptcl_token_right_curly_type,
    ptcl_token_left_square_type,
    ptcl_token_right_square_type,
    ptcl_token_plus_type,
    ptcl_token_minus_type,
    ptcl_token_slash_type,
    ptcl_token_greater_than_type,
    ptcl_token_less_than_type,
    ptcl_token_exclamation_mark_type,
    ptcl_token_equals_type,
    ptcl_token_dot_type,
    ptcl_token_asterisk_type,
    ptcl_token_ampersand_type,
    ptcl_token_semicolon_type,
    ptcl_token_colon_type,
    ptcl_token_comma_type,
    ptcl_token_elipsis_type
} ptcl_token_type;

typedef struct ptcl_location
{
    char *executor;
    size_t position;
} ptcl_location;

typedef struct ptcl_token
{
    ptcl_token_type type;
    char *value;
    ptcl_location location;
    bool is_free_value;
} ptcl_token;

typedef struct ptcl_tokens_list
{
    char *source;
    char *executor;
    ptcl_token *tokens;
    size_t count;
} ptcl_tokens_list;

static ptcl_location ptcl_location_create(char *executor, size_t position)
{
    return (ptcl_location){
        .executor = executor,
        .position = position};
}

static ptcl_token ptcl_token_create(ptcl_token_type type, char *value, ptcl_location location, bool is_free_value)
{
    return (ptcl_token){
        .type = type,
        .value = value,
        .location = location,
        .is_free_value = is_free_value};
}

static ptcl_token ptcl_token_same(ptcl_token token)
{
    return (ptcl_token){
        .type = token.type,
        .value = token.value,
        .location = token.location,
        .is_free_value = false};
}

static void ptcl_token_destroy(ptcl_token token)
{
    if (token.is_free_value)
    {
        free(token.value);
    }
}

static void ptcl_tokens_list_destroy(ptcl_tokens_list tokens_list)
{
    for (size_t i = 0; i < tokens_list.count; i++)
    {
        ptcl_token token = tokens_list.tokens[i];
        ptcl_token_destroy(token);
    }

    free(tokens_list.tokens);
}

static inline bool ptcl_token_type_is_value(ptcl_token_type type)
{
    switch (type)
    {
    case ptcl_token_string_type:
        return true;
    case ptcl_token_number_type:
        return true;
    case ptcl_token_character_type:
        return true;
    default:
        return false;
    }
}

#endif // PTCL_TOKEN_H
