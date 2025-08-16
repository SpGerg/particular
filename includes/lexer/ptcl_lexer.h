#ifndef PTCL_LEXER_H
#define PTCL_LEXER_H

#include <stdlib.h>
#include <ptcl_token.h>
#include <ptcl_lexer_configuration.h>

typedef struct ptcl_lexer ptcl_lexer;

ptcl_lexer* ptcl_lexer_create(char* executor, char* source, ptcl_lexer_configuration *configuration);

char ptcl_lexer_current(ptcl_lexer* lexer);

void ptcl_lexer_skip(ptcl_lexer* lexer);

bool ptcl_lexer_not_ended(ptcl_lexer* lexer);

void ptcl_lexer_add_token_by_str(ptcl_lexer* lexer, char* value, bool check_token);

void ptcl_lexer_add_buffer(ptcl_lexer* lexer, bool check_token);

bool ptcl_lexer_add_token(ptcl_lexer* lexer, ptcl_token token);

bool ptcl_lexer_add_token_string(ptcl_lexer* lexer, ptcl_token_type type, char* value, bool is_free_name);

bool ptcl_lexer_add_token_char(ptcl_lexer* lexer, ptcl_token_type type, char value);

ptcl_tokens_list ptcl_lexer_tokenize(ptcl_lexer* lexer);

void ptcl_lexer_destroy(ptcl_lexer* lexer);

#endif //PTCL_LEXER_H
