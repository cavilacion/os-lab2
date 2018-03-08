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
int parent;

// pipes
int retrieve_from_child[2];
int retrieve_from_parent[2];

// Child specific
int currentProtection;

// Parent specific
int lastUpdatedPID;
int child[2];

/// HELPER FUNCTIONS

void safeMProtect(void *p, size_t size, int prot){
    int result = mprotect(p, size, prot);
    if(result != 0){
        perror("mprotect failed\n");
        exit(EXIT_FAILURE);
    }
}

void sendSignal(int pid, int signal){
    if(kill(pid, signal)){
        perror("mprotect failed\n");
        exit(EXIT_FAILURE);
    }
}

struct sigaction createSigAction(void (*handler)(int, siginfo_t *, void *), int sig){
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;

    sigaction(sig, &sa, NULL);

    return sa;
}

/// PARENT METHODS

void notifyOthers(int sendingPID){
    for(int i = 0 ; i < 2 ; i++){
        if(child[i] == sendingPID) continue; // No need to notify yourself
        if(child[i] != -1){ // Child is still active
            sendSignal(child[i],SIGUSR2); // Notify other child of updated page
        }
    }
}

void removeChildFromActives(int childPID){
    for(int i = 0 ; i < 2 ; i++){
        if(child[i] == childPID){
            child[i] = -1;
            return;
        }
    }
}

int retrieveUpToDateListFromChild(int pid){
    close(retrieve_from_child[WRITE]);
    read(retrieve_from_child[READ], page, pagesize);
}

int sendUpToDateListToChild(){
    close(retrieve_from_parent[READ]);
    write(retrieve_from_parent[WRITE], page, pagesize);
}

/// CHILD METHODS
int sendUpToDateListToParent(){
    close(retrieve_from_child[READ]);
    write(retrieve_from_child[WRITE], page, pagesize);
}

int retrieveUpToDateListFromParent(){
    sendSignal(parent, SIGUSR2);
    safeMProtect(page, pagesize, PROT_WRITE);
    close(retrieve_from_parent[WRITE]);
    read(retrieve_from_parent[READ], page, pagesize);
}


/// AT EXIT
void closing(){
    if(parent == getpid()){ // Parent exit
        for(int i = 0; i < 2 ; i++){
            if(child[i] != -1){
                sendSignal(9, child[i]);
            }
        }
    } else {
        // Child is exiting, we need to make sure the parent is up to date
        sendUpToDateListToParent();
    }

}

/// SIGNAL HANDLERS
void handler(int sig, siginfo_t *si, void *unused){
    if(currentProtection == PROT_READ) {
        // The child is trying to write
        sendSignal(parent, SIGUSR1);
        safeMProtect(page, pagesize, PROT_READ | PROT_WRITE);
        currentProtection = PROT_READ | PROT_WRITE;
    } else if(currentProtection == PROT_NONE){
        retrieveUpToDateListFromParent();
        safeMProtect(page, pagesize, PROT_READ);
        currentProtection = PROT_READ;
    } else {
        printf("NO CLUE\n");
    }
}

void sig1Handler(int sig, siginfo_t *si, void *unused){
    if(getpid() == parent){
        lastUpdatedPID = si->si_pid;
        notifyOthers(si->si_pid);
    } else {
        sendUpToDateListToParent();
    }
}

void sig2Handler(int sig, siginfo_t *si, void *unused){
    if(getpid() == parent) {
        if(lastUpdatedPID != 0){ // Parent is not most up to date
            sendSignal(lastUpdatedPID, SIGUSR1);
            retrieveUpToDateListFromChild(lastUpdatedPID);
        }
        sendUpToDateListToChild();
    } else {    // in child
        safeMProtect(page, pagesize, PROT_NONE);
        currentProtection = PROT_NONE;
    }
}

void sigChldHandler(int sig, siginfo_t *si, void *unused){
    removeChildFromActives(si->si_pid);
    retrieveUpToDateListFromChild(si->si_pid);
    lastUpdatedPID = 0;
}


/// PING PONG PROCESS
void procPingPong(const int whoami, int *sharedTurnVariable) {
    for(int count = 0; count < 5; ++count) {
        while(whoami != *sharedTurnVariable)
            ; //busy waiting
        printf(whoami == 0 ? "Ping\n" : "...Pong\n");
        *sharedTurnVariable = 1 - whoami;
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){

    atexit(closing);

    parent = getpid();

    struct sigaction sa = createSigAction(handler, 10);
    struct sigaction sa1 = createSigAction(handler, 11);
    struct sigaction sa2 = createSigAction(sig1Handler, SIGUSR1);
    struct sigaction sa3 = createSigAction(sig2Handler, SIGUSR2);
    struct sigaction sa4 = createSigAction(sigChldHandler, SIGCHLD);

    pagesize = (size_t)sysconf(_SC_PAGE_SIZE);

    if (posix_memalign(&page, pagesize, pagesize) != 0) {
        fprintf(stderr, "Fatal error: aligned memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    int *sharedVariable = page;

    // Create communication pipes
    if (pipe(retrieve_from_child) == -1) {
        perror("pipe\n");
        exit(EXIT_FAILURE);
    }
    if (pipe(retrieve_from_parent) == -1) {
        perror("pipe\n");
        exit(EXIT_FAILURE);
    }

    // Start child processes
    for(int i = 0;i<2;i++){
        child[i] = fork();
        if(child[i] < 0){
            perror("Fork failed: abort\n");
            exit(EXIT_FAILURE);
        } else if(!child[i]){
            safeMProtect(page, pagesize, PROT_READ);
            currentProtection = PROT_READ;
            sleep(1);       // This is ugly and unsafe, but we don't want to add more complexity by introducing a starting signal
            procPingPong(i, sharedVariable);
        }
    }

    int  statusChildOne, statusChildTwo;
    do {
        waitpid(child[0], &statusChildOne, 0);
    } while(!WIFEXITED(statusChildOne));

    do{
        waitpid(child[1], &statusChildTwo, 0);
    } while(!WIFEXITED(statusChildTwo));

    free(page);

    exit(EXIT_SUCCESS);
}
