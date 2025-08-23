#include <stdlib.h>
#include <string.h>
#include <ptcl_string_buffer.h>
#include <ptcl_string.h>

typedef struct ptcl_string_buffer
{
    char *buffer;
    size_t capacity;
    size_t position;
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
    string_buffer->position = 0;
    string_buffer->buffer[0] = '\0';
    return string_buffer;
}

bool ptcl_string_buffer_append_str(ptcl_string_buffer *string_buffer, char *value, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!ptcl_string_buffer_append(string_buffer, value[i]))
        {
            return false;
        }
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

bool ptcl_string_buffer_insert(ptcl_string_buffer *string_buffer, char value)
{
    if (string_buffer->position > string_buffer->capacity)
    {
        return false;
    }

    if (string_buffer->position == string_buffer->capacity)
    {
        return ptcl_string_buffer_append(string_buffer, value);
    }

    char *buffer = realloc(string_buffer->buffer, (string_buffer->capacity + 2) * sizeof(char));
    if (buffer == NULL)
    {
        return false;
    }

    string_buffer->buffer = buffer;
    for (size_t i = string_buffer->capacity; i > string_buffer->position; i--)
    {
        string_buffer->buffer[i] = string_buffer->buffer[i - 1];
    }

    string_buffer->buffer[string_buffer->position] = value;
    string_buffer->capacity++;
    string_buffer->buffer[string_buffer->capacity] = '\0';
    string_buffer->position++;
    return true;
}

bool ptcl_string_buffer_insert_str(ptcl_string_buffer *string_buffer, char *value, size_t count)
{
    if (string_buffer->position > string_buffer->capacity)
    {
        return false;
    }

    if (count == 0)
    {
        return true;
    }

    size_t new_capacity = string_buffer->capacity + count + 1;
    char *buffer = realloc(string_buffer->buffer, new_capacity * sizeof(char));
    if (buffer == NULL)
    {
        return false;
    }

    string_buffer->buffer = buffer;
    if (string_buffer->position < string_buffer->capacity)
    {
        memmove(string_buffer->buffer + string_buffer->position + count,
                string_buffer->buffer + string_buffer->position,
                string_buffer->capacity - string_buffer->position);
    }

    memcpy(string_buffer->buffer + string_buffer->position, value, count);
    string_buffer->capacity += count;
    string_buffer->position += count;
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

void ptcl_string_buffer_reset_position(ptcl_string_buffer *string_buffer)
{
    string_buffer->position = string_buffer->capacity;
}

size_t ptcl_string_buffer_get_position(ptcl_string_buffer *string_buffer)
{
    return string_buffer->position;
}

void ptcl_string_buffer_set_position(ptcl_string_buffer *string_buffer, size_t position)
{
    if (position > string_buffer->capacity)
    {
        string_buffer->position = string_buffer->capacity;
    }
    else
    {
        string_buffer->position = position;
    }
}

void ptcl_string_buffer_clear(ptcl_string_buffer *string_buffer)
{
    string_buffer->capacity = 0;
    string_buffer->position = 0;
    string_buffer->buffer = realloc(string_buffer->buffer, sizeof(char));
    string_buffer->buffer[0] = '\0';
}

void ptcl_string_buffer_destroy(ptcl_string_buffer *string_buffer)
{
    free(string_buffer->buffer);
    free(string_buffer);
}