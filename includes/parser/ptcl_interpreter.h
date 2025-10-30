#ifndef PTCL_INTERPRETER_H
#define PTCL_INTERPRETER_H

#define PTCL_INTERPRETER_DEFAULT_VARIABLES_CAPACITY 16
#define PTCL_INTERPRETER_DEFAULT_ARGUMENTS_CAPACITY 16
#define PTCL_INTERPRETER_MAX_ARGUMENTS 16
#define PTCL_INTERPRETER_MAX_STACK_TRACE 256

#include <ptcl_parser.h>

typedef struct ptcl_interpreter ptcl_interpreter;

ptcl_interpreter *ptcl_interpreter_create(ptcl_parser *parser);

ptcl_expression *ptcl_interpreter_evaluate_func_body(ptcl_interpreter *interpreter, ptcl_statement_func_body func_body, ptcl_location location);

ptcl_expression *ptcl_interpreter_evaluate_statement(ptcl_interpreter *interpreter, ptcl_statement *statement, ptcl_location location);

ptcl_expression *ptcl_interpreter_evaluate_expression(ptcl_interpreter *interpreter, ptcl_expression *expression, ptcl_location location);

ptcl_expression *ptcl_interpreter_evaluate_function_call(ptcl_interpreter *interpreter, ptcl_statement_func_call func_call, bool evaluate_arguments, ptcl_location location);

ptcl_expression *ptcl_interpreter_get_value(ptcl_interpreter *interpreter, ptcl_name name);

bool ptcl_interpreter_add_variable(ptcl_interpreter *interpreter, ptcl_name name, ptcl_expression *value, bool is_destroy);

bool ptcl_interpreter_was_called(ptcl_interpreter *interpreter, ptcl_name name);

void ptcl_interpreter_reset(ptcl_interpreter *interpreter);

void ptcl_interpreter_destroy(ptcl_interpreter *interpreter);

#endif // PTCL_INTERPRETER_H