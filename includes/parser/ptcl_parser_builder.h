#ifndef PTCL_PARSER_BUILDER_H
#define PTCL_PARSER_BUILDER_H

#include <stdlib.h>
#include <ptcl_parser.h>

typedef struct ptcl_typedata_builder
{
    char *name;
    ptcl_statement_func_body *root;
    ptcl_typedata_member *members;
    size_t count;
} ptcl_typedata_builder;

typedef struct ptcl_func_built_in_builder
{
    char *name;
    ptcl_statement_func_body *root;
    ptcl_built_in_function_t bind;
    ptcl_type return_type;
    ptcl_argument *arguments;
    size_t count;
} ptcl_func_built_in_builder;

static ptcl_func_built_in_builder ptcl_func_built_in_builder_create(char *name)
{
    return (ptcl_func_built_in_builder){
        .name = name,
        .root = NULL,
        .return_type = ptcl_type_void,
        .arguments = NULL,
        .count = 0};
}

static ptcl_parser_instance ptcl_func_built_in_builder_build(ptcl_func_built_in_builder *builder)
{
    return ptcl_parser_built_in_create(
        builder->root,
        ptcl_name_create_fast_w(builder->name, false),
        builder->bind,
        builder->arguments,
        builder->count,
        builder->return_type);
}

static void ptcl_func_built_in_builder_destroy(ptcl_func_built_in_builder *builder)
{
    if (builder->count > 0)
    {
        for (size_t i = 0; i < builder->count; i++)
        {
            ptcl_argument_destroy(builder->arguments[i]);
        }

        free(builder->arguments);
    }
}

static void ptcl_func_built_in_builder_return(ptcl_func_built_in_builder *builder, ptcl_type type)
{
    builder->return_type = type;
}

static void ptcl_func_built_in_builder_bind(ptcl_func_built_in_builder *builder, ptcl_built_in_function_t bind)
{
    builder->bind = bind;
}

static bool ptcl_func_built_in_builder_add_argument(ptcl_func_built_in_builder *builder, ptcl_type type)
{
    ptcl_argument *buffer = realloc(builder->arguments, (builder->count + 1) * sizeof(ptcl_argument));
    if (buffer == NULL)
    {
        return false;
    }

    builder->arguments = buffer;
    builder->arguments[builder->count++] = ptcl_argument_create(type, ptcl_name_create_fast_w("", false));
    return true;
}

static ptcl_typedata_builder ptcl_typedata_builder_create(char *name)
{
    return (ptcl_typedata_builder){
        .name = name,
        .root = NULL,
        .members = NULL,
        .count = 0};
}

static ptcl_parser_instance ptcl_typedata_builder_build(ptcl_typedata_builder builder)
{
    return ptcl_parser_typedata_create(builder.root, ptcl_name_create_fast_w(builder.name, false), builder.members, builder.count);
}

static void ptcl_typedata_builder_destroy(ptcl_typedata_builder *builder)
{
    if (builder->count > 0)
    {
        for (size_t i = 0; i < builder->count; i++)
        {
            ptcl_typedata_member_destroy(builder->members[i]);
        }

        free(builder->members);
    }
}

static bool ptcl_typedata_builder_add_member(ptcl_typedata_builder *builder, char *name, ptcl_type type)
{
    ptcl_typedata_member *buffer = realloc(builder->members, (builder->count + 1) * sizeof(ptcl_typedata_member));
    if (buffer == NULL)
    {
        return false;
    }

    builder->members = buffer;
    builder->members[builder->count] = ptcl_typedata_member_create(ptcl_name_create_fast_w(name, false), type, builder->count);
    builder->count++;
    return true;
}

#endif // PTCL_PARSER_BUILDER_H