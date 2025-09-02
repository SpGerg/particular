#ifndef PTCL_NODE_H
#define PTCL_NODE_H

#include <ptcl_token.h>

typedef struct ptcl_statement ptcl_statement;
typedef struct ptcl_expression ptcl_expression;
typedef struct ptcl_statement_func_body ptcl_statement_func_body;
typedef struct ptcl_expression_dot ptcl_expression_dot;
typedef struct ptcl_type ptcl_type;

typedef enum ptcl_statement_type
{
    ptcl_statement_func_body_type,
    ptcl_statement_func_call_type,
    ptcl_statement_func_decl_type,
    ptcl_statement_typedata_decl_type,
    ptcl_statement_type_decl_type,
    ptcl_statement_assign_type,
    ptcl_statement_return_type,
    ptcl_statement_each_type,
    ptcl_statement_syntax_type,
    ptcl_statement_if_type
} ptcl_statement_type;

typedef enum ptcl_expression_type
{
    ptcl_expression_array_type,
    ptcl_expression_variable_type,
    ptcl_expression_character_type,
    ptcl_expression_double_type,
    ptcl_expression_float_type,
    ptcl_expression_integer_type,
    ptcl_expression_binary_type,
    ptcl_expression_unary_type,
    ptcl_expression_array_element_type,
    ptcl_expression_dot_type,
    ptcl_expression_ctor_type,
    ptcl_expression_if_type,
    ptcl_expression_func_call_type,
    ptcl_expression_word_type,
    ptcl_expression_object_type_type
} ptcl_expression_type;

typedef enum ptcl_binary_operator_type
{
    ptcl_binary_operator_none_type,
    ptcl_binary_operator_plus_type,
    ptcl_binary_operator_minus_type,
    ptcl_binary_operator_multiplicative_type,
    ptcl_binary_operator_division_type,
    ptcl_binary_operator_negation_type,
    ptcl_binary_operator_and_type,
    ptcl_binary_operator_or_type,
    ptcl_binary_operator_not_equals_type,
    ptcl_binary_operator_equals_type,
    ptcl_binary_operator_reference_type,
    ptcl_binary_operator_dereference_type,
    ptcl_binary_operator_greater_than_type,
    ptcl_binary_operator_less_than_type,
    ptcl_binary_operator_greater_equals_than_type,
    ptcl_binary_operator_less_equals_than_type,
} ptcl_binary_operator_type;

typedef enum ptcl_value_type
{
    ptcl_value_object_type_type,
    ptcl_value_typedata_type,
    ptcl_value_array_type,
    ptcl_value_pointer_type,
    ptcl_value_word_type,
    ptcl_value_character_type,
    ptcl_value_double_type,
    ptcl_value_float_type,
    ptcl_value_integer_type,
    ptcl_value_any_pointer_type,
    ptcl_value_any_type,
    ptcl_value_type_type,
    ptcl_value_void_type,
} ptcl_value_type;

typedef struct ptcl_name_word
{
    char *value;
    bool is_anonymous;
    bool is_free;
} ptcl_name_word;

typedef struct ptcl_name_tokens
{
    ptcl_token *tokens;
    size_t count;
} ptcl_name_tokens;

typedef struct ptcl_name
{
    bool is_name;
    ptcl_location location;

    union
    {
        ptcl_name_word word;
        ptcl_name_tokens tokens;
    };
} ptcl_name;

typedef struct ptcl_identifier
{
    bool is_name;

    union
    {
        ptcl_name_word name;
        ptcl_expression *value;
    };
} ptcl_identifier;

typedef struct ptcl_type_pointer
{
    ptcl_type *target;
    bool is_any;
} ptcl_type_pointer;

typedef struct ptcl_type_array
{
    ptcl_type *target;
    int count;
} ptcl_type_array;

typedef struct ptcl_type_object_type
{
    ptcl_type *target;
    bool is_any;
} ptcl_type_object_type;

typedef struct ptcl_type_comp_type
{
    ptcl_name_word identifier;
    ptcl_type *types;
    size_t count;
    bool is_any;
} ptcl_type_comp_type;

typedef struct ptcl_type
{
    ptcl_value_type type;
    bool is_primitive;
    bool is_static;

    union
    {
        ptcl_type_comp_type comp_type;
        ptcl_name_word typedata;
        ptcl_type_pointer pointer;
        ptcl_type_array array;
        ptcl_type_object_type object_type;
    };
} ptcl_type;

typedef struct ptcl_argument
{
    ptcl_type type;
    ptcl_name_word name;
    bool is_variadic;
} ptcl_argument;

static ptcl_type ptcl_type_any = {.type = ptcl_value_any_type, .is_primitive = true, .is_static = false};

static ptcl_type ptcl_type_any_pointer = {.type = ptcl_value_any_pointer_type, .is_primitive = true, .is_static = false};

static ptcl_type ptcl_type_any_type = {
    .type = ptcl_value_object_type_type, .object_type = (ptcl_type_object_type){.is_any = true}, .is_primitive = true, .is_static = false};

static ptcl_type ptcl_type_word = {.type = ptcl_value_word_type, .is_primitive = true, .is_static = true};

static ptcl_type ptcl_type_character = {.type = ptcl_value_character_type, .is_primitive = true, .is_static = false};

static ptcl_type ptcl_type_double = {.type = ptcl_value_double_type, .is_primitive = true, .is_static = false};

static ptcl_type ptcl_type_float = {.type = ptcl_value_float_type, .is_primitive = true, .is_static = false};

static ptcl_type ptcl_type_integer = {.type = ptcl_value_integer_type, .is_primitive = true, .is_static = false};

static ptcl_type ptcl_type_void = {.type = ptcl_value_void_type, .is_primitive = true, .is_static = false};

typedef struct ptcl_expression_variable
{
    ptcl_name_word name;
} ptcl_expression_variable;

typedef struct ptcl_expression_array
{
    ptcl_type type;
    ptcl_expression *expressions;
    size_t count;
} ptcl_expression_array;

typedef struct ptcl_expression_binary
{
    ptcl_binary_operator_type type;
    // First left, second right
    ptcl_expression *children;
} ptcl_expression_binary;

typedef struct ptcl_expression_unary
{
    ptcl_binary_operator_type type;
    ptcl_expression *child;
} ptcl_expression_unary;

typedef struct ptcl_expression_array_element
{
    ptcl_expression *children;
} ptcl_expression_array_element;

typedef struct ptcl_expression_dot
{
    ptcl_expression *left;
    bool is_end;

    union
    {
        ptcl_expression_dot *right;
        ptcl_name_word name;
    };
} ptcl_expression_dot;

// Typedata constructor
typedef struct ptcl_expression_ctor
{
    // Getted from typedata decl, do not free
    ptcl_name_word name;
    // Ordered by members in typedata
    ptcl_expression *values;
    size_t count;
} ptcl_expression_ctor;

typedef struct ptcl_expression_if
{
    // Condition, body, (else)
    ptcl_expression *children;
} ptcl_expression_if;

typedef struct ptcl_expression_object_type
{
    ptcl_type type;
} ptcl_expression_object_type;

typedef struct ptcl_statement_func_call
{
    ptcl_identifier identifier;
    ptcl_expression *arguments;
    size_t count;
    ptcl_type return_type;
    ptcl_expression *built_in;
    bool is_built_in;
} ptcl_statement_func_call;

typedef struct ptcl_expression
{
    ptcl_expression_type type;
    ptcl_type return_type;
    ptcl_location location;

    union
    {
        ptcl_statement_func_call func_call;
        ptcl_expression_array array;
        ptcl_name_word word;
        char character;
        double double_n;
        float float_n;
        int integer_n;
        ptcl_expression_variable variable;
        ptcl_expression_binary binary;
        ptcl_expression_unary unary;
        ptcl_expression_array_element array_element;
        ptcl_expression_dot dot;
        ptcl_expression_ctor ctor;
        ptcl_expression_if if_expr;
        ptcl_expression_object_type object_type;
    };
} ptcl_expression;

typedef struct ptcl_statement_func_body
{
    ptcl_statement *statements;
    size_t count;
    ptcl_statement_func_body *root;
} ptcl_statement_func_body;

typedef struct ptcl_statement_func_decl
{
    ptcl_name_word name;
    ptcl_argument *arguments;
    size_t count;
    ptcl_statement_func_body func_body;
    ptcl_type return_type;
    bool is_prototype;
    bool is_variadic;
} ptcl_statement_func_decl;

typedef struct ptcl_typedata_member
{
    ptcl_name_word name;
    ptcl_type type;
    size_t index;
} ptcl_typedata_member;

typedef struct ptcl_statement_typedata_decl
{
    ptcl_name_word name;
    ptcl_typedata_member *members;
    size_t count;
    bool is_prototype;
} ptcl_statement_typedata_decl;

typedef struct ptcl_statement_type_decl
{
    ptcl_name_word name;
    ptcl_type *types;
    size_t types_count;
    ptcl_statement_func_decl *functions;
    size_t functions_count;
    bool is_prototype;
} ptcl_statement_type_decl;

typedef struct ptcl_statement_assign
{
    ptcl_identifier identifier;
    ptcl_type type;
    bool with_type;
    ptcl_expression value;
    bool is_define;
} ptcl_statement_assign;

typedef struct ptcl_statement_return
{
    ptcl_expression value;
} ptcl_statement_return;

typedef struct ptcl_statement_if
{
    ptcl_expression condition;
    ptcl_statement_func_body body;
    bool with_else;
    ptcl_statement_func_body else_body;
} ptcl_statement_if;

typedef struct ptcl_statement
{
    ptcl_statement_type type;
    ptcl_location location;
    ptcl_statement_func_body *root;

    union
    {
        ptcl_statement_func_call func_call;
        ptcl_statement_func_decl func_decl;
        ptcl_statement_typedata_decl typedata_decl;
        ptcl_statement_type_decl type_decl;
        ptcl_statement_assign assign;
        ptcl_statement_return ret;
        ptcl_statement_if if_stat;
        ptcl_statement_func_body body;
    };
} ptcl_statement;

static void ptcl_statement_destroy(ptcl_statement statement);
static void ptcl_expression_destroy(ptcl_expression expression);

static ptcl_name ptcl_name_create(char *value, bool is_free, ptcl_location location)
{
    return (ptcl_name){
        .is_name = true,
        .location = location,
        .word.value = value,
        .word.is_anonymous = false,
        .word.is_free = is_free};
}

static ptcl_name ptcl_anonymous_name_create(char *value, bool is_free, ptcl_location location)
{
    return (ptcl_name){
        .is_name = true,
        .location = location,
        .word.value = value,
        .word.is_anonymous = true,
        .word.is_free = is_free};
}

static ptcl_name ptcl_name_create_l(char *value, bool is_anonymous, bool is_free, ptcl_location location)
{
    return (ptcl_name){
        .is_name = true,
        .word.value = value,
        .word.is_anonymous = is_anonymous,
        .word.is_free = false};
}

static ptcl_name_word ptcl_name_create_fast_w(char *value, bool is_anonymous)
{
    return (ptcl_name_word){
        .value = value,
        .is_anonymous = is_anonymous,
        .is_free = false};
}

static ptcl_identifier ptcl_identifier_create_by_name(ptcl_name_word name)
{
    return (ptcl_identifier){
        .is_name = true,
        .name = name};
}

static ptcl_type ptcl_type_create_typedata(char *identifier, bool is_anonymous)
{
    return (ptcl_type){
        .type = ptcl_value_typedata_type,
        .typedata = ptcl_name_create_fast_w(identifier, is_anonymous)};
}

static ptcl_type ptcl_type_create_array(ptcl_type *type, size_t count)
{
    return (ptcl_type){
        .type = ptcl_value_array_type,
        .array = (ptcl_type_array){
            .target = type,
            .count = count}};
}

static ptcl_type ptcl_type_create_object_type(ptcl_type *type)
{
    return (ptcl_type){
        .type = ptcl_value_object_type_type,
        .object_type = type};
}

static ptcl_type ptcl_type_create_pointer(ptcl_type *type)
{
    return (ptcl_type){
        .type = ptcl_value_pointer_type,
        .pointer = (ptcl_type_pointer){
            .target = type,
            .is_any = false}};
}

static ptcl_statement_func_body ptcl_statement_func_body_create(ptcl_statement *statements, size_t count, ptcl_statement_func_body *root)
{
    return (ptcl_statement_func_body){
        .statements = statements,
        .count = count,
        .root = root};
}

static ptcl_statement ptcl_statement_func_body_create_stat(ptcl_statement_func_body body, ptcl_location location)
{
    return (ptcl_statement){
        .type = ptcl_statement_func_body_type,
        .root = body.root,
        .body = body,
        .location = location};
}

static ptcl_statement_func_call ptcl_statement_func_call_create(ptcl_identifier identifier, ptcl_expression *arguments, size_t count)
{
    return (ptcl_statement_func_call){
        .identifier = identifier,
        .arguments = arguments,
        .count = 0};
}

static ptcl_name ptcl_name_create_tokens(ptcl_token *tokens, size_t count, ptcl_location location)
{
    return (ptcl_name){
        .is_name = false,
        .location = location,
        .tokens.tokens = tokens,
        .tokens.count = count};
}

static ptcl_statement_func_decl ptcl_statement_func_decl_create(
    ptcl_name_word name, ptcl_argument *arguments, size_t count, ptcl_statement_func_body func_body, ptcl_type return_type, bool is_prototype, bool is_variadic)
{
    return (ptcl_statement_func_decl){
        .name = name,
        .arguments = arguments,
        .count = count,
        .func_body = func_body,
        .return_type = return_type,
        .is_prototype = is_prototype,
        .is_variadic = is_variadic};
}

static ptcl_expression ptcl_expression_array_create(ptcl_type type, ptcl_expression *expressions, size_t count, ptcl_location location)
{
    return (ptcl_expression){
        .type = ptcl_expression_array_type,
        .return_type = type,
        .location = location,
        .array = (ptcl_expression_array){
            .type = type,
            .expressions = expressions,
            .count = count}};
}

static ptcl_expression_array ptcl_expression_array_create_empty(ptcl_type type, ptcl_location location)
{
    return (ptcl_expression_array){
        .type = type,
        .expressions = NULL,
        .count = 0};
}

static ptcl_expression ptcl_expression_create_character(char character, ptcl_location location)
{
    ptcl_expression value = {
        .type = ptcl_expression_character_type,
        .return_type = ptcl_type_character,
        .location = location,
        .character = character};
    value.return_type.is_static = true;
    return value;
}

static ptcl_expression ptcl_expression_create_characters(ptcl_expression *expressions, size_t count, ptcl_location location)
{
    ptcl_type array_type = ptcl_type_create_array(&ptcl_type_character, count);
    array_type.array.target->is_static = true;

    return (ptcl_expression){
        .type = ptcl_expression_array_type,
        .return_type = array_type,
        .location = location,
        .array = (ptcl_expression_array){
            .type = ptcl_type_character,
            .expressions = expressions,
            .count = count}};
}

static ptcl_expression ptcl_expression_create_array(ptcl_type type, ptcl_expression *expressions, size_t count, ptcl_location location)
{
    return (ptcl_expression){
        .type = ptcl_expression_array_type,
        .return_type = type,
        .location = location,
        .array = (ptcl_expression_array){
            .type = ptcl_type_character,
            .expressions = expressions,
            .count = count}};
}

static ptcl_expression ptcl_expression_create_variable(ptcl_name_word name, ptcl_type type, ptcl_location location)
{
    return (ptcl_expression){
        .type = ptcl_expression_variable_type,
        .return_type = type,
        .location = location,
        .variable = (ptcl_expression_variable){
            .name = name}};
}

static ptcl_expression ptcl_expression_create_object_type(ptcl_type return_type, ptcl_type type, ptcl_location location)
{
    return (ptcl_expression){
        .type = ptcl_expression_object_type_type,
        .return_type = return_type,
        .location = location,
        .object_type = (ptcl_expression_object_type){
            .type = type}};
}

static ptcl_statement_type_decl ptcl_statement_type_decl_create(
    ptcl_name_word name,
    ptcl_type *types, size_t types_count,
    ptcl_statement_func_decl *functions, size_t functions_count,
    bool is_prototype)
{
    return (ptcl_statement_type_decl){
        .name = name,
        .types = types,
        .types_count = types_count,
        .functions = functions,
        .functions_count = functions_count,
        .is_prototype = is_prototype};
}

static bool ptcl_name_word_compare(ptcl_name_word left, ptcl_name_word right)
{
    return strcmp(left.value, right.value) == 0 && left.is_anonymous == right.is_anonymous;
}

static bool ptcl_value_type_is_name(ptcl_value_type type)
{
    switch (type)
    {
    case ptcl_value_type_type:
    case ptcl_value_typedata_type:
        return true;
    default:
        return false;
    }
}

static ptcl_type *ptcl_type_get_target(ptcl_type type)
{
    switch (type.type)
    {
    case ptcl_value_pointer_type:
        return type.pointer.target;
    case ptcl_value_array_type:
        return type.array.target;
    default:
        return NULL;
    }
}

static ptcl_token_type ptcl_value_type_to_token(ptcl_value_type type)
{
    switch (type)
    {
    case ptcl_value_character_type:
        return ptcl_token_character_word_type;
    case ptcl_value_double_type:
        return ptcl_token_double_type;
    case ptcl_value_float_type:
        return ptcl_token_float_type;
    case ptcl_value_integer_type:
        return ptcl_token_integer_type;
    case ptcl_value_void_type:
        return ptcl_token_void_type;
    default:
        return ptcl_token_word_type;
    }
}

static char *ptcl_expression_get_name(ptcl_expression expression)
{
    if (expression.type == ptcl_expression_variable_type)
    {
        return expression.variable.name.value;
    }
    else if (expression.type == ptcl_expression_unary_type)
    {
        return ptcl_expression_get_name(*expression.unary.child);
    }

    return NULL;
}

static char *ptcl_identifier_get_name(ptcl_identifier identifier)
{
    if (identifier.is_name)
    {
        return identifier.name.value;
    }

    return ptcl_expression_get_name(*identifier.value);
}

static ptcl_type ptcl_type_get_common(ptcl_type left, ptcl_type right)
{
    enum
    {
        TYPE_INT,
        TYPE_FLOAT,
        TYPE_DOUBLE,
        TYPE_MAX
    };

    int left_priority = -1;
    int right_priority = -1;

    switch (left.type)
    {
    case ptcl_value_integer_type:
        left_priority = TYPE_INT;
        break;
    case ptcl_value_float_type:
        left_priority = TYPE_FLOAT;
        break;
    case ptcl_value_double_type:
        left_priority = TYPE_DOUBLE;
        break;
    }

    switch (right.type)
    {
    case ptcl_value_integer_type:
        right_priority = TYPE_INT;
        break;
    case ptcl_value_float_type:
        right_priority = TYPE_FLOAT;
        break;
    case ptcl_value_double_type:
        right_priority = TYPE_DOUBLE;
        break;
    }

    int result_priority = (left_priority > right_priority) ? left_priority : right_priority;

    switch (result_priority)
    {
    case TYPE_INT:
        return ptcl_type_integer;
    case TYPE_FLOAT:
        return ptcl_type_float;
    case TYPE_DOUBLE:
        return ptcl_type_double;
    }
}

static ptcl_expression ptcl_expression_binary_create(ptcl_binary_operator_type type, ptcl_expression *children, ptcl_location location)
{
    ptcl_type left = children[0].return_type;
    ptcl_type right = children[1].return_type;
    ptcl_expression binary = {
        .type = ptcl_expression_binary_type,
        .return_type = ptcl_type_get_common(left, right),
        .location = location,
        .binary = (ptcl_expression_binary){
            .type = type,
            .children = children}};

    binary.return_type.is_static = left.is_static && right.is_static;
    return binary;
}

static ptcl_expression ptcl_expression_unary_create(ptcl_binary_operator_type type, ptcl_expression *child, ptcl_location location)
{
    return (ptcl_expression){
        .type = ptcl_expression_unary_type,
        .return_type = child->return_type,
        .location = location,
        .unary = (ptcl_expression_unary){
            .type = type,
            .child = child}};
}

static ptcl_expression ptcl_expression_array_element_create(ptcl_expression *children, ptcl_location location)
{
    return (ptcl_expression){
        .type = ptcl_expression_array_element_type,
        .return_type = *children[0].return_type.array.target,
        .location = location,
        .array_element = (ptcl_expression_array_element){
            children = children}};
}

static ptcl_expression ptcl_expression_create_double(double value, ptcl_location location)
{
    ptcl_expression double_n = {
        .type = ptcl_expression_double_type,
        .return_type = ptcl_type_double,
        .location = location,
        .double_n = value};
    double_n.return_type.is_static = true;
    return double_n;
}

static ptcl_expression ptcl_expression_create_float(float value, ptcl_location location)
{
    ptcl_expression float_n = {
        .type = ptcl_expression_float_type,
        .return_type = ptcl_type_float,
        .location = location,
        .float_n = value};
    float_n.return_type.is_static = true;
    return float_n;
}

static ptcl_expression ptcl_expression_create_integer(int value, ptcl_location location)
{
    ptcl_expression integer_n = {
        .type = ptcl_expression_integer_type,
        .return_type = ptcl_type_integer,
        .location = location,
        .integer_n = value};
    integer_n.return_type.is_static = true;
    return integer_n;
}

static ptcl_argument ptcl_argument_create(ptcl_type type, ptcl_name_word name)
{
    return (ptcl_argument){
        .type = type,
        .name = name};
}

static ptcl_argument ptcl_argument_create_variadic()
{
    return (ptcl_argument){
        .name = "...",
        .type = ptcl_type_any,
        .is_variadic = true};
}

static ptcl_statement_assign ptcl_statement_assign_create(ptcl_identifier identifier, ptcl_type type, bool with_type, ptcl_expression value, bool is_define)
{
    return (ptcl_statement_assign){
        .identifier = identifier,
        .type = type,
        .with_type = with_type,
        .value = value,
        .is_define = is_define};
}

static ptcl_statement_return ptcl_statement_return_create(ptcl_expression value)
{
    return (ptcl_statement_return){
        .value = value};
}

static ptcl_typedata_member ptcl_typedata_member_create(ptcl_name_word name, ptcl_type type, size_t index)
{
    return (ptcl_typedata_member){
        .name = name,
        .type = type,
        .index = index};
}

static ptcl_statement_typedata_decl ptcl_statement_typedata_decl_create(ptcl_name_word name, ptcl_typedata_member *members, size_t count, bool is_prototype)
{
    return (ptcl_statement_typedata_decl){
        .name = name,
        .members = members,
        .count = count,
        .is_prototype = is_prototype};
}

static ptcl_statement_if ptcl_statement_if_create(ptcl_expression condition, ptcl_statement_func_body body, bool with_else, ptcl_statement_func_body else_body)
{
    return (ptcl_statement_if){
        .condition = condition,
        .body = body,
        .with_else = with_else,
        .else_body = false};
}

static ptcl_expression_if ptcl_expression_if_create(ptcl_expression *children)
{
    return (ptcl_expression_if){
        .children = children};
}

static ptcl_expression_dot ptcl_expression_dot_create(ptcl_expression *left, ptcl_name_word name)
{
    return (ptcl_expression_dot){
        .left = left,
        .name = name,
        .is_end = true};
}

static ptcl_expression_dot ptcl_expression_dot_create_continue(ptcl_expression_dot left, ptcl_expression_dot *right)
{
    left.is_end = false;
    left.right = right;

    return left;
}

static ptcl_expression_ctor ptcl_expression_ctor_create(ptcl_name_word name, ptcl_expression *values, size_t count)
{
    return (ptcl_expression_ctor){
        .name = name,
        .values = values,
        .count = count};
}

static ptcl_expression ptcl_expression_word_create(ptcl_name_word content, ptcl_location location)
{
    return (ptcl_expression){
        .type = ptcl_expression_word_type,
        .location = location,
        .return_type = ptcl_type_word,
        .word = content};
}

static ptcl_expression ptcl_expression_cast_to_double(ptcl_expression expression)
{
    switch (expression.type)
    {
    case ptcl_value_float_type:
        return ptcl_expression_create_double((double)expression.float_n, expression.location);
    case ptcl_value_integer_type:
        return ptcl_expression_create_double((double)expression.integer_n, expression.location);
    }

    return expression;
}

static ptcl_expression ptcl_expression_cast_to_float(ptcl_expression expression)
{
    switch (expression.type)
    {
    case ptcl_value_double_type:
        return ptcl_expression_create_float((float)expression.double_n, expression.location);
    case ptcl_value_integer_type:
        return ptcl_expression_create_float((float)expression.integer_n, expression.location);
    }

    return expression;
}

static ptcl_expression ptcl_expression_cast_to_integer(ptcl_expression expression)
{
    switch (expression.type)
    {
    case ptcl_value_double_type:
        return ptcl_expression_create_integer((int)expression.double_n, expression.location);
    case ptcl_value_float_type:
        return ptcl_expression_create_integer((int)expression.float_n, expression.location);
    }

    return expression;
}

static ptcl_expression ptcl_expression_unary_static_evaluate(ptcl_expression expression, ptcl_expression_unary unary)
{
    ptcl_type type = expression.return_type;
    if (!type.is_static)
    {
        return expression;
    }

    ptcl_expression value = unary.child[0];
    ptcl_location location = value.location;
    free(unary.child);

    switch (unary.type)
    {
    case ptcl_binary_operator_minus_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            return ptcl_expression_create_integer(
                -value.integer_n,
                location);

        case ptcl_value_float_type:
            return ptcl_expression_create_float(
                -value.float_n,
                location);

        case ptcl_value_double_type:
            return ptcl_expression_create_double(
                -value.double_n,
                location);
        }

        break;
    case ptcl_binary_operator_negation_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            return ptcl_expression_create_integer(
                !value.integer_n,
                location);

        case ptcl_value_float_type:
            return ptcl_expression_create_float(
                !value.float_n,
                location);

        case ptcl_value_double_type:
            return ptcl_expression_create_double(
                !value.double_n,
                location);
        }

        break;
    }
}

static ptcl_type ptcl_type_copy(ptcl_type type)
{
    ptcl_type copy = type;
    switch (type.type)
    {
    case ptcl_value_object_type_type:
        if (type.object_type.target != NULL)
        {
            copy.object_type.target = malloc(sizeof(ptcl_type));
            if (copy.object_type.target != NULL)
            {
                *copy.object_type.target = ptcl_type_copy(*type.object_type.target);
            }
            else
            {
                copy.object_type.target = NULL;
            }
        }

        break;
    case ptcl_value_pointer_type:
        if (type.pointer.target != NULL)
        {
            copy.pointer.target = malloc(sizeof(ptcl_type));
            if (copy.pointer.target != NULL)
            {
                *copy.pointer.target = ptcl_type_copy(*type.pointer.target);
            }
            else
            {
                copy.pointer.target = NULL;
            }
        }

        copy.pointer.is_any = type.pointer.is_any;
        break;
    case ptcl_value_array_type:
        if (type.array.target != NULL)
        {
            copy.array.target = malloc(sizeof(ptcl_type));
            if (copy.array.target != NULL)
            {
                *copy.array.target = ptcl_type_copy(*type.array.target);
            }
            else
            {
                copy.array.target = NULL;
            }
        }

        copy.array.count = type.array.count;
        break;
    case ptcl_value_typedata_type:
    case ptcl_value_word_type:
    case ptcl_value_character_type:
    case ptcl_value_double_type:
    case ptcl_value_float_type:
    case ptcl_value_integer_type:
    case ptcl_value_any_pointer_type:
    case ptcl_value_any_type:
    case ptcl_value_type_type:
    case ptcl_value_void_type:
        copy.typedata = type.typedata;
        break;
    default:
        break;
    }

    return copy;
}

static ptcl_expression ptcl_expression_copy(ptcl_expression target, ptcl_location location)
{
    switch (target.type)
    {
    case ptcl_expression_array_type:
        ptcl_expression *array = malloc(target.array.count * sizeof(ptcl_expression));
        for (size_t i = 0; i < target.array.count; i++)
        {
            array[i] = ptcl_expression_copy(target.array.expressions[i], location);
        }

        return ptcl_expression_create_array(ptcl_type_copy(target.return_type), array, target.array.count, location);
    case ptcl_expression_ctor_type:
        ptcl_expression *ctor = malloc(target.ctor.count * sizeof(ptcl_expression));
        for (size_t i = 0; i < target.ctor.count; i++)
        {
            ctor[i] = ptcl_expression_copy(target.ctor.values[i], location);
        }

        // Original will be freed
        ptcl_name_word copy = target.ctor.name;
        copy.is_free = false;
        return (ptcl_expression){
            .type = ptcl_expression_ctor_type,
            .return_type = ptcl_type_create_typedata(target.ctor.name.value, target.ctor.name.is_anonymous),
            .location = location,
            .ctor = ptcl_expression_ctor_create(copy, ctor, target.ctor.count)};
    case ptcl_expression_word_type:
    case ptcl_expression_character_type:
    case ptcl_expression_double_type:
    case ptcl_expression_float_type:
    case ptcl_expression_integer_type:
        target.location = location;
        return target;
    }
}

static bool ptcl_expression_binary_static_equals(ptcl_expression left, ptcl_expression right)
{
    if (left.type == ptcl_expression_array_type)
    {
        if (left.array.count != right.array.count)
        {
            return false;
        }

        for (size_t i = 0; i < left.array.count; i++)
        {
            if (ptcl_expression_binary_static_equals(left.array.expressions[i], right.array.expressions[i]))
            {
                continue;
            }

            return false;
        }

        return true;
    }

    return left.double_n == right.double_n; // Primitive type in union will have same value in integer
}

static ptcl_expression ptcl_expression_binary_static_evaluate(ptcl_expression expression, ptcl_expression_binary binary)
{
    if (expression.type != ptcl_expression_binary_type)
    {
        return expression;
    }

    ptcl_expression left_child = binary.children[0];
    ptcl_expression right_child = binary.children[1];
    ptcl_location location = left_child.location;
    ptcl_type left_type = left_child.return_type;
    ptcl_type right_type = right_child.return_type;

    if (!left_type.is_static || !right_type.is_static)
    {
        return expression;
    }

    ptcl_expression result;
    bool finded = false;
    switch (binary.type)
    {
    case ptcl_binary_operator_equals_type:
        result = ptcl_expression_create_integer(
            ptcl_expression_binary_static_equals(left_child, right_child),
            location);
        finded = true;
        break;
    case ptcl_binary_operator_not_equals_type:
        result = ptcl_expression_create_integer(
            !ptcl_expression_binary_static_equals(left_child, right_child),
            location);
        finded = true;
        break;
    }

    if (finded)
    {
        free(binary.children);
        ptcl_expression_destroy(left_child);
        ptcl_expression_destroy(right_child);
        return result;
    }

    ptcl_type type = ptcl_type_get_common(left_type, right_type);
    if (left_child.type == ptcl_expression_binary_type)
    {
        ptcl_expression new_left = ptcl_expression_binary_static_evaluate(left_child, left_child.binary);
        ptcl_expression_destroy(left_child);
        left_child = new_left;
    }

    if (right_child.type == ptcl_expression_binary_type)
    {
        ptcl_expression new_right = ptcl_expression_binary_static_evaluate(right_child, right_child.binary);
        ptcl_expression_destroy(right_child);
        right_child = new_right;
    }

    free(binary.children);
    ptcl_expression left_converted = left_child;
    ptcl_expression right_converted = right_child;
    if (left_child.return_type.type != type.type)
    {
        switch (type.type)
        {
        case ptcl_value_integer_type:
            left_converted = ptcl_expression_cast_to_integer(left_child);
            break;
        case ptcl_value_float_type:
            left_converted = ptcl_expression_cast_to_float(left_child);
            break;
        case ptcl_value_double_type:
            left_converted = ptcl_expression_cast_to_double(left_child);
            break;
        default:
            break;
        }
    }

    if (right_child.return_type.type != type.type)
    {
        switch (type.type)
        {
        case ptcl_value_integer_type:
            right_converted = ptcl_expression_cast_to_integer(right_child);
            break;
        case ptcl_value_float_type:
            right_converted = ptcl_expression_cast_to_float(right_child);
            break;
        case ptcl_value_double_type:
            right_converted = ptcl_expression_cast_to_double(right_child);
            break;
        default:
            break;
        }
    }

    if (left_converted.type != right_converted.type)
    {
        return expression;
    }

    switch (binary.type)
    {
    case ptcl_binary_operator_plus_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n + right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_float(
                left_converted.float_n + right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_double(
                left_converted.double_n + right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;

    case ptcl_binary_operator_minus_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n - right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_float(
                left_converted.float_n - right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_double(
                left_converted.double_n - right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;

    case ptcl_binary_operator_multiplicative_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n * right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_float(
                left_converted.float_n * right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_double(
                left_converted.double_n * right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;

    case ptcl_binary_operator_division_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n / right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_float(
                left_converted.float_n / right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_double(
                left_converted.double_n / right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;
    case ptcl_binary_operator_greater_than_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n > right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_integer(
                left_converted.float_n > right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_integer(
                left_converted.double_n > right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;

    case ptcl_binary_operator_less_than_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n < right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_integer(
                left_converted.float_n < right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_integer(
                left_converted.double_n < right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;

    case ptcl_binary_operator_greater_equals_than_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n >= right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_integer(
                left_converted.float_n >= right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_integer(
                left_converted.double_n >= right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;

    case ptcl_binary_operator_less_equals_than_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n <= right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_integer(
                left_converted.float_n <= right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_integer(
                left_converted.double_n <= right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;

    case ptcl_binary_operator_and_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n && right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_integer(
                left_converted.float_n && right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_integer(
                left_converted.double_n && right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;

    case ptcl_binary_operator_or_type:
        switch (type.type)
        {
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer(
                left_converted.integer_n || right_converted.integer_n,
                location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_integer(
                left_converted.float_n || right_converted.float_n,
                location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_integer(
                left_converted.double_n || right_converted.double_n,
                location);
            break;
        default:
            result = expression;
            break;
        }
        break;
    default:
        result = expression;
        break;
    }

    ptcl_expression_destroy(left_converted);
    ptcl_expression_destroy(right_converted);
    return result;
}

static ptcl_binary_operator_type ptcl_binary_operator_type_from_token(ptcl_token_type type)
{
    switch (type)
    {
    case ptcl_token_plus_type:
        return ptcl_binary_operator_plus_type;
    case ptcl_token_minus_type:
        return ptcl_binary_operator_minus_type;
    case ptcl_token_asterisk_type:
        return ptcl_binary_operator_multiplicative_type;
    case ptcl_token_slash_type:
        return ptcl_binary_operator_division_type;
    case ptcl_token_not_type:
        return ptcl_binary_operator_negation_type;
    case ptcl_token_and_type:
        return ptcl_binary_operator_and_type;
    case ptcl_token_or_type:
        return ptcl_binary_operator_or_type;
    case ptcl_token_equals_type:
        return ptcl_binary_operator_equals_type;
    case ptcl_token_ampersand_type:
        return ptcl_binary_operator_reference_type;
    case ptcl_token_greater_than_type:
        return ptcl_binary_operator_greater_than_type;
    case ptcl_token_less_than_type:
        return ptcl_binary_operator_less_than_type;
    default:
        return ptcl_binary_operator_none_type;
    }
}

static char *ptcl_type_to_word_copy(ptcl_type type)
{
    char *name;

    switch (type.type)
    {
    case ptcl_value_typedata_type:
        name = ptcl_string("typedata_", type.typedata.value, "_", NULL);
        break;
    case ptcl_value_array_type:
        char *array_name = ptcl_type_to_word_copy(*type.array.target);
        name = ptcl_string("array_", array_name, "_", NULL);
        free(array_name);
        break;
    case ptcl_value_pointer_type:
        char *pointer_name = ptcl_type_to_word_copy(*type.pointer.target);
        name = ptcl_string("pointer_", pointer_name, "_", NULL);
        free(pointer_name);
        break;
    case ptcl_value_any_pointer_type:
        name = "pointer";
        break;
    case ptcl_value_word_type:
        name = "word";
        break;
    case ptcl_value_character_type:
        name = "character";
        break;
    case ptcl_value_double_type:
        name = "double";
        break;
    case ptcl_value_float_type:
        name = "float";
        break;
    case ptcl_value_integer_type:
        name = "integer";
        break;
    case ptcl_value_any_type:
        name = "any";
        break;
    }

    if (type.type != ptcl_value_array_type && type.type != ptcl_value_pointer_type && type.type != ptcl_value_typedata_type)
    {
        name = ptcl_string(name, NULL);
    }

    return name;
}

static char *ptcl_type_to_present_string_copy(ptcl_type type)
{
    char *is_static = type.is_static ? "static " : "";
    char *name;

    switch (type.type)
    {
    case ptcl_value_typedata_type:
        name = ptcl_string(is_static, "typedata (", type.typedata.value, ")", NULL);
        break;
    case ptcl_value_array_type:
        char *array_name = ptcl_type_to_present_string_copy(*type.array.target);
        name = ptcl_string(is_static, "array (", array_name, ")", NULL);
        free(array_name);
        break;
    case ptcl_value_pointer_type:
        char *pointer_name = ptcl_type_to_present_string_copy(*type.pointer.target);
        name = ptcl_string(is_static, "pointer (", pointer_name, ")", NULL);
        free(pointer_name);
        break;
    case ptcl_value_any_pointer_type:
        name = "pointer";
        break;
    case ptcl_value_word_type:
        name = "word";
        break;
    case ptcl_value_character_type:
        name = "character";
        break;
    case ptcl_value_double_type:
        name = "double";
        break;
    case ptcl_value_float_type:
        name = "float";
        break;
    case ptcl_value_integer_type:
        name = "integer";
        break;
    case ptcl_value_any_type:
        name = "any";
        break;
    }

    if (type.type != ptcl_value_array_type && type.type != ptcl_value_pointer_type && type.type != ptcl_value_typedata_type)
    {
        name = ptcl_string(is_static, name, NULL);
    }

    return name;
}

static bool ptcl_func_body_can_access(ptcl_statement_func_body *target, ptcl_statement_func_body *requester)
{
    if (target == NULL)
    {
        return true;
    }

    if (requester == NULL)
    {
        return false;
    }

    if (target != requester)
    {
        return ptcl_func_body_can_access(target, requester->root);
    }

    return true;
}

static bool ptcl_type_equals(ptcl_type expected, ptcl_type target)
{
    if (expected.is_static && !target.is_static)
    {
        return false;
    }

    if (expected.type == ptcl_value_any_type)
    {
        return true;
    }

    if (expected.type == ptcl_value_any_pointer_type)
    {
        return target.type == ptcl_value_pointer_type ||
               target.type == ptcl_value_array_type ||
               target.type == ptcl_value_any_pointer_type;
    }

    if (expected.type != target.type)
    {
        if (expected.type == ptcl_value_float_type &&
            target.type == ptcl_value_integer_type)
        {
            return true;
        }

        if (expected.type == ptcl_value_double_type &&
            (target.type == ptcl_value_float_type ||
             target.type == ptcl_value_integer_type))
        {
            return true;
        }

        return false;
    }

    switch (expected.type)
    {
    case ptcl_value_pointer_type:
        if (expected.pointer.is_any || target.pointer.is_any)
            return true;

        return ptcl_type_equals(*expected.pointer.target, *target.pointer.target);
    case ptcl_value_array_type:
        return ptcl_type_equals(*expected.array.target, *target.array.target);
    case ptcl_value_object_type_type:
        if (expected.object_type.is_any)
        {
            return true;
        }

        return ptcl_type_equals(*expected.object_type.target, *target.object_type.target);
    case ptcl_value_typedata_type:
        if (!ptcl_name_word_compare(expected.comp_type.identifier, target.comp_type.identifier))
        {
            return false;
        }

        if (expected.comp_type.count != target.comp_type.count)
        {
            return false;
        }

        for (size_t i = 0; i < expected.comp_type.count; i++)
        {
            if (!ptcl_type_equals(expected.comp_type.types[i], target.comp_type.types[i]))
            {
                return false;
            }
        }

        return true;
    case ptcl_value_type_type:
        return ptcl_name_word_compare(expected.typedata, target.typedata);
    case ptcl_value_word_type:
    case ptcl_value_character_type:
    case ptcl_value_double_type:
    case ptcl_value_float_type:
    case ptcl_value_integer_type:
    case ptcl_value_void_type:
        return true;
    default:
        return true;
    }
}

static void ptcl_name_word_destroy(ptcl_name_word name)
{
    if (name.is_free)
    {
        free(name.value);
    }
}

static void ptcl_name_destroy(ptcl_name name)
{
    if (name.is_name)
    {
        ptcl_name_word_destroy(name.word);
    }
    else
    {
        for (size_t i = 0; i < name.tokens.count; i++)
        {
            ptcl_token_destroy(name.tokens.tokens[i]);
        }

        free(name.tokens.tokens);
    }
}

static void ptcl_statement_func_body_destroy(ptcl_statement_func_body func_body)
{
    if (func_body.count > 0)
    {
        for (size_t i = 0; i < func_body.count; i++)
        {
            ptcl_statement_destroy(func_body.statements[i]);
        }

        free(func_body.statements);
    }
}

static void ptcl_statement_if_destroy(ptcl_statement_if if_stat)
{
    ptcl_expression_destroy(if_stat.condition);
    ptcl_statement_func_body_destroy(if_stat.body);

    if (if_stat.with_else)
    {
        ptcl_statement_func_body_destroy(if_stat.else_body);
    }
}

static void ptcl_expression_if_destroy(ptcl_expression_if if_expr)
{
    ptcl_expression_destroy(if_expr.children[0]);
    ptcl_expression_destroy(if_expr.children[1]);
    free(if_expr.children);
}

static void ptcl_type_destroy(ptcl_type type)
{
    switch (type.type)
    {
    case ptcl_value_pointer_type:
        if (type.pointer.target != NULL)
        {
            ptcl_type_destroy(*type.pointer.target);
            free(type.pointer.target);
            type.pointer.target = NULL;
        }

        break;
    case ptcl_value_array_type:
        if (type.array.target != NULL)
        {
            ptcl_type_destroy(*type.array.target);
            free(type.array.target);
            type.array.target = NULL;
        }

        break;
    case ptcl_value_object_type_type:
        if (type.object_type.target != NULL)
        {
            ptcl_type_destroy(*type.object_type.target);
            free(type.object_type.target);
            type.object_type.target = NULL;
        }

        break;
    case ptcl_value_typedata_type:
    case ptcl_value_word_type:
    case ptcl_value_character_type:
    case ptcl_value_double_type:
    case ptcl_value_float_type:
    case ptcl_value_integer_type:
    case ptcl_value_any_pointer_type:
    case ptcl_value_any_type:
    case ptcl_value_type_type:
    case ptcl_value_void_type:
        break;
    default:
        break;
    }
}

static void ptcl_identifier_destroy(ptcl_identifier identifier)
{
    if (!identifier.is_name)
    {
        ptcl_expression_destroy(*identifier.value);
        free(identifier.value);
    }
    else
    {
        ptcl_name_word_destroy(identifier.name);
    }
}

static void ptcl_statement_func_call_destroy(ptcl_statement_func_call func_call)
{
    ptcl_identifier_destroy(func_call.identifier);

    if (func_call.count > 0)
    {
        for (size_t i = 0; i < func_call.count; i++)
        {
            ptcl_expression_destroy(func_call.arguments[i]);
        }

        free(func_call.arguments);
    }

    if (func_call.is_built_in)
    {
        free(func_call.built_in);
    }
}

static void ptcl_argument_destroy(ptcl_argument argument)
{
    ptcl_name_word_destroy(argument.name);
    if (!argument.is_variadic)
    {
        ptcl_type_destroy(argument.type);
    }
}

static void ptcl_statement_func_decl_destroy(ptcl_statement_func_decl func_decl)
{
    ptcl_name_word_destroy(func_decl.name);
    ptcl_type_destroy(func_decl.return_type);
    if (!func_decl.is_prototype)
    {
        ptcl_statement_func_body_destroy(func_decl.func_body);
    }

    if (func_decl.count > 0)
    {
        for (size_t i = 0; i < func_decl.count; i++)
        {
            ptcl_argument_destroy(func_decl.arguments[i]);
        }

        free(func_decl.arguments);
    }
}

static void ptcl_statement_type_decl_destroy(ptcl_statement_type_decl type_decl)
{
    ptcl_name_word_destroy(type_decl.name);
    if (type_decl.types_count > 0)
    {
        for (size_t i = 0; i < type_decl.types_count; i++)
        {
            ptcl_type_destroy(type_decl.types[i]);
        }

        free(type_decl.types);
    }

    if (type_decl.functions_count > 0)
    {
        for (size_t i = 0; i < type_decl.functions_count; i++)
        {
            ptcl_statement_func_decl_destroy(type_decl.functions[i]);
        }

        free(type_decl.functions);
    }
}

static ptcl_statement_assign ptcl_statement_assign_destroy(ptcl_statement_assign statement)
{
    ptcl_identifier_destroy(statement.identifier);
    ptcl_expression_destroy(statement.value);

    if (statement.with_type)
    {
        ptcl_type_destroy(statement.type);
    }
}

static void ptcl_typedata_member_destroy(ptcl_typedata_member typedata)
{
    ptcl_name_word_destroy(typedata.name);
    ptcl_type_destroy(typedata.type);
}

static void ptcl_statement_typedata_decl_destroy(ptcl_statement_typedata_decl typedata)
{
    if (typedata.count > 0)
    {
        for (size_t i = 0; i < typedata.count; i++)
        {
            ptcl_typedata_member_destroy(typedata.members[i]);
        }

        free(typedata.members);
    }
}

static void ptcl_statement_destroy(ptcl_statement statement)
{
    switch (statement.type)
    {
    case ptcl_statement_func_call_type:
        ptcl_statement_func_call_destroy(statement.func_call);
        break;
    case ptcl_statement_func_decl_type:
        ptcl_statement_func_decl_destroy(statement.func_decl);
        break;
    case ptcl_statement_typedata_decl_type:
        ptcl_statement_typedata_decl_destroy(statement.typedata_decl);
        break;
    case ptcl_statement_return_type:
        ptcl_expression_destroy(statement.ret.value);
        break;
    case ptcl_statement_assign_type:
        ptcl_statement_assign_destroy(statement.assign);
        break;
    case ptcl_statement_if_type:
        ptcl_statement_if_destroy(statement.if_stat);
        break;
    case ptcl_statement_type_decl_type:
        ptcl_statement_type_decl_destroy(statement.type_decl);
        break;
    case ptcl_statement_func_body_type:
        ptcl_statement_func_body_destroy(statement.body);
        break;
    }
}

static void ptcl_expression_ctor_destroy(ptcl_expression_ctor ctor)
{
    ptcl_name_word_destroy(ctor.name);
    if (ctor.count > 0)
    {
        for (size_t i = 0; i < ctor.count; i++)
        {
            ptcl_expression_destroy(ctor.values[i]);
        }

        free(ctor.values);
    }
}

static void ptcl_expression_array_destroy(ptcl_expression_array array)
{
    if (array.count > 0)
    {
        for (size_t i = 0; i < array.count; i++)
        {
            ptcl_expression_destroy(array.expressions[i]);
        }

        free(array.expressions);
    }

    ptcl_type_destroy(array.type);
}

static void ptcl_expression_destroy(ptcl_expression expression)
{
    if (expression.type != ptcl_expression_variable_type)
    {
        ptcl_type_destroy(expression.return_type);
    }

    switch (expression.type)
    {
    case ptcl_expression_func_call_type:
        ptcl_statement_func_call_destroy(expression.func_call);
        break;
    case ptcl_expression_array_type:
        ptcl_expression_array_destroy(expression.array);
        break;
    case ptcl_expression_binary_type:
        ptcl_expression_destroy(expression.binary.children[0]);
        ptcl_expression_destroy(expression.binary.children[1]);
        free(expression.binary.children);
        break;
    case ptcl_expression_unary_type:
        ptcl_expression_destroy(*expression.unary.child);
        free(expression.unary.child);
        break;
    case ptcl_expression_dot_type:
        if (expression.dot.is_end)
        {
            ptcl_name_word_destroy(expression.dot.name);
        }

        ptcl_expression_destroy(*expression.dot.left);
        free(expression.dot.left);
        break;
    case ptcl_expression_ctor_type:
        ptcl_expression_ctor_destroy(expression.ctor);
        break;
    case ptcl_expression_if_type:
        ptcl_expression_if_destroy(expression.if_expr);
        break;
    case ptcl_expression_word_type:
        ptcl_name_word_destroy(expression.word);
        break;
    }
}

#endif // PTCL_NODE_H
