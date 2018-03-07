#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define READ 0 /* Read end of pipe */
#define WRITE 1 /* Write end of pipe */

void *page;

int pagesize,
        parent,
        child[2],
        pipefd[2][2];

int lastUpdatedPID;

// Child specific
int number;
int currentProtection;

// HELPER FUNCTIONS
int pidOfOtherChild(int ownPid){
    if (child[0] == ownPid){
        return child[1];
    }
    return child[0];
}

int childNumber(int ownPid){
    if (child[0] == ownPid){
        return 0;
    }
    return 1;
}


void handler(int sig, siginfo_t *si, void *unused){
    if(currentProtection == PROT_READ) {
        printf("%d: in handler for writing\n", getpid());
        mprotect(si->si_addr, pagesize, PROT_WRITE | PROT_READ);
        kill(parent, SIGUSR1);
    } else {
        printf("%d: in handler for reading\n", getpid());
        mprotect(si->si_addr, pagesize, PROT_WRITE | PROT_READ);
        kill(parent, SIGUSR2);
    }

    //printf("%d : Writing to parent from child %d! \n", getpid(), number );
    //close(pipefd[number][READ]);
    //write(pipefd[number][WRITE], &page, sizeof(pagesize));
    //printf("%d : Writing done! \n", getpid());
}

void sig1Handler(int sig, siginfo_t *si, void *unused){
    printf("%d: in sig1Handler\n", getpid());
    if(getpid() == parent){
        lastUpdatedPID = si->si_pid;
        printf("%d: Updating other child \n", getpid());
        kill(pidOfOtherChild(si->si_pid),SIGUSR2); // Notify other child of updated page
    } else {
        printf("%d: writing page info with value %d from child number %d\n", getpid(), *(int *)page, number);
        close(pipefd[number][READ]);
        write(pipefd[number][WRITE], &page, sizeof(pagesize));
        close(pipefd[number][WRITE]);
        printf("%d: writing completed \n", getpid());
    }
    //printf("%d : Reading from = %d ! \n", getpid(), childNumber(si->si_pid));
    //close(pipefd[childNumber(si->si_pid)][WRITE]);
    //read(pipefd[childNumber(si->si_pid)][READ], &page, sizeof(pagesize));
    //printf("%d : Reading from %d done and got %d! \n", getpid(), childNumber(si->si_pid), *page);
    //*page = 1 - *page
}

void sig2Handler(int sig, siginfo_t *si, void *unused){
    printf("%d: in sig2Handler\n", getpid());
    if(getpid() == parent) {
        printf("%d: retrieving most up to date list from %d, which is child %d \n", getpid(), lastUpdatedPID, childNumber(lastUpdatedPID));
        kill(lastUpdatedPID, SIGUSR1);
        close(pipefd[childNumber(lastUpdatedPID)][WRITE]);
        read(pipefd[childNumber(lastUpdatedPID)][READ], &page, sizeof(pagesize));
        close(pipefd[childNumber(lastUpdatedPID)][READ]);
        printf("%d: retrieved the new x = %d\n", getpid(), *(int *)page);
    } else {    // in child
        printf("%d: protection set to PROT_NONE \n", getpid());
        mprotect(page, pagesize, PROT_NONE);
        currentProtection = PROT_NONE;
    }
}

void procPingPong(const int whoami, int *sharedTurnVariable) {
    printf("%d : Starting proces! \n", getpid());
    for(int count = 0; count < 5; ++count) {
        //printf("%d : Shared variable = %d \n", getpid(), *sharedTurnVariable);
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

    parent = getpid();

    struct sigaction sa = createSigAction(handler, 10);
    struct sigaction sa1 = createSigAction(sig1Handler, SIGUSR1);
    struct sigaction sa2 = createSigAction(sig2Handler, SIGUSR2);

    pagesize = sysconf(_SC_PAGE_SIZE);

    if (posix_memalign(&page, pagesize, pagesize) != 0) {
        fprintf(stderr, "Fatal error: aligned memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    int *sharedVariable = page;

    mprotect(page, pagesize, PROT_READ);
    currentProtection = PROT_READ;

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
        } else if(!child[i]){
            number = i;
            child[i] = getpid();
            printf("Child %d has pid : %d\n", number, getpid());
            procPingPong(i, sharedVariable);
        }
    }

    waitpid(child[0], &status, 0);
    waitpid(child[1], &status, 0);

    free(page);

    exit(EXIT_SUCCESS);
}
