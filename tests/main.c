#include <stdio.h>
#include <ptcl_transpiler.h>
#include <ptcl_parser.h>
#include <ptcl_lexer.h>

int main()
{
    char *source;
    FILE *target = fopen("script.ptcl", "rb");
    if (!target)
    {
        perror("Failed to open file");
        return 1;
    }

    fseek(target, 0, SEEK_END);
    long file_size = ftell(target);
    if (file_size < 0)
    {
        perror("Failed to get file size");
        fclose(target);
        return 1;
    }

    if (file_size == 0)
    {
        fclose(target);
        source = strdup("");
    }
    else
    {
        fseek(target, 0, SEEK_SET);
        source = malloc(file_size + 1);

        if (!source)
        {
            fclose(target);
            perror("Memory allocation failed");
            return 1;
        }

        size_t bytes_read = fread(source, 1, file_size, target);
        if (bytes_read != (size_t)file_size)
        {
            source[bytes_read] = '\0';
        }
        else
        {
            source[file_size] = '\0';
        }
    }

    fclose(target);

    ptcl_lexer_configuration configuration = ptcl_lexer_configuration_default();
    ptcl_lexer *lexer = ptcl_lexer_create("console", source, &configuration);
    ptcl_tokens_list tokens_list = ptcl_lexer_tokenize(lexer);

    free(source);

    ptcl_parser *parser = ptcl_parser_create(&tokens_list, &configuration);
    ptcl_parser_result result = ptcl_parser_parse(parser);

    if (result.count == 0)
    {
        ptcl_transpiler *transpiler = ptcl_transpiler_create(result);
        char *transpiled = ptcl_transpiler_transpile(transpiler);
        puts(transpiled);

        ptcl_transpiler_destroy(transpiler);
        free(transpiled);
    }
    else {
        for (size_t i = 0;i < result.count;i++) {
            printf("%s, at: %d\n", result.errors[i].message, result.errors[i].location.position);
        }
    }

    ptcl_tokens_list_destroy(tokens_list);
    ptcl_lexer_destroy(lexer);

    ptcl_parser_result_destroy(result);
    ptcl_parser_destroy(parser);
    return 0;
}