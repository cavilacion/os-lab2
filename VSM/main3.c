#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>

int pagesize,
        parent,
        child[2],
        parentMsg = 0,
        *page,
        pipefd[2][2];

void handler(int sig, siginfo_t *si, void *unused){
    printf("%d: in handler\n", getpid());
    //mprotect(si->si_addr, pagesize, PROT_WRITE | PROT_READ);
    //kill(parent, SIGUSR1);
    exit(EXIT_SUCCESS);
}

void sig1Handler(int sig, siginfo_t *si, void *unused){
    printf("%d: in sig1Handler\n", getpid());
    //*page = 1 - *page;
    kill(child[*page],SIGUSR2);
}

void sig2Handler(int sig, siginfo_t *si, void *unused){
    printf("%d: in sig2Handler\n", getpid());
    mprotect(&page, pagesize, PROT_WRITE);
    //*page = 1 - *page;
    mprotect(&page, pagesize, PROT_READ);
    sleep(1);
}

void procPingPong(const int whoami, int *sharedTurnVariable) {
    printf("%d : Starting proces! \n", getpid());
    for(int count = 0; count < 5; ++count) {
        printf("%d : Shared variable = %d \n", getpid(), *sharedTurnVariable);
        while(whoami != *sharedTurnVariable)
            ; //busy waiting
        printf(whoami == 0 ? "Ping\n" : "...Pong\n");
        *sharedTurnVariable = 1 - whoami;
        printf("%d : Shared variable set to = %d \n", getpid(), *sharedTurnVariable);
    }
    exit(EXIT_SUCCESS);
}


struct sigaction createSigAction(void (*handler)(int, siginfo_t *, void *), int sig){
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;

    sigaction(sig, &sa, NULL);

    return sa;
}

int main(int argc, char *argv[]){
    int  status = 0;
    void *buffer;

    parent = getpid();

    //struct sigaction sa = createSigAction(handler, SIGSEGV);
    struct sigaction sa1 = createSigAction(sig1Handler, SIGUSR1);
    struct sigaction sa2 = createSigAction(sig2Handler, SIGUSR2);

    pagesize = sysconf(_SC_PAGE_SIZE);

    if (posix_memalign(&buffer, pagesize, pagesize) != 0) {
        fprintf(stderr, "Fatal error: aligned memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    page = buffer;
    mprotect(page, pagesize, PROT_READ);

    for(int i = 0;i<2;i++){
        if (pipe(pipefd[i]) == -1) {
            perror("pipe\n");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0;i<2;i++){
        child[i] = fork();
        if(child[i] < 0){
            perror("Fork failed: abort\n");
            exit(EXIT_FAILURE);
        } else if(child[i]){
            procPingPong(i, page);
        }
    }

    waitpid(child[0], &status, 0);
    waitpid(child[1], &status, 0);

    free(buffer);

    exit(EXIT_SUCCESS);
}
