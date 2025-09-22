#ifndef PTCL_PARSER_BUILDER_H
#define PTCL_PARSER_BUILDER_H

#include <stdlib.h>
#include <ptcl_parser.h>

typedef struct ptcl_typedata_builder
{
    char *name;
    ptcl_typedata_member *members;
    size_t count;
} ptcl_typedata_builder;

static ptcl_typedata_builder ptcl_typedata_builder_create(char *name)
{
    return (ptcl_typedata_builder){
        .name = name,
        .members = NULL,
        .count = 0};
}

static ptcl_parser_instance ptcl_typedata_builder_build(ptcl_typedata_builder builder)
{
    return ptcl_parser_typedata_create(ptcl_name_create_fast_w(builder.name, false), builder.members, builder.count);
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