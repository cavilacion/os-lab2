CFLAGS=-std=c99

all: ring execute duplicates clean

copy:
	cp ring execute duplicates /usr/bin

ring: ring.c
	gcc $(CFLAGS) -o ring ring.c

execute: execute.c
	gcc $(CFLAGS) -o execute execute.c

duplicates: ex3/duplicates.c ex3/allocs.o ex3/directoryInfo.o
	gcc -g $(CFLAGS) -o duplicates ex3/duplicates.c ex3/allocs.o ex3/directoryInfo.o

clean: 
	rm -f *.o
	rm -f ex3/*.o
	rm -f #*

remove:
	rm -f ring execute duplicates


