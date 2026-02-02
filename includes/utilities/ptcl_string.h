#ifndef PTCL_STRING_H
#define PTCL_STRING_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static inline char *ptcl_string_duplicate(char *source)
{
    char *copy = malloc(strlen(source) + 1);
    if (copy == NULL)
    {
        return NULL;
    }

    strcpy(copy, source);
    return copy;
}

static inline char *ptcl_from_long(size_t number)
{
    size_t length = snprintf(NULL, 0, "%zu", number) + 1;
    char *result = (char *)malloc(length);
    if (result == NULL)
    {
        return NULL;
    }

    snprintf(result, length, "%zu", number);
    return result;
}

static inline char *ptcl_from_double(double number)
{
    size_t length = snprintf(NULL, 0, "%.15g", number) + 1;

    char *result = malloc(length);
    if (!result)
        return NULL;

    snprintf(result, length, "%.15g", number);
    return result;
}

static inline char *ptcl_from_float(float number)
{
    return ptcl_from_double((double)number);
}

static inline char *ptcl_from_int(int number)
{
    size_t length = snprintf(NULL, 0, "%d", number) + 1;
    char *result = malloc(length);
    if (result == NULL)
    {
        return NULL;
    }

    snprintf(result, length, "%d", number);
    return result;
}

static inline char *ptcl_string(char *first, ...)
{
    va_list arguments;
    va_start(arguments, first);

    size_t total_length = strlen(first);
    char *next_string;

    while ((next_string = va_arg(arguments, char *)) != NULL)
    {
        total_length += strlen(next_string);
    }

    va_end(arguments);
    char *result = (char *)malloc(total_length + 1);
    if (result == NULL)
    {
        return NULL;
    }

    strcpy(result, first);
    va_start(arguments, first);

    while ((next_string = va_arg(arguments, char *)) != NULL)
    {
        strcat(result, next_string);
    }

    va_end(arguments);
    return result;
}

static char *ptcl_string_append(char *original, const char *first, ...)
{
    if (first == NULL)
    {
        return original;
    }

    size_t additional_length = strlen(first);
    va_list args;
    va_start(args, first);
    const char *next;
    while ((next = va_arg(args, const char *)) != NULL)
    {
        additional_length += strlen(next);
    }

    va_end(args);

    size_t original_length = original ? strlen(original) : 0;
    char *new_str = realloc(original, original_length + additional_length + 1);
    if (!new_str)
    {
        free(original);
        return NULL;
    }

    char *current_pos = new_str + original_length;
    strcpy(current_pos, first);
    current_pos += strlen(first);

    va_start(args, first);
    while ((next = va_arg(args, const char *)) != NULL)
    {
        strcpy(current_pos, next);
        current_pos += strlen(next);
    }

    va_end(args);
    return new_str;
}

static inline bool ptcl_string_is_float(char *source)
{
    return strchr(source, '.') != NULL;
}

#endif // PTCL_STRING_H