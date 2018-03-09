CC=gcc
CFLAGS=-Wall -std=c99
all: scanner
	${CC} -o a.out ${CFLAGS} lex.yy.c -lfl

scanner: scanner.fl
	flex scanner.fl
clean:
	rm -f lex.yy.c
	rm -f *.o
	rm -f *~
