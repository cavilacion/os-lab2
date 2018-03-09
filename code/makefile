CC=gcc
CFLAGS=-O2 -Wall
all: scanner parser
	${CC} -o a.out ${CFLAGS} parser.tab.c -ll -lm -lfl

scanner: scanner.fl
	flex scanner.fl
parser: parser.y
	bison -d -v parser.y
clean:
	rm -f parser.tab.c
	rm -f parser.tab.h
	rm -f lex.yy.c
	rm -f *.o
	rm -f *~
