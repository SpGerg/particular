#ifndef PTCL_PARSER_ERROR_H
#define PTCL_PARSER_ERROR_H

typedef enum ptcl_parser_error_type
{
    ptcl_parser_error_out_of_memory_type,
    ptcl_parser_error_except_token_type,
    ptcl_parser_error_except_type_type,
    ptcl_parser_error_except_type_specifier_type,
    ptcl_parser_error_incorrect_type_type,
    ptcl_parser_error_variable_redefination_type,
    ptcl_parser_error_none_type_type,
    ptcl_parser_error_must_be_variable_type,
    ptcl_parser_error_must_be_static_type,
    ptcl_parser_error_wrong_arguments_type,
    ptcl_parser_error_max_depth_type,
    ptcl_parser_error_unknown_member_type,
    ptcl_parser_error_unknown_statement_type,
    ptcl_parser_error_unknown_expression_type,
    ptcl_parser_error_unknown_function_type,
    ptcl_parser_error_unknown_variable_type,
    ptcl_parser_error_unknown_syntax_type,
    ptcl_parser_error_unknown_variable_or_type_type
} ptcl_parser_error_type;

typedef struct ptcl_parser_error
{
    ptcl_parser_error_type type;
    bool is_critical;
    char *message;
    bool is_free_message;
    ptcl_location location;
} ptcl_parser_error;

static ptcl_parser_error ptcl_parser_error_create(
    ptcl_parser_error_type type, bool is_critical, char *message, bool is_free_message, ptcl_location location)
{
    return (ptcl_parser_error){
        .type = type,
        .is_critical = is_critical,
        .message = message,
        .is_free_message = is_free_message,
        .location = location};
}

static void ptcl_parser_error_destroy(ptcl_parser_error error) {
    if (error.is_free_message) {
        free(error.message);
    }
}

#endif // PTCL_PARSER_ERROR_H