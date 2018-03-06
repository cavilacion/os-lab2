#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mman.h>

long pagesize;
int cnt;


void handler(int sig, siginfo_t *si, void *unused) {
    printf("%d : Handling SIGSEGV \n", getpid());
    printf("Got SIGSEGV at address: 0x%lx\n", (long) si->si_addr);
    mprotect(si->si_addr, pagesize, PROT_NONE);
    exit(EXIT_SUCCESS);
}

void procPingPong(int whoami) {
    printf("%d - %d: Starting \n", getpid(), whoami);

    void *buffer;
    int *shared;
    if (posix_memalign(&buffer, pagesize, 4*pagesize) != 0) {
        fprintf(stderr, "Fatal error: aligned memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    mprotect(buffer, 4*pagesize, PROT_READ);

    shared = buffer;
    *shared = 0;
    printf("TDASDFA\n");

    //exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {
    pid_t childOne, childTwo;

    int chanPing[2], chanPong[2], status;
    //pipe(chanPing);
    //pipe(chanPong);

    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    sigaction(SIGSEGV, &sa, NULL);

    pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize == -1)
        exit(EXIT_FAILURE);

    if ((childOne = fork()) == 0) {
        printf("Test %d\n", getpid());
        procPingPong(0);
    }
    /*if ((childTwo = fork()) == 0) {
        //printf("Test %d\n", getpid());
        //procPingPong(1);
    }*/

    waitpid(childOne, &status, 0);
    //waitpid(childTwo, &status, 0);

    printf("Master : %d\n", cnt);

    return EXIT_SUCCESS;
}