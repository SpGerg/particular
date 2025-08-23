#ifndef PTCL_STRING_BUFFER_H
#define PTCL_STRING_BUFFER_H

#include <stdbool.h>

typedef struct ptcl_string_buffer ptcl_string_buffer;

ptcl_string_buffer *ptcl_string_buffer_create();

bool ptcl_string_buffer_append_str(ptcl_string_buffer *string_buffer, char* value, size_t count);

bool ptcl_string_buffer_append(ptcl_string_buffer *string_buffer, char value);

bool ptcl_string_buffer_insert(ptcl_string_buffer *string_buffer, char value);

bool ptcl_string_buffer_insert_str(ptcl_string_buffer *string_buffer, char *value, size_t count);

char *ptcl_string_buffer_copy(ptcl_string_buffer *string_buffer);

char *ptcl_string_buffer_copy_and_clear(ptcl_string_buffer *string_buffer);

size_t ptcl_string_buffer_length(ptcl_string_buffer *string_buffer);

bool ptcl_string_buffer_is_empty(ptcl_string_buffer *string_buffer);

void ptcl_string_buffer_reset_position(ptcl_string_buffer *string_buffer);

size_t ptcl_string_buffer_get_position(ptcl_string_buffer *string_buffer);

void ptcl_string_buffer_set_position(ptcl_string_buffer *string_buffer, size_t position);

void ptcl_string_buffer_clear(ptcl_string_buffer *string_buffer);

void ptcl_string_buffer_destroy(ptcl_string_buffer *string_buffer);

#endif // PTCL_STRING_BUFFER_H