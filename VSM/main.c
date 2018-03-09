/*
 * This program illustrated the concept of virtually shared memory. It does so by means of a simple ping pong program.
 *
 * A complete description of the thought process behind this program and a explanation of our implementation descisions
 * can be found in our report. Nevertheless, we will give a small overview of the most important concepts below.
 *
 * Processes
 * There are two types of processes. First there is the memory management process, or MMP in short, which functions as
 * as intermediary between the processes sharing the virtual memory. The processes sharing memory are called ping pong
 * processes or PP for short.
 *
 * States
 * Processes sharing the memory are always in one of three states, synchronize, leading or outdated. These states
 * represent how their memory related to the virtualy shared memory. Based on the states a protection level is set for
 * the processes memory.
 */

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define READ 0 /* Read end of pipe */
#define WRITE 1 /* Write end of pipe */

void *page;
size_t pagesize;
int parent;

// pipes
int retrieve_from_child[2];
int retrieve_from_parent[2];

// Child specific
typedef enum state{
    SYNCHRONIZED,
    LEADING,
    OUTDATED
} State;
int state;

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

void safeRead(int pipe, void *p, size_t size){
    ssize_t result = read(pipe, p, size);
    if(result < 0 ){
        perror("read failed\n");
        exit(EXIT_FAILURE);
    }
}

void safeWrite(int pipe, void *p, size_t size){
    ssize_t result = write(pipe, p, size);
    if(result < 0 ){
        perror("write failed\n");
        exit(EXIT_FAILURE);
    }
}

void sendSignal(int pid, int signal){
    if(kill(pid, signal)){
        perror("mprotect failed\n");
        exit(EXIT_FAILURE);
    }
}

// The funciton below can be used to create a signal handler for a specific signal
struct sigaction createSigAction(void (*handler)(int, siginfo_t *, void *), int sig){
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;

    sigaction(sig, &sa, NULL);

    return sa;
}

/// PARENT METHODS
//This function notifies all other sharing processes of a change in the virtually shared memory.
void notifyOthers(int sendingPID){
    for(int i = 0 ; i < 2 ; i++){
        if(child[i] == sendingPID) continue; // No need to notify yourself
        if(child[i] != -1){ // Child is still active
            sendSignal(child[i],SIGUSR2); // Notify other child of updated page
        }
    }
}

//When a child process terminates it should no longer be notified of changes in the shared memory.
void removeChildFromActives(int childPID){
    for(int i = 0 ; i < 2 ; i++){
        if(child[i] == childPID){
            child[i] = -1;
            return;
        }
    }
}

int retrieveUpToDateListFromChild(){
    safeRead(retrieve_from_child[READ], page, pagesize);
}

int sendUpToDateListToChild(){
    safeWrite(retrieve_from_parent[WRITE], page, pagesize);
}

/// CHILD METHODS
int sendUpToDateListToParent(){
    safeWrite(retrieve_from_child[WRITE], page, pagesize);
}

int retrieveUpToDateListFromParent(){
    sendSignal(parent, SIGUSR2);
    safeMProtect(page, pagesize, PROT_WRITE);
    safeRead(retrieve_from_parent[READ], page, pagesize);
}


/// AT EXIT
/*
 * Due to processes beign dependent on eachother memory a process can't simply die whenever it wishes to. This is an
 * important difference between the theoretical explanation of the states in our report and the states in our
 * implementation. Because we don't want to keep all process running indefinitly we made the decision to allow the
 * parent process to act as a leading process. This is further described in out report.
 *
 * What this means for this function is that we need to keep a process alive as long as it is a leading process.
 */
void closing(){
    if(parent != getpid() ){
       while(state == LEADING){ // The process can't die yet because it has the latest version
            pause();
        }
    }
}

/// SIGNAL HANDLERS
/*
 * This handler is used to catch segmentation faults. As these are forcefully triggered by using mprotect
 * we can use this handler to communicate with the MMP process.
 *
 * Based ont he current state of the process we know what action the process tried to perform that resulted in the
 * segmentation fault. Based on this we can initiate a state transition.
 */
void handler(int sig, siginfo_t *si, void *unused){
    if(state == SYNCHRONIZED) {
        // The child is trying to write
        sendSignal(parent, SIGUSR1);
        safeMProtect(page, pagesize, PROT_READ | PROT_WRITE);
        state = LEADING;
    } else if(state == OUTDATED){
        // The child is trying to write
        retrieveUpToDateListFromParent();
        safeMProtect(page, pagesize, PROT_READ);
        state = SYNCHRONIZED;
    }
}

/*
 * This handler is used to catch SIGUSR1 signals. It has a different function based on which process catches it.
 *
 * MMP:
 * The sender of the signal has updated it memory. The MMP should now notify all other processed of this change
 *
 * PP:
 * If aPP receives the SIGUSR1 signals this means the MMP needs the PPs current memory page. After doing so the current
 * process is no longer leading.
 */
void sig1Handler(int sig, siginfo_t *si, void *unused){
    if(getpid() == parent){
        lastUpdatedPID = si->si_pid;
        notifyOthers(si->si_pid);
    } else {
        sendUpToDateListToParent();
        state = SYNCHRONIZED;
    }
}

/*
 * This handler is used to catch SIGUSR2 signals. It has a different function based on which process catches it.
 *
 * MMP:
 * The sender of the signal wants to read or write to its memory but it knows that it is longer up to date. Therefore,
 * the MMP either sends it the MMPs own page if it knows that this page is uptodate or it retrieves the up to date page
 * and send the requesting process this page.
 *
 * PP:
 * If PP receives the SIGUSR2 another process has updated its memory. From this point on the PP is no longer allowed to
 * freely read or write to its memory.
 */
void sig2Handler(int sig, siginfo_t *si, void *unused){
    if(getpid() == parent) {
        if(lastUpdatedPID != 0){ // Parent is not most up to date
            sendSignal(lastUpdatedPID, SIGUSR1);
            retrieveUpToDateListFromChild();
            lastUpdatedPID = 0;
        }
        sendUpToDateListToChild();
    } else {    // in child
        safeMProtect(page, pagesize, PROT_NONE);
        state = OUTDATED;
    }
}

/*
 * As mentioned in the report and the description of the closing function we only allow a proces to die when it is not
 * a leading process. This requires us to register the exit of each PP in our MMP. Without doing so there would always
 * remain one process that is leading and would therefor not die. This function handles this.
 */
void sigChldHandler(int sig, siginfo_t *si, void *unused) {
    int count = 0;
    removeChildFromActives(si->si_pid);
    for(int i = 0 ; i < 2 ;i++){
        if(child[i] > 0){
            count++;
        }
    }
    if(count == 1){
        sendSignal(lastUpdatedPID, SIGUSR1);
        retrieveUpToDateListFromChild();
    }
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

    struct sigaction sa = createSigAction(handler, SIGBUS);
    struct sigaction sa1 = createSigAction(handler, SIGSEGV);
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
            state = SYNCHRONIZED;
            sleep(1);       // This is ugly, but we don't want to add more complexity by introducing a starting signal
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
