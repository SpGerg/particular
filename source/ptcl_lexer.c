#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <ptcl_lexer.h>
#include <ptcl_string_buffer.h>

typedef struct ptcl_lexer
{
    char *source;
    size_t length;
    char *executor;
    ptcl_token *tokens;
    size_t capacity;
    ptcl_string_buffer *buffer;
    ptcl_lexer_configuration *configuration;
    size_t position;
} ptcl_lexer;

static ptcl_location ptcl_lexer_create_location(ptcl_lexer *lexer)
{
    return ptcl_location_create(lexer->executor, lexer->position);
}

ptcl_lexer *ptcl_lexer_create(char *executor, char *source, ptcl_lexer_configuration *configuration)
{
    ptcl_lexer *lexer = malloc(sizeof(ptcl_lexer));
    if (lexer == NULL)
    {
        return NULL;
    }

    lexer->buffer = ptcl_string_buffer_create();
    if (lexer->buffer == NULL)
    {
        free(lexer);

        return NULL;
    }

    lexer->source = source;
    lexer->length = strlen(source);
    lexer->executor = executor;
    lexer->configuration = configuration;
    lexer->position = 0;

    return lexer;
}

char ptcl_lexer_current(ptcl_lexer *lexer)
{
    return lexer->source[lexer->position];
}

void ptcl_lexer_skip(ptcl_lexer *lexer)
{
    lexer->position++;
}

bool ptcl_lexer_not_ended(ptcl_lexer *lexer)
{
    return lexer->position < lexer->length;
}

void ptcl_lexer_add_token_by_str(ptcl_lexer *lexer, char *value, bool check_token)
{
    if (strcmp(value, "") == 0)
    {
        free(value);

        return;
    }

    ptcl_token_type type = ptcl_token_word_type;
    if (check_token)
    {
        ptcl_lexer_configuration_try_get_token(lexer->configuration, value, &type);
    }

    ptcl_lexer_add_token_string(lexer, type, value, true);
}

void ptcl_lexer_add_buffer(ptcl_lexer *lexer, bool check_token)
{
    char *value = ptcl_string_buffer_copy_and_clear(lexer->buffer);
    ptcl_lexer_add_token_by_str(lexer, value, check_token);
}

bool ptcl_lexer_add_token(ptcl_lexer *lexer, ptcl_token token)
{
    ptcl_token *buffer = realloc(lexer->tokens, (lexer->capacity + 1) * sizeof(ptcl_token));
    if (buffer == NULL)
    {
        return false;
    }

    lexer->tokens = buffer;
    lexer->tokens[lexer->capacity++] = token;
    return true;
}

bool ptcl_lexer_add_token_string(ptcl_lexer *lexer, ptcl_token_type type, char *value, bool is_free_name)
{
    ptcl_token token = ptcl_token_create(type, value, ptcl_lexer_create_location(lexer), is_free_name);
    return ptcl_lexer_add_token(lexer, token);
}

bool ptcl_lexer_add_token_char(ptcl_lexer *lexer, ptcl_token_type type, char value)
{
    char *buffer = malloc(sizeof(char) * 2);
    if (buffer == NULL)
    {
        return false;
    }

    buffer[0] = value;
    buffer[1] = '\0';
    ptcl_token token = ptcl_token_create(type, buffer, ptcl_lexer_create_location(lexer), true);

    if (!ptcl_lexer_add_token(lexer, token))
    {
        free(buffer);
        return false;
    }

    return true;
}

ptcl_tokens_list ptcl_lexer_tokenize(ptcl_lexer *lexer)
{
    lexer->tokens = NULL;
    lexer->capacity = 0;

    while (ptcl_lexer_not_ended(lexer))
    {
        char current = ptcl_lexer_current(lexer);

        if (iscntrl(current))
        {
            ptcl_lexer_add_buffer(lexer, true);
            ptcl_lexer_skip(lexer);

            continue;
        }

        if (ptcl_string_buffer_is_empty(lexer->buffer) && isdigit(current))
        {
            while (isdigit(current) || current == '.')
            {
                ptcl_string_buffer_append(lexer->buffer, current);
                ptcl_lexer_skip(lexer);
                current = ptcl_lexer_current(lexer);
            }

            ptcl_lexer_add_token_string(lexer, ptcl_token_number_type, ptcl_string_buffer_copy_and_clear(lexer->buffer), true);
            continue;
        }

        if (current == '"')
        {
            ptcl_lexer_skip(lexer);
            current = ptcl_lexer_current(lexer);

            while (current != '"')
            {
                if (!ptcl_lexer_not_ended(lexer))
                {
                    break;
                }

                ptcl_string_buffer_append(lexer->buffer, current);
                ptcl_lexer_skip(lexer);
                current = ptcl_lexer_current(lexer);
            }

            ptcl_lexer_skip(lexer);
            ptcl_lexer_add_token_string(lexer, ptcl_token_string_type, ptcl_string_buffer_copy_and_clear(lexer->buffer), true);
            continue;
        }
        else if (current == '\'')
        {
            ptcl_lexer_skip(lexer);

            char value = ptcl_lexer_current(lexer);
            ptcl_lexer_skip(lexer);
            ptcl_lexer_skip(lexer);

            ptcl_string_buffer_clear(lexer->buffer);
            ptcl_lexer_add_token_char(lexer, ptcl_token_character_type, value);
            continue;
        }
        else if (current == '/' && ptcl_lexer_not_ended(lexer) && lexer->source[lexer->position + 1] == '/')
        {
            while (ptcl_lexer_not_ended(lexer) && ptcl_lexer_current(lexer) != '\n')
            {
                ptcl_lexer_skip(lexer);
            }

            continue;
        }

        ptcl_string_buffer_append(lexer->buffer, current);
        ptcl_lexer_skip(lexer);

        if (!ptcl_lexer_not_ended(lexer))
        {
            // Getting last symbol
            current = lexer->source[strlen(lexer->source) - 1];
        }

        if (isspace(current))
        {
            char *buffer = ptcl_string_buffer_copy_and_clear(lexer->buffer);
            buffer[strlen(buffer) - 1] = '\0';

            ptcl_lexer_add_token_by_str(lexer, buffer, true);
            continue;
        }

        ptcl_token_type operator_type;

        if (!ptcl_lexer_configuration_try_get_token_char(lexer->configuration, current, &operator_type))
        {
            char *value = ptcl_string_buffer_copy(lexer->buffer);
            ptcl_token_type type;

            if (!ptcl_lexer_configuration_try_get_token(lexer->configuration, value, &type))
            {
                free(value);

                continue;
            }

            if (ptcl_lexer_not_ended(lexer) && current != ' ')
            {
                free(value);

                continue;
            }

            ptcl_lexer_add_token_string(lexer, type, value, true);
            continue;
        }

        char *value = ptcl_string_buffer_copy_and_clear(lexer->buffer);

        if (current != value[0])
        {
            // Remove the operator
            value[strlen(value) - 1] = '\0';
            ptcl_lexer_add_token_by_str(lexer, value, true);
        }
        else
        {
            free(value);
        }

        if (operator_type == ptcl_token_dot_type && lexer->capacity > 2)
        {
            ptcl_token *first = &lexer->tokens[lexer->capacity - 2];
            ptcl_token second = lexer->tokens[lexer->capacity - 1];
            if (first->type == ptcl_token_dot_type && second.type == ptcl_token_dot_type)
            {
                if (first->is_free_value)
                {
                    free(first->value);
                }

                if (second.is_free_value)
                {
                    free(second.value);
                }

                *first = (ptcl_token){
                    .type = ptcl_token_elipsis_type,
                    .value = "...",
                    .is_free_value = false,
                    .location = first->location};

                lexer->capacity--;
                continue;
            }
        }

        ptcl_lexer_add_token_char(lexer, operator_type, current);
    }

    ptcl_lexer_add_token_by_str(lexer, ptcl_string_buffer_copy_and_clear(lexer->buffer), false);
    return (ptcl_tokens_list){
        .source = lexer->source,
        .executor = lexer->executor,
        .tokens = lexer->tokens,
        .count = lexer->capacity};
}

void ptcl_lexer_destroy(ptcl_lexer *lexer)
{
    ptcl_string_buffer_destroy(lexer->buffer);
    free(lexer);
}
