CC=gcc
CFLAGS=-Wall -std=c99
all: scanner shell.c parser.c
	${CC} -o minishell ${CFLAGS} lex.yy.c shell.c parser.c -lfl

scanner: scanner.fl
	flex scanner.fl
clean:
	rm -f lex.yy.c
	rm -f *.o
	rm -f *~
