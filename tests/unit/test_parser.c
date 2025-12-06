/*
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ptcl_lexer.h>
#include <ptcl_parser.h>

typedef struct
{
    const char *filename;
    const char *full_path;
    char *source_code;
    int (*test_function)(const char *filename, const char *source_code);
    void *custom_data;
} test_case_t;

#define MAX_TEST_CASES 100
static test_case_t test_cases[MAX_TEST_CASES];
static int test_case_count = 0;

int test_expected_success(const char *filename, const char *source_code)
{
    ptcl_lexer_configuration default_configuration = ptcl_lexer_configuration_default();
    ptcl_lexer *lexer = ptcl_lexer_create("test", source_code, &default_configuration);
    ptcl_tokens_list tokenized = ptcl_lexer_tokenize(lexer);
    ptcl_parser *parser = ptcl_parser_create(&tokenized, &default_configuration);
    ptcl_parser_result result = ptcl_parser_parse(parser);
    bool success = (result.count == 0);

    ptcl_parser_result_destroy(result);
    ptcl_parser_destroy(parser);
    ptcl_tokens_list_destroy(tokenized);
    ptcl_lexer_destroy(lexer);
    return success;
}

int test_expected_failure(const char *filename, const char *source_code)
{
    return !test_expected_success(filename, source_code);
}

int test_variables(const char *filename, const char *source_code)
{
    printf("  Running variables-specific test...\n");
    return test_expected_success(filename, source_code);
}

int test_functions(const char *filename, const char *source_code)
{
    printf("  Running functions-specific test...\n");
    return test_expected_success(filename, source_code);
}

int test_control_flow(const char *filename, const char *source_code)
{
    printf("  Running control-flow-specific test...\n");
    return test_expected_success(filename, source_code);
}

int (*get_test_function(const char *filename, bool expect_success))(const char *, const char *)
{
    if (strstr(filename, "variables"))
    {
        return test_variables;
    }
    else if (strstr(filename, "functions"))
    {
        return test_functions;
    }
    else if (strstr(filename, "if") || strstr(filename, "while") || strstr(filename, "for"))
    {
        return test_control_flow;
    }

    return expect_success ? test_expected_success : test_expected_failure;
}

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

void init_test_case(test_case_t *test_case, const char *path, const char *filename, bool expect_success)
{
    test_case->filename = strdup(filename);

    size_t full_path_len = strlen(path) + strlen(filename) + 2;
    char *full_path = malloc(full_path_len);
    snprintf(full_path, full_path_len, "%s/%s", path, filename);
    test_case->full_path = full_path;

    test_case->source_code = read_file_contents(full_path);
    test_case->test_function = get_test_function(filename, expect_success);
    test_case->custom_data = NULL;
}

void cleanup_test_case(test_case_t *test_case)
{
    free((void *)test_case->filename);
    free((void *)test_case->full_path);
    free(test_case->source_code);
    test_case->filename = NULL;
    test_case->full_path = NULL;
    test_case->source_code = NULL;
    test_case->test_function = NULL;
    test_case->custom_data = NULL;
}

void run_test_case(test_case_t *test_case, bool expect_success)
{
    if (!test_case || !test_case->source_code)
    {
        printf("[SKIP] %s (initialization error)\n", test_case->filename);
        return;
    }

    printf("Testing: %s\n", test_case->filename);

    int result = test_case->test_function(test_case->filename, test_case->source_code);
    int test_passed = (result == expect_success);

    if (test_passed)
    {
        printf("[PASS] %s\n", test_case->filename);
    }
    else
    {
        printf("[FAIL] %s - Expected %s but got %s\n",
               test_case->filename,
               expect_success ? "SUCCESS" : "FAILURE",
               result ? "SUCCESS" : "FAILURE");
    }
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

    test_case_count = 0;
    while ((entry = readdir(dp)) && test_case_count < MAX_TEST_CASES)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        if (!is_regular_file(path, entry->d_name))
        {
            continue;
        }

        init_test_case(&test_cases[test_case_count], path, entry->d_name, expect_success);
        if (!test_cases[test_case_count].source_code)
        {
            printf("[SKIP] %s (read error)\n", entry->d_name);
            cleanup_test_case(&test_cases[test_case_count]);
            continue;
        }

        run_test_case(&test_cases[test_case_count], expect_success);
        if (test_cases[test_case_count].test_function(test_cases[test_case_count].filename,
                                                      test_cases[test_case_count].source_code) == expect_success)
        {
            pass_count++;
        }

        test_count++;
        test_case_count++;
    }

    for (int i = 0; i < test_case_count; i++)
    {
        cleanup_test_case(&test_cases[i]);
    }

    test_case_count = 0;
    closedir(dp);
    printf("------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);
}

int main()
{
    printf("Starting parser tests...\n");

    run_tests_in_directory("C:\\Users\\spger\\Desktop\\particular-lang\\tests\\unit\\integration\\valid", true);
    run_tests_in_directory("C:\\Users\\spger\\Desktop\\particular-lang\\tests\\unit\\integration\\invalid", false);

    printf("\nAll test batches completed.\n");
    return 0;
}
*/