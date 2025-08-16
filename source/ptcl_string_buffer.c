#include <stdlib.h>
#include <string.h>
#include <ptcl_string_buffer.h>
#include <ptcl_string.h>

typedef struct ptcl_string_buffer
{
    char *buffer;
    size_t capacity;
} ptcl_string_buffer;

ptcl_string_buffer *ptcl_string_buffer_create()
{
    ptcl_string_buffer *string_buffer = malloc(sizeof(ptcl_string_buffer));

    if (string_buffer == NULL)
    {
        return NULL;
    }

    char *buffer = malloc(sizeof(char));

    if (buffer == NULL)
    {
        free(string_buffer);
        return NULL;
    }

    string_buffer->buffer = buffer;
    string_buffer->capacity = 0;
    return string_buffer;
}

bool ptcl_string_buffer_append_str(ptcl_string_buffer *string_buffer, char *value, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (ptcl_string_buffer_append(string_buffer, value[i]))
        {
            continue;
        }

        return false;
    }

    return true;
}

bool ptcl_string_buffer_append(ptcl_string_buffer *string_buffer, char value)
{
    char *buffer = realloc(string_buffer->buffer, (string_buffer->capacity + 2) * sizeof(char));

    if (buffer == NULL)
    {
        return false;
    }

    string_buffer->buffer = buffer;
    string_buffer->buffer[string_buffer->capacity++] = value;
    string_buffer->buffer[string_buffer->capacity] = '\0';
    return true;
}

char *ptcl_string_buffer_copy(ptcl_string_buffer *string_buffer)
{
    return ptcl_string_duplicate(string_buffer->buffer);
}

char *ptcl_string_buffer_copy_and_clear(ptcl_string_buffer *string_buffer)
{
    char *result = ptcl_string_buffer_copy(string_buffer);

    ptcl_string_buffer_clear(string_buffer);
    return result;
}

size_t ptcl_string_buffer_length(ptcl_string_buffer *string_buffer)
{
    return string_buffer->capacity;
}

bool ptcl_string_buffer_is_empty(ptcl_string_buffer *string_buffer)
{
    if (string_buffer->capacity == 0)
    {
        return true;
    }

    for (size_t i = 0; i < string_buffer->capacity; i++)
    {
        if (string_buffer->buffer[i] == ' ')
        {
            continue;
        }

        return false;
    }

    return true;
}

void ptcl_string_buffer_clear(ptcl_string_buffer *string_buffer)
{
    string_buffer->capacity = 0;
    string_buffer->buffer = realloc(string_buffer->buffer, sizeof(char));
    string_buffer->buffer[0] = '\0';
}

void ptcl_string_buffer_destroy(ptcl_string_buffer *string_buffer)
{
    free(string_buffer->buffer);
    free(string_buffer);
}