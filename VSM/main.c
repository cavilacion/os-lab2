#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mman.h>

void *buffer;
int pagesize;

void procPing(int fromPong, int toPong) {
    int msg = 0;
    while (msg < 5) {
        printf ("Ping\n");
        write(toPong, &msg, sizeof(int));
        read(fromPong, &msg, sizeof(int));
    }
    exit(EXIT_SUCCESS);
}

void procPong(int fromPing, int toPing) {
    int msg;
    do {
        read(fromPing, &msg, sizeof(int));
        printf ("...Pong\n");
        msg++;
        write(toPing, &msg, sizeof(int));
    } while (msg<5);
    exit(EXIT_SUCCESS);
}

void procPingPong(int whoami, volatile int *sharedTurnVariable) {
    /* whoami == 0 or whoami == 1 */
    printf("%d : Starting process\n", getpid());
    int msg = 0;
    printf("%d : mprotect \n", getpid());
    while (msg < 5) {
        while (*sharedTurnVariable != whoami); /* busy-waiting */
        printf (whoami == 0 ? "Ping\n" : "...Pong\n");
        *sharedTurnVariable = 1 - whoami;
        printf("%d\n", *sharedTurnVariable);
        msg++;
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    int start = 0;
    int *shared = &start;
    pid_t childOne, childTwo;


    int chanPing[2], chanPong[2], status;
    pipe(chanPing);
    pipe(chanPong);

    pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize == -1)
        exit(EXIT_FAILURE);

    /* Allocate a buffer aligned on a page boundary;
       initial protection is PROT_READ | PROT_WRITE
    if (posix_memalign(&buffer, pagesize, 4*pagesize) != 0) {
        //fprintf(stderr, "Fatal error: aligned memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }*/

    mprotect(shared, pagesize, PROT_WRITE | PROT_READ);

    *shared = 1;

    if ((childOne = fork()) == 0) {
        printf("Test %d\n", getpid());
        procPingPong(0, shared);
    }
    if ((childTwo = fork()) == 0) {
        printf("Test %d\n", getpid());
        procPingPong(1, shared);
    }

    waitpid(childOne, &status, 0);
    waitpid(childTwo, &status, 0);
    return EXIT_SUCCESS;
}