#ifndef PTCL_NODE_H
#define PTCL_NODE_H

#include <ptcl_token.h>

typedef struct ptcl_statement ptcl_statement;
typedef struct ptcl_expression ptcl_expression;
typedef struct ptcl_statement_func_body ptcl_statement_func_body;
typedef struct ptcl_expression_dot ptcl_expression_dot;
typedef struct ptcl_type ptcl_type;
typedef struct ptcl_type_member ptcl_type_member;
typedef struct ptcl_statement_func_decl ptcl_statement_func_decl;
typedef struct ptcl_argument ptcl_argument;
typedef struct ptcl_statement_func_decl ptcl_statement_func_decl;

typedef enum ptcl_statement_type
{
    ptcl_statement_none_type,
    ptcl_statement_func_body_type,
    ptcl_statement_func_call_type,
    ptcl_statement_func_decl_type,
    ptcl_statement_typedata_decl_type,
    ptcl_statement_type_decl_type,
    ptcl_statement_assign_type,
    ptcl_statement_return_type,
    ptcl_statement_each_type,
    ptcl_statement_syntax_type,
    ptcl_statement_unsyntax_type,
    ptcl_statement_undefine_type,
    ptcl_statement_if_type,
    ptcl_statement_import_type
} ptcl_statement_type;

typedef enum ptcl_expression_type
{
    ptcl_expression_lated_func_body_type,
    ptcl_expression_array_type,
    ptcl_expression_variable_type,
    ptcl_expression_character_type,
    ptcl_expression_double_type,
    ptcl_expression_float_type,
    ptcl_expression_integer_type,
    ptcl_expression_binary_type,
    ptcl_expression_cast_type,
    ptcl_expression_unary_type,
    ptcl_expression_array_element_type,
    ptcl_expression_dot_type,
    ptcl_expression_ctor_type,
    ptcl_expression_if_type,
    ptcl_expression_func_call_type,
    ptcl_expression_word_type,
    ptcl_expression_object_type_type,
    ptcl_expression_null_type,
    ptcl_expression_in_statement_type,
    ptcl_expression_in_expression_type,
    ptcl_expression_in_token_type
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
    ptcl_binary_operator_type_equals_type,
    ptcl_binary_operator_reference_type,
    ptcl_binary_operator_dereference_type,
    ptcl_binary_operator_greater_than_type,
    ptcl_binary_operator_less_than_type,
    ptcl_binary_operator_greater_equals_than_type,
    ptcl_binary_operator_less_equals_than_type,
} ptcl_binary_operator_type;

typedef enum ptcl_statement_modifiers
{
    ptcl_statement_modifiers_none_flag = 0,
    ptcl_statement_modifiers_const_flag = 1 << 0,
    ptcl_statement_modifiers_static_flag = 2 << 0,
    ptcl_statement_modifiers_global_flag = 3 << 0,
    ptcl_statement_modifiers_prototype_flag = 4 << 0,
    ptcl_statement_modifiers_auto_flag = 5 << 0,
    ptcl_statement_modifiers_optional_flag = 6 << 0
} ptcl_statement_modifiers;

static bool ptcl_statement_modifiers_flags_has(int flags, ptcl_statement_modifiers flag)
{
    return (flags & flag) == flag;
}

static void ptcl_statement_modifiers_flags_set(int *flags, ptcl_statement_modifiers flag)
{
    *flags |= flag;
}

static void ptcl_statement_modifiers_flags_remove(int *flags, ptcl_statement_modifiers flag)
{
    *flags &= ~flag;
}

static bool ptcl_statement_modifiers_flags_prototype(const int flags)
{
    return (flags & ptcl_statement_modifiers_prototype_flag) == ptcl_statement_modifiers_prototype_flag;
}

static bool ptcl_statement_modifiers_flags_static(const int flags)
{
    return (flags & ptcl_statement_modifiers_static_flag) == ptcl_statement_modifiers_static_flag;
}

static bool ptcl_statement_modifiers_flags_global(const int flags)
{
    return (flags & ptcl_statement_modifiers_global_flag) == ptcl_statement_modifiers_global_flag;
}

static bool ptcl_statement_modifiers_flags_const(const int flags)
{
    return (flags & ptcl_statement_modifiers_const_flag) == ptcl_statement_modifiers_const_flag;
}

static bool ptcl_statement_modifiers_flags_auto(const int flags)
{
    return (flags & ptcl_statement_modifiers_auto_flag) == ptcl_statement_modifiers_auto_flag;
}

static bool ptcl_statement_modifiers_flags_optional(const int flags)
{
    return (flags & ptcl_statement_modifiers_optional_flag) == ptcl_statement_modifiers_optional_flag;
}

static ptcl_statement_modifiers ptcl_statement_modifiers_none()
{
    return ptcl_statement_modifiers_none_flag;
}

typedef enum ptcl_value_type
{
    ptcl_value_function_pointer_type,
    ptcl_value_object_type_type,
    ptcl_value_typedata_type,
    ptcl_value_array_type,
    ptcl_value_pointer_type,
    ptcl_value_word_type,
    ptcl_value_character_type,
    ptcl_value_double_type,
    ptcl_value_float_type,
    ptcl_value_integer_type,
    ptcl_value_any_type,
    ptcl_value_type_type,
    ptcl_value_void_type,
} ptcl_value_type;

typedef struct ptcl_name
{
    ptcl_location location;
    char *value;
    bool is_anonymous;
    bool is_free;
} ptcl_name;

typedef struct ptcl_identifier
{
    bool is_name;

    union
    {
        ptcl_name name;
        ptcl_expression *value;
    };
} ptcl_identifier;

typedef struct ptcl_type_pointer
{
    ptcl_type *target;
    bool is_any;
    // TODO: make own null pointer type instead of it
    bool is_null;
    bool is_const;
} ptcl_type_pointer;

typedef struct ptcl_type_array
{
    ptcl_type *target;
    int count;
} ptcl_type_array;

typedef struct ptcl_type_object_type
{
    ptcl_type *target;
} ptcl_type_object_type;

typedef struct ptcl_type_comp_type
{
    ptcl_name identifier;
    ptcl_type_member *types;
    size_t count;
    ptcl_statement_func_body *functions;
    bool is_static;
    bool has_invariant;
    bool is_optional;
    bool is_any;
} ptcl_type_comp_type;

typedef struct ptcl_type_functon_pointer_type
{
    ptcl_type *return_type;
    ptcl_argument *arguments;
    size_t count;
    bool is_variadic;
    bool is_static_by_declaration;
} ptcl_type_functon_pointer_type;

typedef struct ptcl_type
{
    ptcl_value_type type;
    bool is_primitive;
    bool is_static;
    bool is_prototype_static;
    bool is_const;

    union
    {
        ptcl_type_comp_type *comp_type;
        ptcl_name typedata;
        ptcl_type_pointer pointer;
        ptcl_type_array array;
        ptcl_type_object_type object_type;
        ptcl_type_functon_pointer_type function_pointer;
    };
} ptcl_type;

typedef struct ptcl_attribute
{
    ptcl_name name;
} ptcl_attribute;

typedef struct ptcl_attributes
{
    ptcl_attribute *attributes;
    size_t count;
} ptcl_attributes;

typedef struct ptcl_argument
{
    ptcl_type type;
    ptcl_name name;
    bool is_variadic;
} ptcl_argument;

static ptcl_name ptcl_self_name = {
    .value = "self",
    .location = {0},
    .is_anonymous = false,
    .is_free = false};

static ptcl_type ptcl_type_any = {
    .type = ptcl_value_any_type,
    .is_primitive = true,
    .is_static = false,
    .is_const = false};

static ptcl_type ptcl_type_any_pointer = {
    .type = ptcl_value_pointer_type,
    .pointer = {.is_any = true, .is_null = false, .target = NULL},
    .is_primitive = true,
    .is_static = false,
    .is_const = false};

static ptcl_type ptcl_type_any_type = {
    .type = ptcl_value_object_type_type,
    .object_type = {.target = &ptcl_type_any},
    .is_primitive = true,
    .is_static = false,
    .is_const = false};

static ptcl_type const ptcl_type_word = {
    .type = ptcl_value_word_type,
    .is_primitive = true,
    .is_static = true,
    .is_const = false};

static ptcl_type ptcl_type_character = {
    .type = ptcl_value_character_type,
    .is_primitive = true,
    .is_static = false,
    .is_const = false};

static ptcl_type ptcl_type_double = {
    .type = ptcl_value_double_type,
    .is_primitive = true,
    .is_static = false,
    .is_const = false};

static ptcl_type ptcl_type_float = {
    .type = ptcl_value_float_type,
    .is_primitive = true,
    .is_static = false,
    .is_const = false};

static ptcl_type ptcl_type_integer = {
    .type = ptcl_value_integer_type,
    .is_primitive = true,
    .is_static = false,
    .is_const = false};

static ptcl_type ptcl_type_void = {
    .type = ptcl_value_void_type,
    .is_primitive = true,
    .is_static = false,
    .is_const = false};

static ptcl_type ptcl_type_null = {
    .type = ptcl_value_pointer_type,
    .is_primitive = true,
    .is_static = true,
    .is_const = false,
    .pointer = {.is_any = false, .is_null = true, .target = NULL}};

typedef struct ptcl_expression_variable
{
    ptcl_statement_func_body *root;
    ptcl_name name;
    bool is_syntax_variable;
} ptcl_expression_variable;

typedef struct ptcl_expression_array
{
    ptcl_type type;
    ptcl_expression **expressions;
    size_t count;
} ptcl_expression_array;

typedef struct ptcl_expression_binary
{
    ptcl_binary_operator_type type;
    ptcl_expression *left;
    ptcl_expression *right;
} ptcl_expression_binary;

typedef struct ptcl_expression_unary
{
    ptcl_binary_operator_type type;
    ptcl_expression *child;
} ptcl_expression_unary;

typedef struct ptcl_expression_array_element
{
    ptcl_expression *value;
    ptcl_expression *index;
} ptcl_expression_array_element;

typedef struct ptcl_expression_dot
{
    ptcl_expression *left;
    bool is_name;

    union
    {
        ptcl_expression *right;
        ptcl_name name;
    };
} ptcl_expression_dot;

// Typedata constructor
typedef struct ptcl_expression_ctor
{
    // Getted from typedata decl, do not free
    ptcl_name name;
    // Ordered by members in typedata
    ptcl_expression **values;
    size_t count;
} ptcl_expression_ctor;

typedef struct ptcl_expression_if
{
    ptcl_expression *condition;
    ptcl_expression *body;
    ptcl_expression *else_body;
} ptcl_expression_if;

typedef struct ptcl_expression_object_type
{
    ptcl_type type;
} ptcl_expression_object_type;

typedef struct ptcl_expression_cast
{
    ptcl_expression *value;
    ptcl_type type;
    bool is_free;
} ptcl_expression_cast;

typedef struct ptcl_statement_func_call
{
    ptcl_statement_func_decl *func_decl;
    ptcl_identifier identifier;
    ptcl_expression **arguments;
    size_t count;
    ptcl_type return_type;
    ptcl_expression *built_in;
    bool is_built_in;
} ptcl_statement_func_call;

typedef struct ptcl_statement_func_body
{
    ptcl_statement **statements;
    size_t count;
    ptcl_statement_func_body *root;
    // TODO: fix it somehow
    ptcl_argument *arguments;
    size_t arguments_count;
    ptcl_expression *self;
    ptcl_statement_func_call func_call;
} ptcl_statement_func_body;

typedef struct ptcl_expression_lated_func_body
{
    size_t index;
} ptcl_expression_lated_func_body;

typedef struct ptcl_statement_func_decl
{
    ptcl_statement_modifiers modifiers;
    ptcl_name name;
    ptcl_argument *arguments;
    size_t count;
    ptcl_statement_func_body *func_body;
    int index;
    ptcl_type return_type;
    bool is_variadic;
    bool is_self_const;
    bool with_self;
} ptcl_statement_func_decl;

typedef struct ptcl_expression
{
    ptcl_expression_type type;
    ptcl_type return_type;
    ptcl_location location;
    bool is_original;
    bool with_type;

    union
    {
        ptcl_expression_lated_func_body lated_body;
        ptcl_statement_func_call func_call;
        ptcl_expression_array array;
        ptcl_name word;
        char character;
        long long_n;
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
        ptcl_expression_cast cast;
        ptcl_statement *internal_statement;
        ptcl_token internal_token;
    };
} ptcl_expression;

typedef struct ptcl_statement_typedata_decl
{
    ptcl_statement_modifiers modifiers;
    ptcl_name name;
    ptcl_argument *members;
    size_t count;
    bool is_prototype;
} ptcl_statement_typedata_decl;

typedef struct ptcl_type_member
{
    ptcl_type type;
    bool is_up;
} ptcl_type_member;

typedef struct ptcl_statement_type_decl
{
    ptcl_statement_modifiers modifiers;
    ptcl_name name;
    ptcl_type_member *types;
    size_t types_count;
    ptcl_statement_func_body *body;
    ptcl_statement_func_body *functions;
} ptcl_statement_type_decl;

typedef struct ptcl_statement_assign
{
    ptcl_statement_modifiers modifiers;
    ptcl_identifier identifier;
    ptcl_type type;
    bool with_type;
    ptcl_expression *value;
    bool is_define;
} ptcl_statement_assign;

typedef struct ptcl_statement_return
{
    ptcl_expression *value;
} ptcl_statement_return;

typedef struct ptcl_statement_if
{
    ptcl_expression *condition;
    ptcl_statement_func_body body;
    bool with_else;
    ptcl_statement_func_body else_body;
} ptcl_statement_if;

typedef struct ptcl_statement
{
    ptcl_statement_type type;
    ptcl_location location;
    ptcl_statement_func_body *root;
    ptcl_attributes attributes;
    bool is_original;

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

static void ptcl_statement_destroy(ptcl_statement *statement);
static void ptcl_expression_destroy(ptcl_expression *expression);
static void ptcl_expression_inside_destroy(ptcl_expression *expression);
static bool ptcl_type_is_castable(ptcl_type expected, ptcl_type target);
static bool ptcl_type_equals(ptcl_type left, ptcl_type right);
static ptcl_name ptcl_name_create_fast_w(char *value, bool is_anonymous);
static void ptcl_type_destroy(ptcl_type type);

static ptcl_expression *ptcl_expression_create(ptcl_expression_type type, ptcl_type return_type, ptcl_location location)
{
    ptcl_expression *expression = malloc(sizeof(ptcl_expression));
    if (expression != NULL)
    {
        expression->type = type;
        expression->return_type = return_type;
        expression->location = location;
        expression->is_original = true;
        expression->with_type = true;
    }

    return expression;
}

static ptcl_statement *ptcl_statement_create(ptcl_statement_type type, ptcl_statement_func_body *root, ptcl_attributes attributes, ptcl_location location)
{
    ptcl_statement *statement = malloc(sizeof(ptcl_statement));
    if (statement != NULL)
    {
        statement->type = type;
        statement->is_original = true;
        statement->attributes = attributes;
        statement->root = root;
        statement->location = location;
    }

    return statement;
}

static ptcl_argument ptcl_argument_create(ptcl_type type, ptcl_name name)
{
    return (ptcl_argument){
        .type = type,
        .name = name};
}

static ptcl_argument ptcl_argument_create_variadic()
{
    return (ptcl_argument){
        .name = ptcl_name_create_fast_w("...", false),
        .type = ptcl_type_any,
        .is_variadic = true};
}

static ptcl_attributes ptcl_attributes_create(ptcl_attribute *attributes, size_t count)
{
    return (ptcl_attributes){
        .attributes = attributes,
        .count = count};
}

static ptcl_attribute ptcl_attribute_create(ptcl_name name)
{
    return (ptcl_attribute){
        .name = name};
}

static ptcl_name ptcl_name_create(char *value, bool is_free, ptcl_location location)
{
    return (ptcl_name){
        .location = location,
        .value = value,
        .is_anonymous = false,
        .is_free = is_free};
}

static ptcl_name ptcl_anonymous_name_create(char *value, bool is_free, ptcl_location location)
{
    return (ptcl_name){
        .location = location,
        .value = value,
        .is_anonymous = true,
        .is_free = is_free};
}

static ptcl_name ptcl_name_create_l(char *value, bool is_anonymous, bool is_free, ptcl_location location)
{
    return (ptcl_name){
        .value = value,
        .is_anonymous = is_anonymous,
        .location = location,
        .is_free = is_free};
}

static ptcl_name ptcl_name_create_fast_w(char *value, bool is_anonymous)
{
    return (ptcl_name){
        .value = value,
        .is_anonymous = is_anonymous,
        .is_free = false};
}

static ptcl_identifier ptcl_identifier_create_by_name(ptcl_name name)
{
    return (ptcl_identifier){
        .is_name = true,
        .name = name};
}

static ptcl_identifier ptcl_identifier_create_by_expr(ptcl_expression *expression)
{
    return (ptcl_identifier){
        .is_name = false,
        .value = expression};
}

static ptcl_identifier ptcl_identifier_create_by_str(char *value)
{
    return (ptcl_identifier){
        .is_name = true,
        .name = ptcl_name_create_fast_w(value, false)};
}

static ptcl_type ptcl_type_create_base(ptcl_value_type type, bool is_primitive, bool is_const, bool is_static)
{
    return (ptcl_type){
        .type = type,
        .is_primitive = is_primitive,
        .is_const = is_const,
        .is_static = is_static,
        .is_prototype_static = false};
}

static ptcl_type ptcl_type_create_typedata(char *identifier, bool is_const, bool is_anonymous)
{
    ptcl_type base = ptcl_type_create_base(ptcl_value_typedata_type, true, is_const, false);
    base.typedata = ptcl_name_create_fast_w(identifier, is_anonymous);
    return base;
}

static ptcl_type ptcl_type_create_array(ptcl_type *type, bool is_const, int count)
{
    ptcl_type base = ptcl_type_create_base(ptcl_value_array_type, true, is_const, true);
    base.array = (ptcl_type_array){
        .target = type,
        .count = count};
    return base;
}

static ptcl_type ptcl_type_create_object_type(ptcl_type *type, bool is_const)
{
    ptcl_type base = ptcl_type_create_base(ptcl_value_object_type_type, true, is_const, false);
    base.object_type.target = type;
    return base;
}

static ptcl_type ptcl_type_create_pointer(ptcl_type *type, bool is_const)
{
    ptcl_type base = ptcl_type_create_base(ptcl_value_pointer_type, true, is_const, type->is_static);
    base.pointer = (ptcl_type_pointer){
        .target = type,
        .is_any = false,
        .is_null = false,
        .is_const = is_const};
    base.pointer.target->is_static = false;
    return base;
}

static ptcl_type_comp_type ptcl_type_create_comp_type(ptcl_statement_type_decl type_decl, bool is_static)
{
    return (ptcl_type_comp_type){
        .identifier = ptcl_name_create_fast_w(type_decl.name.value, false),
        .types = type_decl.types,
        .count = type_decl.types_count,
        .functions = type_decl.functions,
        .is_static = is_static,
        .is_optional = ptcl_statement_modifiers_flags_optional(type_decl.modifiers),
        .is_any = false};
}

static ptcl_type_comp_type ptcl_type_create_comp_type_empty(ptcl_name name)
{
    return (ptcl_type_comp_type){
        .identifier = name,
        .types = NULL,
        .count = 0,
        .functions = NULL,
        .is_optional = false,
        .is_any = false};
}

static ptcl_type ptcl_type_create_comp_type_t(ptcl_type_comp_type *type, bool is_const)
{
    ptcl_type base = ptcl_type_create_base(ptcl_value_type_type, true, is_const, false);
    base.comp_type = type;
    return base;
}

static ptcl_statement_func_body ptcl_statement_func_body_inserted_create(
    ptcl_statement **statements, size_t count,
    ptcl_statement_func_body *root,
    ptcl_argument *arguments,
    size_t arguments_count,
    ptcl_expression *self,
    ptcl_statement_func_call func_call)
{
    return (ptcl_statement_func_body){
        .statements = statements,
        .count = count,
        .root = root,
        .arguments = arguments,
        .arguments_count = arguments_count,
        .self = self,
        .func_call = func_call};
}

static ptcl_statement_func_body ptcl_statement_func_body_create(
    ptcl_statement **statements, size_t count,
    ptcl_statement_func_body *root)
{
    return ptcl_statement_func_body_inserted_create(statements, count, root, NULL, 0, false, (ptcl_statement_func_call){0});
}

static ptcl_statement *ptcl_statement_func_body_create_stat(ptcl_statement_func_body body, ptcl_statement_func_body *root, ptcl_location location)
{
    ptcl_statement *statement = ptcl_statement_create(ptcl_statement_func_body_type, root, ptcl_attributes_create(NULL, 0), location);
    if (statement != NULL)
    {
        statement->body = body;
    }

    return statement;
}

static ptcl_statement_func_call ptcl_statement_func_call_create(ptcl_statement_func_decl *func_decl, ptcl_identifier identifier, ptcl_expression **arguments, size_t count)
{
    return (ptcl_statement_func_call){
        .func_decl = func_decl,
        .identifier = identifier,
        .arguments = arguments,
        .count = count};
}

static ptcl_expression *ptcl_expression_create_null(ptcl_location location)
{
    return ptcl_expression_create(ptcl_expression_null_type, ptcl_type_null, location);
}

static ptcl_statement_func_decl ptcl_statement_func_decl_create(
    ptcl_statement_modifiers modifiers, ptcl_name name, ptcl_argument *arguments, size_t count, ptcl_statement_func_body *func_body, ptcl_type return_type, bool is_variadic)
{
    return (ptcl_statement_func_decl){
        .name = name,
        .arguments = arguments,
        .count = count,
        .func_body = func_body,
        .index = -1,
        .return_type = return_type,
        .modifiers = modifiers,
        .is_variadic = is_variadic,
        .with_self = false};
}

static ptcl_expression *ptcl_expression_array_create(ptcl_type type, ptcl_expression **expressions, size_t count, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_array_type, type, location);
    if (expression != NULL)
    {
        expression->array = (ptcl_expression_array){
            .type = type,
            .expressions = expressions,
            .count = count};
    }

    return expression;
}

static ptcl_expression_array ptcl_expression_array_create_empty(ptcl_type type)
{
    return (ptcl_expression_array){
        .type = type,
        .expressions = NULL,
        .count = 0};
}

static ptcl_expression *ptcl_expression_create_character(char character, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_character_type, ptcl_type_character, location);
    if (expression != NULL)
    {
        expression->character = character;
        expression->return_type.is_static = true;
    }

    return expression;
}

static ptcl_expression *ptcl_expression_create_characters(ptcl_expression **expressions, size_t count, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_array_type, (ptcl_type){0}, location);
    if (expression != NULL)
    {
        ptcl_type array_type = ptcl_type_create_array(&ptcl_type_character, false, (int)count);
        expression->return_type = array_type;
        expression->array = (ptcl_expression_array){
            .type = ptcl_type_character,
            .expressions = expressions,
            .count = count};
    }

    return expression;
}

static ptcl_expression *ptcl_expression_create_array(ptcl_type type, ptcl_expression **expressions, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_array_type, type, location);
    if (expression != NULL)
    {
        expression->array = (ptcl_expression_array){
            .type = *type.array.target,
            .expressions = expressions,
            .count = type.array.count};
    }

    return expression;
}

static ptcl_expression *ptcl_expression_create_variable(ptcl_name name, ptcl_type type, bool is_syntax_variable, ptcl_statement_func_body *root, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_variable_type, type, location);
    if (expression != NULL)
    {
        expression->variable = (ptcl_expression_variable){
            .name = name,
            .is_syntax_variable = is_syntax_variable,
            .root = root};
    }

    return expression;
}

static ptcl_expression *ptcl_expression_create_object_type(ptcl_type return_type, ptcl_type type, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_object_type_type, return_type, location);
    if (expression != NULL)
    {
        expression->object_type = (ptcl_expression_object_type){
            .type = type};
    }

    return expression;
}

static ptcl_type_member ptcl_type_member_create(ptcl_type type, bool is_up)
{
    return (ptcl_type_member){
        .type = type,
        .is_up = is_up};
}

static ptcl_statement_type_decl ptcl_statement_type_decl_create(
    ptcl_statement_modifiers modifiers,
    ptcl_name name,
    ptcl_type_member *types, size_t types_count, ptcl_statement_func_body *body,
    ptcl_statement_func_body *functions)
{
    return (ptcl_statement_type_decl){
        .modifiers = modifiers,
        .name = name,
        .types = types,
        .types_count = types_count,
        .body = body,
        .functions = functions};
}

static bool ptcl_value_is_number(ptcl_value_type type)
{
    switch (type)
    {
    case ptcl_value_pointer_type:
    case ptcl_value_double_type:
    case ptcl_value_float_type:
    case ptcl_value_integer_type:
        return true;
    default:
        return false;
    }
}

static bool ptcl_name_compare(ptcl_name left, ptcl_name right)
{
    return left.is_anonymous == right.is_anonymous && (left.value == right.value || strcmp(left.value, right.value) == 0);
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

static ptcl_type ptcl_type_create_func_from_decl(ptcl_statement_func_decl func_decl, ptcl_type *return_type, bool is_static_by_declaration)
{
    return (ptcl_type){
        .type = ptcl_value_function_pointer_type,
        .is_primitive = true,
        .is_static = false,
        .function_pointer.return_type = return_type,
        .function_pointer.arguments = func_decl.arguments,
        .function_pointer.count = func_decl.count,
        .function_pointer.is_static_by_declaration = is_static_by_declaration,
        .function_pointer.is_variadic = func_decl.is_variadic};
}

static ptcl_type ptcl_type_create_func(ptcl_type *return_type, ptcl_argument *arguments, size_t count, bool is_static_by_declaration,
                                       bool is_variadic)
{
    return (ptcl_type){
        .type = ptcl_value_function_pointer_type,
        .is_primitive = true,
        .is_static = false,
        .function_pointer.return_type = return_type,
        .function_pointer.arguments = arguments,
        .function_pointer.count = count,
        .function_pointer.is_static_by_declaration = is_static_by_declaration,
        .function_pointer.is_variadic = is_variadic};
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

static ptcl_name ptcl_expression_get_name(ptcl_expression *expression)
{
    if (expression->type == ptcl_expression_variable_type)
    {
        return expression->variable.name;
    }
    else if (expression->type == ptcl_expression_unary_type)
    {
        return ptcl_expression_get_name(expression->unary.child);
    }
    else if (expression->type == ptcl_expression_array_element_type)
    {
        return ptcl_expression_get_name(expression->array_element.value);
    }
    else if (expression->type == ptcl_expression_dot_type)
    {
        return ptcl_expression_get_name(expression->dot.left);
    }

    return (ptcl_name){0};
}

static ptcl_name ptcl_identifier_get_name(ptcl_identifier identifier)
{
    if (identifier.is_name)
    {
        return identifier.name;
    }

    return ptcl_expression_get_name(identifier.value);
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
    case ptcl_value_pointer_type:
        right_priority = TYPE_INT;
        break;
    default:
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
    case ptcl_value_pointer_type:
        right_priority = TYPE_INT;
        break;
    default:
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

    return ptcl_type_integer;
}

static ptcl_expression *ptcl_expression_binary_create(ptcl_binary_operator_type type, ptcl_expression *left, ptcl_expression *right, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_binary_type, ptcl_type_get_common(left->return_type, right->return_type), location);
    if (expression != NULL)
    {
        expression->binary = (ptcl_expression_binary){
            .type = type,
            .left = left,
            .right = right};
        expression->return_type.is_static = left->return_type.is_static && right->return_type.is_static;
    }

    return expression;
}

static ptcl_expression *ptcl_expression_cast_create(ptcl_expression *value, ptcl_type type, bool is_free, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_cast_type, type, location);
    if (expression != NULL)
    {
        expression->cast = (ptcl_expression_cast){
            .type = type,
            .value = value,
            .is_free = is_free};
    }

    return expression;
}

static ptcl_expression *ptcl_expression_unary_create(ptcl_binary_operator_type type, ptcl_expression *child, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_unary_type, child->return_type, location);
    if (expression != NULL)
    {
        expression->unary = (ptcl_expression_unary){
            .type = type,
            .child = child};
    }

    return expression;
}

static ptcl_expression *ptcl_expression_create_token(ptcl_token token)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_in_token_type, ptcl_type_float, token.location);
    if (expression != NULL)
    {
        expression->internal_token = token;
        expression->internal_token.is_free_value = false;
        expression->return_type.is_static = true;
    }

    return expression;
}

static ptcl_expression *ptcl_expression_array_element_create(ptcl_expression *value, ptcl_type type, ptcl_expression *index, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_array_element_type, type, location);
    if (expression != NULL)
    {
        expression->array_element = (ptcl_expression_array_element){
            .value = value,
            .index = index};
    }

    return expression;
}

static ptcl_expression *ptcl_expression_create_double(double value, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_double_type, ptcl_type_double, location);
    if (expression != NULL)
    {
        expression->double_n = value;
        expression->return_type.is_static = true;
    }

    return expression;
}

static ptcl_expression *ptcl_expression_create_float(float value, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_float_type, ptcl_type_float, location);
    if (expression != NULL)
    {
        expression->float_n = value;
        expression->return_type.is_static = true;
    }

    return expression;
}

static ptcl_expression *ptcl_expression_create_integer(int value, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_integer_type, ptcl_type_integer, location);
    if (expression != NULL)
    {
        expression->integer_n = value;
        expression->return_type.is_static = true;
    }

    return expression;
}

static ptcl_statement_assign ptcl_statement_assign_create(ptcl_statement_modifiers modifiers, ptcl_identifier identifier, ptcl_type type, bool with_type, ptcl_expression *value, bool is_define)
{
    return (ptcl_statement_assign){
        .modifiers = modifiers,
        .identifier = identifier,
        .type = type,
        .with_type = with_type,
        .value = value,
        .is_define = is_define};
}

static ptcl_statement_return ptcl_statement_return_create(ptcl_expression *value)
{
    return (ptcl_statement_return){
        .value = value};
}

static ptcl_statement_typedata_decl ptcl_statement_typedata_decl_create(ptcl_statement_modifiers modifiers, ptcl_name name, ptcl_argument *members, size_t count, bool is_prototype)
{
    return (ptcl_statement_typedata_decl){
        .modifiers = modifiers,
        .name = name,
        .members = members,
        .count = count,
        .is_prototype = is_prototype};
}

static ptcl_statement_if ptcl_statement_if_create(ptcl_expression *condition, ptcl_statement_func_body body, bool with_else, ptcl_statement_func_body else_body)
{
    return (ptcl_statement_if){
        .condition = condition,
        .body = body,
        .with_else = with_else,
        .else_body = else_body};
}

static ptcl_expression_if ptcl_expression_if_create(ptcl_expression *condition, ptcl_expression *body, ptcl_expression *else_body)
{
    return (ptcl_expression_if){
        .condition = condition,
        .body = body,
        .else_body = else_body};
}

static ptcl_expression_dot ptcl_expression_dot_create(ptcl_expression *left, ptcl_name name)
{
    return (ptcl_expression_dot){
        .left = left,
        .name = name,
        .is_name = true};
}

static ptcl_expression_dot ptcl_expression_dot_expression_create(ptcl_expression *left, ptcl_expression *expression)
{
    return (ptcl_expression_dot){
        .left = left,
        .right = expression,
        .is_name = false};
}

static ptcl_expression_ctor ptcl_expression_ctor_create(ptcl_name name, ptcl_expression **values, size_t count)
{
    return (ptcl_expression_ctor){
        .name = name,
        .values = values,
        .count = count};
}

static ptcl_expression *ptcl_expression_word_create(ptcl_name content, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(ptcl_expression_word_type, ptcl_type_word, location);
    if (expression != NULL)
    {
        expression->word = content;
    }

    return expression;
}

static bool ptcl_comp_type_equals(ptcl_type_comp_type *comp_type, ptcl_type target, bool is_to)
{
    if (is_to)
    {
        for (size_t i = 0; i < comp_type->count; i++)
        {
            ptcl_type_member member = comp_type->types[i];
            if (member.is_up && ptcl_type_is_castable(member.type, target))
            {
                return true;
            }
        }

        if (comp_type->is_optional)
        {
            for (size_t i = 0; i < comp_type->count; i++)
            {
                if (ptcl_type_is_castable(comp_type->types[i].type, target))
                {
                    return true;
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < comp_type->count; i++)
        {
            ptcl_type_member member = comp_type->types[i];
            if (comp_type->is_optional)
            {
                if (ptcl_type_is_castable(member.type, target))
                {
                    return true;
                }
            }
            else
            {
                if (!member.is_up && ptcl_type_is_castable(member.type, target))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool ptcl_type_function_equals(ptcl_type_functon_pointer_type left, ptcl_type_functon_pointer_type right)
{
    if (left.count != right.count || !ptcl_type_equals(*left.return_type, *right.return_type))
    {
        return false;
    }

    for (size_t i = 0; i < left.count; i++)
    {
        if (ptcl_type_equals(left.arguments[i].type, right.arguments[i].type))
        {
            continue;
        }

        return false;
    }

    return true;
}

static char *ptcl_string_from_array(ptcl_expression_array expression)
{
    char *characters = malloc(sizeof(char) * expression.count);
    if (characters == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < expression.count; i++)
    {
        characters[i] = expression.expressions[i]->character;
    }

    return characters;
}

static bool ptcl_type_equals(ptcl_type left, ptcl_type right)
{
    if (left.type != right.type)
    {
        return false;
    }

    switch (left.type)
    {
    case ptcl_value_pointer_type:
        if (left.pointer.is_any || right.pointer.is_any)
        {
            return true;
        }

        return ptcl_type_equals(*left.pointer.target, *right.pointer.target);
    case ptcl_value_array_type:
        return ptcl_type_equals(*left.array.target, *right.array.target);
    case ptcl_value_object_type_type:
        return ptcl_type_equals(*left.object_type.target, *right.object_type.target);
    case ptcl_value_function_pointer_type:
        return ptcl_type_function_equals(left.function_pointer, right.function_pointer);
    default:
        break;
    }

    return true;
}

static bool ptcl_type_is_castable_to_unstatic(ptcl_type type)
{
    if (!type.is_static)
    {
        return true;
    }

    switch (type.type)
    {
    case ptcl_value_array_type:
        return ptcl_type_is_castable_to_unstatic(*type.array.target);
    case ptcl_value_word_type:
        return false;
    case ptcl_value_type_type:
        if (type.comp_type->count == 0 || (type.comp_type->is_static && !type.comp_type->has_invariant))
        {
            return false;
        }

        return true;
    default:
        return true;
    }
}

static inline bool ptcl_type_is_static(ptcl_type type)
{
    return type.is_static || type.is_prototype_static;
}

static bool ptcl_type_is_castable(ptcl_type expected, ptcl_type target)
{
    if ((ptcl_type_is_static(expected) && !ptcl_type_is_static(target)) || (!expected.is_const && target.is_const))
    {
        return false;
    }

    if (!ptcl_type_is_static(expected) && ptcl_type_is_static(target) && !ptcl_type_is_castable_to_unstatic(target))
    {
        return false;
    }

    if (expected.type == ptcl_value_any_type)
    {
        if (ptcl_type_is_static(expected))
        {
            return ptcl_type_is_static(target);
        }

        return true;
    }

    if (target.type == ptcl_value_function_pointer_type && !ptcl_type_is_static(expected) && (ptcl_type_is_static(target) && !target.function_pointer.is_static_by_declaration))
    {
        return false;
    }

    if (expected.type == ptcl_value_type_type)
    {
        if (target.type != ptcl_value_type_type || !ptcl_name_compare(expected.comp_type->identifier, target.comp_type->identifier))
        {
            return ptcl_comp_type_equals(expected.comp_type, target, true);
        }

        if (expected.comp_type->is_static && !target.comp_type->is_static)
        {
            return false;
        }

        return true;
    }

    if (expected.type != target.type)
    {
        if (expected.type == ptcl_value_pointer_type && target.type == ptcl_value_array_type)
        {
            return ptcl_type_is_castable(*expected.pointer.target, *target.array.target);
        }
        else if (target.type == ptcl_value_type_type && target.comp_type->is_optional)
        {
            return ptcl_comp_type_equals(target.comp_type, expected, false);
        }
        else if (expected.type == ptcl_value_function_pointer_type && target.type == ptcl_value_pointer_type && target.pointer.is_null)
        {
            return true;
        }

        return false;
    }

    switch (expected.type)
    {
    case ptcl_value_pointer_type:
        if (expected.pointer.is_any || target.pointer.is_null)
        {
            return true;
        }

        return ptcl_type_is_castable(*expected.pointer.target, *target.pointer.target);
    case ptcl_value_array_type:
        return ptcl_type_is_castable(*expected.array.target, *target.array.target);
    case ptcl_value_object_type_type:
        return ptcl_type_is_castable(*expected.object_type.target, *target.object_type.target);
    case ptcl_value_typedata_type:
        return ptcl_name_compare(expected.typedata, target.typedata);
    case ptcl_value_word_type:
    case ptcl_value_character_type:
    case ptcl_value_double_type:
    case ptcl_value_float_type:
    case ptcl_value_integer_type:
    case ptcl_value_void_type:
        return true;
    case ptcl_value_function_pointer_type:
        if (expected.function_pointer.count != target.function_pointer.count)
        {
            return false;
        }

        const bool is_castable_return = ptcl_type_is_castable(*expected.function_pointer.return_type, *target.function_pointer.return_type);
        if (!is_castable_return)
        {
            return false;
        }

        for (size_t i = 0; i < expected.function_pointer.count; i++)
        {
            ptcl_argument *left = &expected.function_pointer.arguments[i];
            ptcl_argument *right = &target.function_pointer.arguments[i];
            // Compiler will optimize it
            if (ptcl_type_is_static(expected))
            {
                if (!ptcl_name_compare(left->name, right->name))
                {
                    return false;
                }
            }

            if (!ptcl_type_equals(left->type, right->type))
            {
                return false;
            }
        }

        return true;
    default:
        return true;
    }
}

static ptcl_expression *ptcl_expression_cast_to_double(ptcl_expression *expression)
{
    switch (expression->type)
    {
    case ptcl_expression_float_type:
        expression->type = ptcl_expression_double_type;
        expression->return_type = ptcl_type_double;
        expression->return_type.is_static = true;
        expression->double_n = (double)expression->float_n;
        break;
    case ptcl_expression_integer_type:
        expression->type = ptcl_expression_double_type;
        expression->return_type = ptcl_type_double;
        expression->return_type.is_static = true;
        expression->double_n = (double)expression->integer_n;
        break;
    default:
        break;
    }

    return expression;
}

static ptcl_expression *ptcl_expression_cast_to_float(ptcl_expression *expression)
{
    switch (expression->type)
    {
    case ptcl_expression_double_type:
        expression->type = ptcl_expression_float_type;
        expression->return_type = ptcl_type_float;
        expression->return_type.is_static = true;
        expression->float_n = (float)expression->double_n;
        break;
    case ptcl_expression_integer_type:
        expression->type = ptcl_expression_float_type;
        expression->return_type = ptcl_type_float;
        expression->return_type.is_static = true;
        expression->float_n = (float)expression->integer_n;
        break;
    default:
        break;
    }

    return expression;
}

static void ptcl_expression_cast_to_integer(ptcl_expression *expression)
{
    switch (expression->type)
    {
    case ptcl_expression_double_type:
        expression->type = ptcl_expression_integer_type;
        expression->return_type = ptcl_type_integer;
        expression->return_type.is_static = true;
        expression->integer_n = (int)expression->double_n;
        break;
    case ptcl_expression_float_type:
        expression->type = ptcl_expression_integer_type;
        expression->return_type = ptcl_type_float;
        expression->return_type.is_static = true;
        expression->integer_n = (int)expression->float_n;
        break;
    default:
        break;
    }
}

static ptcl_expression *ptcl_expression_unary_static_evaluate(ptcl_binary_operator_type type, ptcl_expression *value)
{
    ptcl_type return_type = value->return_type;
    ptcl_location location = value->location;
    if ((!return_type.is_static || return_type.is_prototype_static) || type == ptcl_binary_operator_reference_type)
    {
        ptcl_expression *unary = ptcl_expression_unary_create(type, value, value->location);
        if (unary == NULL)
        {
            ptcl_expression_destroy(value);
        }

        return unary;
    }

    switch (type)
    {
    case ptcl_binary_operator_minus_type:
        switch (return_type.type)
        {
        case ptcl_value_integer_type:
            value->integer_n = -value->integer_n;
            break;
        case ptcl_value_float_type:
            value->float_n = -value->float_n;
            break;
        case ptcl_value_double_type:
            value->double_n = -value->double_n;
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_negation_type:
        switch (return_type.type)
        {
        case ptcl_value_integer_type:
            value->integer_n = !value->integer_n;
            break;
        case ptcl_value_float_type:
            value->float_n = !value->float_n;
            break;
        case ptcl_value_double_type:
            value->double_n = !value->double_n;
            break;
        default:
            break;
        }

        break;
    default:
        break;
    }

    return value;
}

static bool ptcl_statement_is_skip(ptcl_statement_type type)
{
    switch (type)
    {
    case ptcl_statement_syntax_type:
    case ptcl_statement_each_type:
    case ptcl_statement_func_body_type:
        return true;
    default:
        break;
    }

    return false;
}

static ptcl_type ptcl_type_copy(ptcl_type type, bool *is_out_of_memory)
{
    *is_out_of_memory = false;
    ptcl_type copy = type;
    switch (type.type)
    {
    case ptcl_value_object_type_type:
        if (type.object_type.target != NULL)
        {
            copy.object_type.target = malloc(sizeof(ptcl_type));
            *is_out_of_memory = copy.object_type.target == NULL;
            if (!*is_out_of_memory)
            {
                *copy.object_type.target = ptcl_type_copy(*type.object_type.target, is_out_of_memory);
                if (*is_out_of_memory)
                {
                    free(copy.object_type.target);
                    break;
                }

                copy.object_type.target->is_primitive = false;
            }
        }

        break;
    case ptcl_value_pointer_type:
        if (type.pointer.target != NULL)
        {
            copy.pointer.target = malloc(sizeof(ptcl_type));
            *is_out_of_memory = copy.pointer.target == NULL;
            if (!*is_out_of_memory)
            {
                *copy.pointer.target = ptcl_type_copy(*type.pointer.target, is_out_of_memory);
                if (*is_out_of_memory)
                {
                    free(copy.pointer.target);
                    break;
                }

                copy.pointer.target->is_primitive = false;
            }
        }

        copy.pointer.is_any = type.pointer.is_any;
        break;
    case ptcl_value_function_pointer_type:
    {
        ptcl_type *return_type = malloc(sizeof(ptcl_type));
        *is_out_of_memory = return_type == NULL;
        if (!*is_out_of_memory)
        {
            *return_type = ptcl_type_copy(*type.function_pointer.return_type, is_out_of_memory);
            if (*is_out_of_memory)
            {
                free(return_type);
                break;
            }

            return_type->is_primitive = false;
            ptcl_argument *arguments = malloc(type.function_pointer.count * sizeof(ptcl_argument));
            if (arguments == NULL)
            {
                *is_out_of_memory = true;
                free(return_type);
                break;
            }

            for (size_t i = 0; i < type.function_pointer.count; i++)
            {
                ptcl_argument target_argument = type.function_pointer.arguments[i];
                if (target_argument.is_variadic)
                {
                    arguments[i] = ptcl_argument_create_variadic();
                }
                else
                {
                    arguments[i] = ptcl_argument_create(ptcl_type_copy(type.function_pointer.arguments[i].type, is_out_of_memory), ptcl_name_create_fast_w(NULL, false));
                    if (*is_out_of_memory)
                    {
                        for (size_t j = 0; j < i; j++)
                        {
                            ptcl_type_destroy(arguments[j].type);
                        }

                        ptcl_type_destroy(*return_type);
                        free(return_type);
                        free(arguments);
                        break;
                    }
                }
            }

#pragma warning(push)
#pragma warning(disable : 6001)

            copy.function_pointer.return_type = return_type;
            copy.function_pointer.arguments = arguments;

#pragma warning(pop)
        }

        break;
    }
    case ptcl_value_array_type:
        if (type.array.target != NULL)
        {
            copy.array.target = malloc(sizeof(ptcl_type));
            *is_out_of_memory = copy.array.target == NULL;
            if (!*is_out_of_memory)
            {
                *copy.array.target = ptcl_type_copy(*type.array.target, is_out_of_memory);
                if (*is_out_of_memory)
                {
                    free(copy.array.target);
                    break;
                }

                copy.array.target->is_primitive = false;
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

static inline bool ptcl_expression_with_own_type(ptcl_expression_type type)
{
    if (type == ptcl_expression_variable_type ||
        type == ptcl_expression_null_type ||
        type == ptcl_expression_cast_type ||
        type == ptcl_expression_func_call_type ||
        type == ptcl_expression_dot_type ||
        type == ptcl_expression_array_element_type)
    {
        return false;
    }

    return true;
}

static ptcl_statement *ptcl_statement_copy(ptcl_statement *target, ptcl_location location)
{
    ptcl_statement *statement = ptcl_statement_create(target->type, target->root, target->attributes, location);
    if (statement != NULL)
    {
        *statement = *target;
        statement->is_original = false;
        statement->location = location;
    }

    return statement;
}

static ptcl_expression *ptcl_expression_copy(ptcl_expression *target, ptcl_location location)
{
    ptcl_expression *expression = ptcl_expression_create(target->type, target->return_type, location);
    if (expression != NULL)
    {
        *expression = *target;
        expression->is_original = false;

        bool is_out_of_memory = false;
        expression->return_type = ptcl_type_copy(target->return_type, &is_out_of_memory);
        if (is_out_of_memory)
        {
            free(expression);
            return NULL;
        }

        expression->location = location;
    }

    return expression;
}

static inline ptcl_expression_type ptcl_expression_type_from_value(ptcl_value_type type)
{
    switch (type)
    {
    case ptcl_value_double_type:
        return ptcl_expression_double_type;
    case ptcl_value_float_type:
        return ptcl_expression_float_type;
    case ptcl_value_integer_type:
        return ptcl_expression_integer_type;
    default:
        return -1;
    }
}

static bool ptcl_type_is_primitive(ptcl_value_type type)
{
    switch (type)
    {
    case ptcl_value_any_type:
    case ptcl_value_pointer_type:
    case ptcl_value_array_type:
    case ptcl_value_object_type_type:
    case ptcl_value_typedata_type:
    case ptcl_value_void_type:
    case ptcl_value_type_type:
        return false;
    default:
        return true;
    }
}

static ptcl_expression *ptcl_expression_static_cast(ptcl_expression *expression)
{
    if (expression->type != ptcl_expression_cast_type || !expression->cast.value->return_type.is_static)
    {
        return expression;
    }

    ptcl_type type = expression->cast.type;
    ptcl_expression *value = expression->cast.value;
    if (value->return_type.type == ptcl_value_function_pointer_type || !ptcl_type_is_primitive(expression->return_type.type) || !ptcl_type_is_primitive(type.type))
    {
        return expression;
    }

    ptcl_expression *result = NULL;
    ptcl_location location = expression->location;
    switch (value->return_type.type)
    {
    case ptcl_value_character_type:
        switch (type.type)
        {
        case ptcl_value_character_type:
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_double((double)value->character, location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_float((float)value->character, location);
            break;
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer((int)value->character, location);
            break;
        default:
            break;
        }

        break;
    case ptcl_value_double_type:
        switch (type.type)
        {
        case ptcl_value_character_type:
            result = ptcl_expression_create_character((char)value->double_n, location);
            break;
        case ptcl_value_double_type:
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_float((float)value->double_n, location);
            break;
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer((int)value->double_n, location);
            break;
        default:
            break;
        }

        break;
    case ptcl_value_float_type:
        switch (type.type)
        {
        case ptcl_value_character_type:
            result = ptcl_expression_create_character((char)value->float_n, location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_double((double)value->float_n, location);
            break;
        case ptcl_value_float_type:
            break;
        case ptcl_value_integer_type:
            result = ptcl_expression_create_integer((int)value->float_n, location);
            break;
        default:
            break;
        }

        break;
    case ptcl_value_integer_type:
        switch (type.type)
        {
        case ptcl_value_character_type:
            result = ptcl_expression_create_character((char)value->integer_n, location);
            break;
        case ptcl_value_double_type:
            result = ptcl_expression_create_double((double)value->integer_n, location);
            break;
        case ptcl_value_float_type:
            result = ptcl_expression_create_float((float)value->integer_n, location);
            break;
        case ptcl_value_integer_type:
            break;
        default:
            break;
        }

        break;
    default:
        break;
    }

    if (result == NULL)
    {
        return expression;
    }

    ptcl_expression_destroy(expression);
    result->return_type.is_static = true;
    return result;
}

static void ptcl_expression_by_count_destroy(ptcl_expression **expressions, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        ptcl_expression *expression = expressions[i];
        if (expression == NULL)
        {
            continue;
        }

        ptcl_expression_destroy(expression);
    }
}

static bool ptcl_statement_is_caller(ptcl_statement_type type)
{
    if (type == ptcl_statement_func_call_type || type == ptcl_statement_import_type)
    {
        return true;
    }

    return false;
}

static bool ptcl_expression_binary_static_type_equals(ptcl_expression *left, ptcl_expression *right)
{
    if (right->return_type.type != ptcl_value_object_type_type)
    {
        return false;
    }

    return ptcl_type_is_castable(*right->return_type.object_type.target, left->return_type);
}

static bool ptcl_expression_binary_static_equals(ptcl_expression *left, ptcl_expression *right)
{
    if (left->type == ptcl_expression_array_type)
    {
        if (left->array.count != right->array.count)
        {
            return false;
        }

        for (size_t i = 0; i < left->array.count; i++)
        {
            if (ptcl_expression_binary_static_equals(left->array.expressions[i], right->array.expressions[i]))
            {
                continue;
            }

            return false;
        }

        return true;
    }
    else if (right->type == ptcl_expression_null_type)
    {
        return left->type == ptcl_expression_null_type;
    }

    return left->long_n == right->long_n; // Primitive type in union will have same value in integer
}

static inline ptcl_expression *ptcl_expression_binary_static_evaluate(ptcl_binary_operator_type type, ptcl_expression *left, ptcl_expression *right)
{
    ptcl_location location = left->location;
    ptcl_type left_type = left->return_type;
    ptcl_type right_type = right->return_type;
    if ((!left_type.is_static || (left_type.is_prototype_static || right_type.is_prototype_static)) || !right_type.is_static)
    {
        ptcl_expression *binary = ptcl_expression_binary_create(type, left, right, location);
        if (binary == NULL)
        {
            ptcl_expression_destroy(left);
            ptcl_expression_destroy(right);
        }

        return binary;
    }

    int condition = -1;
    ptcl_expression *result = left;
    result->location = location;
    switch (type)
    {
    case ptcl_binary_operator_type_equals_type:
        condition = (int)ptcl_expression_binary_static_type_equals(left, right);
        break;
    case ptcl_binary_operator_equals_type:
        condition = (int)ptcl_expression_binary_static_equals(left, right);
        break;
    case ptcl_binary_operator_not_equals_type:
        condition = (int)!ptcl_expression_binary_static_equals(left, right);
        break;
    default:
        break;
    }

    if (condition >= 0)
    {
        ptcl_expression_inside_destroy(result);
        result->type = ptcl_expression_integer_type;
        result->return_type = ptcl_type_integer;
        result->return_type.is_static = true;
        result->integer_n = condition;
        ptcl_expression_destroy(right);
        return result;
    }

    ptcl_type common_type = ptcl_type_get_common(left_type, right_type);
    if (left->return_type.type != common_type.type)
    {
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            ptcl_expression_cast_to_integer(left);
            break;
        case ptcl_value_float_type:
            ptcl_expression_cast_to_float(left);
            break;
        case ptcl_value_double_type:
            ptcl_expression_cast_to_double(left);
            break;
        default:
            break;
        }
    }

    if (right->return_type.type != common_type.type)
    {
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            ptcl_expression_cast_to_integer(right);
            break;
        case ptcl_value_float_type:
            ptcl_expression_cast_to_float(right);
            break;
        case ptcl_value_double_type:
            ptcl_expression_cast_to_double(right);
            break;
        default:
            break;
        }
    }

    left->location = location;
    left->with_type = false;
    ptcl_expression_inside_destroy(left);
    switch (type)
    {
    case ptcl_binary_operator_plus_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n += right->integer_n;
            break;
        case ptcl_value_float_type:
            left->float_n += right->float_n;
            break;
        case ptcl_value_double_type:
            left->double_n += right->double_n;
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_minus_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n -= right->integer_n;
            break;
        case ptcl_value_float_type:
            left->float_n -= right->float_n;
            break;
        case ptcl_value_double_type:
            left->double_n -= right->double_n;
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_multiplicative_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n *= right->integer_n;
            break;
        case ptcl_value_float_type:
            left->float_n *= right->float_n;
            break;
        case ptcl_value_double_type:
            left->double_n *= right->double_n;
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_division_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n /= right->integer_n;
            break;
        case ptcl_value_float_type:
            left->float_n /= right->float_n;
            break;
        case ptcl_value_double_type:
            left->double_n /= right->double_n;
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_greater_than_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n = (int)(result->integer_n > right->integer_n);
            break;
        case ptcl_value_float_type:
            left->float_n = (float)(result->float_n > right->float_n);
            break;
        case ptcl_value_double_type:
            left->double_n = (double)(result->double_n > right->double_n);
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_less_than_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n = (int)(result->integer_n < right->integer_n);
            break;
        case ptcl_value_float_type:
            left->float_n = (float)(result->float_n < right->float_n);
            break;
        case ptcl_value_double_type:
            left->double_n = (double)(result->double_n < right->double_n);
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_greater_equals_than_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n = (int)(result->integer_n >= right->integer_n);
            break;
        case ptcl_value_float_type:
            left->float_n = (float)(result->float_n >= right->float_n);
            break;
        case ptcl_value_double_type:
            left->double_n = (double)(result->double_n >= right->double_n);
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_less_equals_than_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n = (int)(result->integer_n <= right->integer_n);
            break;
        case ptcl_value_float_type:
            left->float_n = (float)(result->float_n <= right->float_n);
            break;
        case ptcl_value_double_type:
            left->double_n = (double)(result->double_n <= right->double_n);
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_and_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n = (int)(result->integer_n && right->integer_n);
            break;
        case ptcl_value_float_type:
            left->float_n = (float)(result->float_n && right->float_n);
            break;
        case ptcl_value_double_type:
            left->double_n = (double)(result->double_n && right->double_n);
            break;
        default:
            break;
        }

        break;
    case ptcl_binary_operator_or_type:
        switch (common_type.type)
        {
        case ptcl_value_integer_type:
            left->integer_n = (int)(result->integer_n || right->integer_n);
            break;
        case ptcl_value_float_type:
            left->float_n = (float)(result->float_n || right->float_n);
            break;
        case ptcl_value_double_type:
            left->double_n = (double)(result->double_n || right->double_n);
            break;
        default:
            break;
        }

        break;
    default:
        break;
    }

    left->type = ptcl_expression_type_from_value(common_type.type);
    left->return_type = common_type;
    left->return_type.is_static = true;
    left->with_type = true;
    ptcl_expression_destroy(right);
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
    case ptcl_token_is_type:
        return ptcl_binary_operator_type_equals_type;
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
    case ptcl_value_type_type:
        name = ptcl_string(type.comp_type->identifier.value, NULL);
        break;
    case ptcl_value_typedata_type:
        name = ptcl_string("_struct_", type.typedata.value, NULL);
        break;
    case ptcl_value_array_type:
    {
        char *array_name = ptcl_type_to_word_copy(*type.array.target);
        name = ptcl_string("_array_", array_name, "_", NULL);
        free(array_name);
        break;
    }
    case ptcl_value_pointer_type:
    {
        char *pointer_name = ptcl_type_to_word_copy(*type.pointer.target);
        name = ptcl_string("_ptr_", pointer_name, "_", NULL);
        free(pointer_name);
        break;
    }
    case ptcl_value_function_pointer_type:
    {
        char *return_type = ptcl_type_to_word_copy(*type.function_pointer.return_type);
        name = ptcl_string("fn", return_type, "_", NULL);
        for (size_t i = 0; i < type.function_pointer.count; i++)
        {
            ptcl_argument argument = type.function_pointer.arguments[i];
            if (argument.is_variadic)
            {
                name = ptcl_string_append(name, "_va", NULL);
            }
            else
            {
                char *value = ptcl_type_to_word_copy(argument.type);
                name = ptcl_string_append(name, value, NULL);
                free(value);
            }
        }

        free(return_type);
        break;
    }
    case ptcl_value_object_type_type:
    {
        char *target_name = ptcl_type_to_word_copy(*type.object_type.target);
        name = ptcl_string(target_name, "_t", NULL);
        free(target_name);
        break;
    }
    case ptcl_value_word_type:
        name = ptcl_string("_word", NULL);
        break;
    case ptcl_value_character_type:
        name = ptcl_string("_char", NULL);
        break;
    case ptcl_value_double_type:
        name = ptcl_string("_double", NULL);
        break;
    case ptcl_value_float_type:
        name = ptcl_string("_float", NULL);
        break;
    case ptcl_value_integer_type:
        name = ptcl_string("_int", NULL);
        break;
    case ptcl_value_any_type:
        name = ptcl_string("_any", NULL);
        break;
    case ptcl_value_void_type:
        name = ptcl_string("_void", NULL);
        break;
    default:
        name = ptcl_string("_unknown", NULL);
        break;
    }

    return name;
}

static char *ptcl_type_to_present_string_copy(ptcl_type type)
{
    char *is_static = type.is_static ? "static " : "";
    char *name;

    switch (type.type)
    {
    case ptcl_value_type_type:
        name = ptcl_string(is_static, type.comp_type->identifier.value, NULL);
        break;
    case ptcl_value_typedata_type:
        name = ptcl_string(is_static, "typedata (", type.typedata.value, ")", NULL);
        break;
    case ptcl_value_array_type:
    {
        char *array_name = ptcl_type_to_present_string_copy(*type.array.target);
        name = ptcl_string(is_static, "array (", array_name, ")", NULL);
        free(array_name);
        break;
    }
    case ptcl_value_function_pointer_type:
    {
        char *return_type = ptcl_type_to_present_string_copy(*type.function_pointer.return_type);
        name = ptcl_string(is_static, "function (", NULL);
        for (size_t i = 0; i < type.function_pointer.count; i++)
        {
            char *argument = ptcl_type_to_present_string_copy(type.function_pointer.arguments[i].type);
            name = ptcl_string_append(name, argument, NULL);

            if (i != type.function_pointer.count - 1)
            {
                name = ptcl_string_append(name, ", ", NULL);
            }

            free(argument);
        }

        name = ptcl_string_append(name, "): ", return_type, NULL);
        free(return_type);
        break;
    }
    case ptcl_value_pointer_type:
        if (type.pointer.is_any || type.pointer.is_null)
        {
            name = ptcl_string(is_static, "pointer", NULL);
        }
        else
        {
            char *pointer_name = ptcl_type_to_present_string_copy(*type.pointer.target);
            name = ptcl_string(is_static, "pointer (", pointer_name, ")", NULL);
            free(pointer_name);
        }
        break;
    case ptcl_value_word_type:
        name = ptcl_string(is_static, "word", NULL);
        break;
    case ptcl_value_object_type_type:
    {
        char *target_present = ptcl_type_to_present_string_copy(*type.object_type.target);
        name = ptcl_string(is_static, "object type (", target_present, ")", NULL);
        free(target_present);
        break;
    }
    case ptcl_value_character_type:
        name = ptcl_string(is_static, "character", NULL);
        break;
    case ptcl_value_double_type:
        name = ptcl_string(is_static, "double", NULL);
        break;
    case ptcl_value_float_type:
        name = ptcl_string(is_static, "float", NULL);
        break;
    case ptcl_value_integer_type:
        name = ptcl_string(is_static, "integer", NULL);
        break;
    case ptcl_value_any_type:
        name = ptcl_string(is_static, "any", NULL);
        break;
    case ptcl_value_void_type:
        name = ptcl_string(is_static, "void", NULL);
        break;
    default:
        name = ptcl_string(is_static, "unknown", NULL);
        break;
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

static void ptcl_name_destroy(ptcl_name name)
{
    if (name.is_free)
    {
        free(name.value);
    }
}

static void ptcl_attribute_destroy(ptcl_attribute attribute)
{
    ptcl_name_destroy(attribute.name);
}

static void ptcl_attributes_destroy(ptcl_attributes attributes)
{
    for (size_t i = 0; i < attributes.count; i++)
    {
        ptcl_attribute_destroy(attributes.attributes[i]);
    }

    free(attributes.attributes);
}

static void ptcl_statement_func_body_destroy(ptcl_statement_func_body func_body)
{
    for (size_t i = 0; i < func_body.count; i++)
    {
        ptcl_statement_destroy(func_body.statements[i]);
    }

    free(func_body.statements);
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
    ptcl_expression_destroy(if_expr.condition);
    ptcl_expression_destroy(if_expr.body);
    ptcl_expression_destroy(if_expr.else_body);
}

static bool ptcl_type_is_function(ptcl_type type, ptcl_type_functon_pointer_type *function)
{
    if (type.type == ptcl_value_function_pointer_type)
    {
        *function = type.function_pointer;
        return true;
    }

    if (type.type == ptcl_value_array_type)
    {
        return ptcl_type_is_function(*type.array.target, function);
    }
    else if (type.type == ptcl_value_pointer_type)
    {
        if (type.pointer.is_any || type.pointer.is_null)
        {
            return false;
        }

        return ptcl_type_is_function(*type.pointer.target, function);
    }
    else if (type.type == ptcl_value_type_type)
    {
        for (size_t i = 0; i < type.comp_type->count; i++)
        {
            ptcl_type_member member = type.comp_type->types[i];
            if (!ptcl_type_is_function(member.type, function))
            {
                continue;
            }

            return true;
        }
    }

    return false;
}

static void ptcl_type_destroy(ptcl_type type)
{
    switch (type.type)
    {
    case ptcl_value_pointer_type:
        if (type.pointer.target != NULL && !type.array.target->is_primitive)
        {
            ptcl_type_destroy(*type.pointer.target);
            free(type.pointer.target);
            type.pointer.target = NULL;
        }

        break;
    case ptcl_value_array_type:
        if (type.array.target != NULL && !type.array.target->is_primitive)
        {
            ptcl_type_destroy(*type.array.target);
            free(type.array.target);
            type.array.target = NULL;
        }

        break;
    case ptcl_value_object_type_type:
        if (type.object_type.target != NULL && !type.array.target->is_primitive)
        {
            ptcl_type_destroy(*type.object_type.target);
            free(type.object_type.target);
            type.object_type.target = NULL;
        }

        break;
    case ptcl_value_function_pointer_type:
        if (!type.function_pointer.return_type->is_primitive)
        {
            ptcl_type_destroy(*type.function_pointer.return_type);
            free(type.function_pointer.return_type);
        }

        for (size_t i = 0; i < type.function_pointer.count; i++)
        {
            ptcl_type_destroy(type.function_pointer.arguments[i].type);
        }

        free(type.function_pointer.arguments);
        break;
    case ptcl_value_type_type:
    case ptcl_value_typedata_type:
    case ptcl_value_word_type:
    case ptcl_value_character_type:
    case ptcl_value_double_type:
    case ptcl_value_float_type:
    case ptcl_value_integer_type:
    case ptcl_value_any_type:
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
        ptcl_expression_destroy(identifier.value);
    }
    else
    {
        ptcl_name_destroy(identifier.name);
    }
}

static void ptcl_statement_func_call_destroy(ptcl_statement_func_call func_call)
{
    ptcl_identifier_destroy(func_call.identifier);
    for (size_t i = 0; i < func_call.count; i++)
    {
        ptcl_expression_destroy(func_call.arguments[i]);
    }

    free(func_call.arguments);
}

static void ptcl_argument_destroy(ptcl_argument argument)
{
    ptcl_name_destroy(argument.name);
    if (!argument.is_variadic)
    {
        ptcl_type_destroy(argument.type);
    }
}

static void ptcl_statement_func_decl_destroy(ptcl_statement_func_decl func_decl)
{
    ptcl_name_destroy(func_decl.name);
    ptcl_type_destroy(func_decl.return_type);
    if (!ptcl_statement_modifiers_flags_prototype(func_decl.modifiers) && func_decl.func_body != NULL)
    {
        ptcl_statement_func_body_destroy(*func_decl.func_body);
        free(func_decl.func_body);
    }

    for (size_t i = 0; i < func_decl.count; i++)
    {
        ptcl_argument_destroy(func_decl.arguments[i]);
    }

    free(func_decl.arguments);
}

static void ptcl_statement_type_decl_destroy(ptcl_statement_type_decl type_decl)
{
    ptcl_name_destroy(type_decl.name);
    for (size_t i = 0; i < type_decl.types_count; i++)
    {
        ptcl_type_destroy(type_decl.types[i].type);
    }

    free(type_decl.types);

    if (type_decl.body != NULL)
    {
        ptcl_statement_func_body_destroy(*type_decl.body);
        free(type_decl.body);
    }
}

static void ptcl_statement_assign_destroy(ptcl_statement_assign statement)
{
    ptcl_identifier_destroy(statement.identifier);
    if (statement.value != NULL)
    {
        ptcl_expression_destroy(statement.value);
    }

    if (statement.with_type && statement.is_define)
    {
        ptcl_type_destroy(statement.type);
    }
}

static void ptcl_typedata_member_destroy(ptcl_argument typedata)
{
    ptcl_name_destroy(typedata.name);
    ptcl_type_destroy(typedata.type);
}

static void ptcl_statement_typedata_decl_destroy(ptcl_statement_typedata_decl typedata)
{
    ptcl_name_destroy(typedata.name);
    for (size_t i = 0; i < typedata.count; i++)
    {
        ptcl_typedata_member_destroy(typedata.members[i]);
    }

    free(typedata.members);
}

static void ptcl_statement_destroy(ptcl_statement *statement)
{
    if (!statement->is_original)
    {
        free(statement);
        return;
    }

    ptcl_attributes_destroy(statement->attributes);
    switch (statement->type)
    {
    case ptcl_statement_func_call_type:
        ptcl_statement_func_call_destroy(statement->func_call);
        break;
    case ptcl_statement_func_decl_type:
        ptcl_statement_func_decl_destroy(statement->func_decl);
        break;
    case ptcl_statement_type_decl_type:
        ptcl_statement_type_decl_destroy(statement->type_decl);
        break;
    case ptcl_statement_typedata_decl_type:
        ptcl_statement_typedata_decl_destroy(statement->typedata_decl);
        break;
    case ptcl_statement_return_type:
        if (statement->ret.value != NULL)
        {
            ptcl_expression_destroy(statement->ret.value);
        }

        break;
    case ptcl_statement_assign_type:
        ptcl_statement_assign_destroy(statement->assign);
        break;
    case ptcl_statement_if_type:
        ptcl_statement_if_destroy(statement->if_stat);
        break;
    case ptcl_statement_func_body_type:
        if (statement->body.arguments != NULL)
        {
            free(statement->body.func_call.arguments);
        }

        ptcl_statement_func_body_destroy(statement->body);
        break;
    case ptcl_statement_each_type:
    case ptcl_statement_syntax_type:
    case ptcl_statement_unsyntax_type:
    case ptcl_statement_undefine_type:
    case ptcl_statement_import_type:
    case ptcl_statement_none_type:
        break;
    }

    free(statement);
}

static void ptcl_expression_ctor_destroy(ptcl_expression_ctor ctor)
{
    for (size_t i = 0; i < ctor.count; i++)
    {
        ptcl_expression_destroy(ctor.values[i]);
    }

    free(ctor.values);
}

static void ptcl_expression_array_destroy(ptcl_expression_array array)
{
    for (size_t i = 0; i < array.count; i++)
    {
        ptcl_expression_destroy(array.expressions[i]);
    }

    free(array.expressions);
    ptcl_type_destroy(array.type);
}

static void ptcl_expression_inside_destroy(ptcl_expression *expression)
{
    if (!expression->is_original)
    {
        if (expression->with_type)
        {
            ptcl_type_destroy(expression->return_type);
        }

        return;
    }

    if (expression->with_type && ptcl_expression_with_own_type(expression->type))
    {
        ptcl_type_destroy(expression->return_type);
    }

    switch (expression->type)
    {
    case ptcl_expression_lated_func_body_type:
        break;
    case ptcl_expression_func_call_type:
        ptcl_statement_func_call_destroy(expression->func_call);
        break;
    case ptcl_expression_array_type:
        ptcl_expression_array_destroy(expression->array);
        break;
    case ptcl_expression_binary_type:
        ptcl_expression_destroy(expression->binary.left);
        ptcl_expression_destroy(expression->binary.right);
        break;
    case ptcl_expression_cast_type:
        ptcl_expression_destroy(expression->cast.value);
        if (expression->cast.is_free)
        {
            ptcl_type_destroy(expression->cast.type);
        }

        break;
    case ptcl_expression_unary_type:
        ptcl_expression_destroy(expression->unary.child);
        break;
    case ptcl_expression_dot_type:
        if (expression->dot.is_name)
        {
            ptcl_name_destroy(expression->dot.name);
        }
        else
        {
            ptcl_expression_destroy(expression->dot.right);
        }

        ptcl_expression_destroy(expression->dot.left);
        break;
    case ptcl_expression_ctor_type:
        ptcl_expression_ctor_destroy(expression->ctor);
        break;
    case ptcl_expression_if_type:
        ptcl_expression_if_destroy(expression->if_expr);
        break;
    case ptcl_expression_word_type:
        ptcl_name_destroy(expression->word);
        break;
    case ptcl_expression_array_element_type:
        ptcl_expression_destroy(expression->array_element.value);
        ptcl_expression_destroy(expression->array_element.index);
        break;
    case ptcl_expression_variable_type:
        ptcl_name_destroy(expression->variable.name);
        break;
    case ptcl_expression_in_statement_type:
        ptcl_statement_destroy(expression->internal_statement);
        break;
    case ptcl_expression_in_expression_type:
        break;
    case ptcl_expression_in_token_type:
        ptcl_token_destroy(expression->internal_token);
        break;
    case ptcl_expression_object_type_type:
        // ptcl_type_destroy(expression->object_type.type);
        break;
    case ptcl_expression_character_type:
    case ptcl_expression_double_type:
    case ptcl_expression_float_type:
    case ptcl_expression_integer_type:
    case ptcl_expression_null_type:
        break;
    }
}

static void ptcl_expression_destroy(ptcl_expression *expression)
{
    ptcl_expression_inside_destroy(expression);
    free(expression);
}

#endif // PTCL_NODE_H
