#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ptcl_lexer.h>
#include <ptcl_parser.h>

char *read_file_contents(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    if (buffer)
    {
        size_t read_len = fread(buffer, 1, length, file);
        buffer[read_len] = '\0';
    }
    else
    {
        fprintf(stderr, "Memory allocation failed for file: %s\n", filename);
    }

    fclose(file);
    return buffer;
}

int is_regular_file(const char *path, const char *filename)
{
    struct stat path_stat;
    char full_path[1024];

    snprintf(full_path, sizeof(full_path), "%s/%s", path, filename);
    if (stat(full_path, &path_stat) != 0)
    {
        return 0;
    }

    return S_ISREG(path_stat.st_mode);
}

void run_tests_in_directory(const char *path, bool expect_success)
{
    struct dirent *entry;
    DIR *dp = opendir(path);

    if (dp == NULL)
    {
        perror("opendir failed");
        return;
    }

    printf("\nRunning tests in: %s (expecting %s)\n", path,
           expect_success ? "SUCCESS" : "FAILURE");
    printf("==========================================\n");

    int test_count = 0;
    int pass_count = 0;
    while ((entry = readdir(dp)))
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        if (!is_regular_file(path, entry->d_name))
        {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        char *source_code = read_file_contents(full_path);
        if (source_code == NULL)
        {
            printf("[SKIP] %s (read error)\n", entry->d_name);
            continue;
        }

        ptcl_lexer_configuration default_configuration = ptcl_lexer_configuration_default();
        ptcl_lexer *lexer = ptcl_lexer_create("test", source_code, &default_configuration);
        ptcl_tokens_list tokenized = ptcl_lexer_tokenize(lexer);
        ptcl_parser *parser = ptcl_parser_create(&tokenized, &default_configuration);
        ptcl_parser_result result = ptcl_parser_parse(parser);
        int test_passed = (result.count == 0) == expect_success;
        if (test_passed)
        {
            printf("[PASS] %s\n", entry->d_name);
            pass_count++;
        }
        else
        {
            printf("[FAIL] %s\n", entry->d_name);
        }

        test_count++;
        free(source_code);
        ptcl_tokens_list_destroy(tokenized);
        ptcl_lexer_destroy(lexer);

        ptcl_parser_result_destroy(result);
        ptcl_parser_destroy(parser);
    }

    closedir(dp);
    printf("------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);
}

int main()
{
    printf("Starting parser tests...\n");
    // Only for windows now
    run_tests_in_directory("tests\\unit\\integration\\valid", true);
    run_tests_in_directory("tests\\unit\\integration\\invalid", false);
    printf("\nAll test batches completed.\n");
    return 0;
}