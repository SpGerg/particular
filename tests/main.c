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
    else
    {
        for (size_t i = 0; i < result.count; i++)
        {
            int error_pos = result.errors[i].location.position;

            // Самый тупой способ: ищем начало и конец строки
            int line_start = 0;
            int line_end = 0;
            int line_number = 1;

            // Считаем строки до позиции ошибки
            for (int j = 0; j < error_pos; j++)
            {
                if (source[j] == '\n')
                {
                    line_number++;
                    line_start = j + 1;
                }
            }

            // Ищем конец текущей строки
            line_end = error_pos;
            while (source[line_end] != '\0' && source[line_end] != '\n')
            {
                line_end++;
            }

            printf("Error at line %d, position %d:\n", line_number, error_pos - line_start);
            printf("  ");

            // Выводим строку с ошибкой
            for (int j = line_start; j < line_end; j++)
            {
                putchar(source[j]);
            }
            printf("\n  ");

            // Выводим указатель на ошибку
            for (int j = line_start; j < error_pos; j++)
            {
                putchar(' ');
            }
            printf("^\n");

            printf("Message: %s\n\n", result.errors[i].message);
        }
    }

    free(source);
    ptcl_tokens_list_destroy(tokens_list);
    ptcl_lexer_destroy(lexer);

    ptcl_parser_result_destroy(result);
    ptcl_parser_destroy(parser);
    return 0;
}