NAME = ptcl
CC = gcc
CFLAGS = -o $(NAME)

SOURCES = $(wildcard ../source/*.c)
LEXER_INCLUDES = ./../includes/lexer/
PARSER_INCLUDES = ./../includes/parser/
TRANSPILER_INCLUDES = ./../includes/transpiler/
UTILITIES_INCLUDES = ./../includes/utilities/

debug:
	$(CC) $(CFLAGS) -g main.c $(SOURCES) \
	-I$(LEXER_INCLUDES) -I$(PARSER_INCLUDES) -I$(TRANSPILER_INCLUDES) -I$(UTILITIES_INCLUDES)

lexer-parser:
	$(CC) $(CFLAGS) main.c $(SOURCES) -I$(LEXER_INCLUDES)