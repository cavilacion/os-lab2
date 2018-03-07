#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>

#define READ 0 /* Read end of pipe */
#define WRITE 1 /* Write end of pipe */

void *page;

size_t pagesize;
int parent,
        child[2];
int retrieve_from_child[2];
int retrieve_from_parent[2];

int lastUpdatedPID;

// Child specific
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

char *currentProcess(){
    if(getpid() == parent){
        return "PARENT";
    } return "CHILD";
}

int retrieveUpToDateListFromChild(){
    printf("%s(%d): retrieving most up to date list from %d,\n", currentProcess(), getpid(), lastUpdatedPID);
    kill(lastUpdatedPID, SIGUSR1);
    //mprotect(page, pagesize, PROT_WRITE | PROT_NONE);
    //close(retrieve_from_child[WRITE]);
    read(retrieve_from_child[READ], page, pagesize);
    //close(retrieve_from_child[READ]);
    printf("%s(%d): retrieved the new x = %d\n", currentProcess(), getpid(), *(int *)page);
}

int sendUpToDateListToParent(){
    printf("%s(%d): writing page info with value %d\n", currentProcess(), getpid(), *(int *)page);
    //close(retrieve_from_child[READ]);
    write(retrieve_from_child[WRITE], page, pagesize);
    //close(retrieve_from_child[WRITE]);
    printf("%s(%d): writing completed \n", currentProcess(), getpid());
}

int sendUpToDateListToChild(){
    printf("%s(%d): writing page info with value %d\n", currentProcess(), getpid(), *(int *)page);
    //close(retrieve_from_parent[READ]);
    write(retrieve_from_parent[WRITE], page, pagesize);
    //close(retrieve_from_parent[WRITE]);
    printf("%s: writing completed \n", getpid());
}

int retrieveUpToDateListFromParent(){
    kill(parent, SIGUSR2);
    printf("%s(%d): retrieving list from parent\n", currentProcess(), getpid());
    mprotect(page, pagesize, PROT_WRITE);
    //close(retrieve_from_parent[WRITE]);
    read(retrieve_from_parent[READ], page, pagesize);
    //close(retrieve_from_parent[READ]);
    printf("%s(%d): retrieved the new x form parent = %d\n", currentProcess(), getpid(), *(int *)page);
}

void handler(int sig, siginfo_t *si, void *unused){
    if(getpid() == parent){
        exit(EXIT_SUCCESS);
    }
    if(currentProtection == PROT_READ) {
        printf("%s(%d): in handler for writing\n", currentProcess(), getpid());
        mprotect(page, pagesize, PROT_WRITE | PROT_WRITE);
        currentProtection = PROT_WRITE;
        kill(parent, SIGUSR1);
        printf("Parent = %d\n", parent);
    } else if(currentProtection == PROT_NONE){
        printf("%s(%d): in handler for reading\n", currentProcess(), getpid());
        retrieveUpToDateListFromParent();
        mprotect(page, pagesize, PROT_READ);
        currentProtection = PROT_READ;
    } else {
        printf("NO CLUE\n");
    }
}

void sig1Handler(int sig, siginfo_t *si, void *unused){
    printf("%s(%d): in sig1Handler\n", currentProcess(), getpid());
    if(getpid() == parent){
        //mprotect(page, pagesize, PROT_WRITE | PROT_NONE);
        lastUpdatedPID = si->si_pid;
        printf("%s(%d): Updating other child \n", currentProcess(), getpid());
        kill(pidOfOtherChild(si->si_pid),SIGUSR2); // Notify other child of updated page
    } else {
        sendUpToDateListToParent();
    }
}

void sig2Handler(int sig, siginfo_t *si, void *unused){
    printf("%s(%d): in sig2Handler\n", currentProcess(), getpid());
    if(getpid() == parent) {
        retrieveUpToDateListFromChild();
        sendUpToDateListToChild();
    } else {    // in child
        printf("%s(%d): protection set to PROT_NONE \n", currentProcess(), getpid());
        mprotect(page, pagesize, PROT_NONE);
        currentProtection = PROT_NONE;
    }
}

void procPingPong(const int whoami, int *sharedTurnVariable) {
    printf("%s(%d): Starting proces! \n", currentProcess(), getpid());
    for(int count = 0; count < 5; ++count) {
        printf("%d : Shared variable = %d \n", getpid(), *sharedTurnVariable);
        while(whoami != *sharedTurnVariable)
            ; //busy waiting
        printf(whoami == 0 ? "Ping\n" : "...Pong\n");
        *sharedTurnVariable = 1 - whoami;
        printf("%s(%d): Shared variable set to = %d \n", currentProcess(), getpid(), *sharedTurnVariable);
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
    struct sigaction sa1 = createSigAction(handler, 11);
    struct sigaction sa2 = createSigAction(sig1Handler, SIGUSR1);
    struct sigaction sa3 = createSigAction(sig2Handler, SIGUSR2);

    pagesize = sysconf(_SC_PAGE_SIZE);

    if (posix_memalign(&page, pagesize, pagesize) != 0) {
        fprintf(stderr, "Fatal error: aligned memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    int *sharedVariable = page;

        if (pipe(retrieve_from_child) == -1) {
            perror("pipe\n");
            exit(EXIT_FAILURE);
        }
    if (pipe(retrieve_from_parent) == -1) {
        perror("pipe\n");
        exit(EXIT_FAILURE);
    }


    for(int i = 0;i<2;i++){
        child[i] = fork();
        if(child[i] < 0){
            perror("Fork failed: abort\n");
            exit(EXIT_FAILURE);
        } else if(!child[i]){
            mprotect(page, pagesize, PROT_READ);
            currentProtection = PROT_READ;
            procPingPong(i, sharedVariable);
        }
    }

    waitpid(child[0], &status, 0);
    waitpid(child[1], &status, 0);

    free(page);

    exit(EXIT_SUCCESS);
}
