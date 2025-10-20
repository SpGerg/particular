NAME = ptcl
TEST_NAME = ptcl_test
CC = gcc
CFLAGS = -o $(NAME) -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-variable
TEST_CFLAGS = -o $(TEST_NAME)

SOURCES = $(wildcard ../sources/*.c)
LEXER_INCLUDES = ./../includes/lexer/
PARSER_INCLUDES = ./../includes/parser/
TRANSPILER_INCLUDES = ./../includes/transpiler/
UTILITIES_INCLUDES = ./../includes/utilities/

debug:
	$(CC) $(CFLAGS) -O0 -g main.c $(SOURCES) \
	-I$(LEXER_INCLUDES) -I$(PARSER_INCLUDES) -I$(TRANSPILER_INCLUDES) -I$(UTILITIES_INCLUDES)

opt:
	$(CC) $(CFLAGS) -O3 -g main.c $(SOURCES) \
	-I$(LEXER_INCLUDES) -I$(PARSER_INCLUDES) -I$(TRANSPILER_INCLUDES) -I$(UTILITIES_INCLUDES)

tests:
	$(CC) $(TEST_CFLAGS) -g unit\test_parser.c $(SOURCES) \
	-I$(LEXER_INCLUDES) -I$(PARSER_INCLUDES) -I$(TRANSPILER_INCLUDES) -I$(UTILITIES_INCLUDES)
