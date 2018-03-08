/*
 *      The  program  below  allocates  four pages of memory, makes them all 
 *      read-only, and then executes a loop that walks upward through the allocated
 *      region modifying bytes. Of course, the protection prevents this, so at the 
 *      start of each page, a SIGSEGV is generated, which in turn produces a jump to 
 *      a signal handler. That signal handler changes the protection such that writing 
 *      is allowed. On return from the handler, the instruction is restarted and the
 *      write succeeds. However, on the third call of the handler, the handler simply 
 *      aborts the program.
 *
 *      An example of what we might see when running the program is the following:
 *             $ ./a.out
 *             Start of region:        0xf9f000
 *             Got SIGSEGV at address: 0xf9f000
 *             Got SIGSEGV at address: 0xfa0000
 *             Got SIGSEGV at address: 0xfa1000
 */

	
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

int pagesize;
int cnt = 0;

void handler(int sig, siginfo_t *si, void *unused) {
  printf("Got SIGSEGV at address: 0x%lx\n", (long) si->si_addr);
  mprotect(si->si_addr, pagesize, PROT_WRITE);
  cnt++;
  if (cnt == 3) {
    exit(EXIT_FAILURE);
  }
  
}

int main2(int argc, char *argv[]) {
  void *buffer;
  char *p;
  struct sigaction _sigsegv;

  _sigsegv.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  _sigsegv.sa_sigaction = handler;
  sigaction(SIGSEGV, &_sigsegv, NULL);
  
  pagesize = sysconf(_SC_PAGE_SIZE);

  /* Allocate a buffer aligned on a page boundary;
   * default initial protection is PROT_READ | PROT_WRITE 
   */
  if (posix_memalign(&buffer, pagesize, 4*pagesize) != 0) {
    fprintf(stderr, "Fatal error: aligned memory allocation failed.\n");
    exit(EXIT_FAILURE);
  }

  printf("Start of region:        0x%lx\n", (long) buffer);
  mprotect(buffer, 4*pagesize, PROT_READ);
  
  p = buffer;
  while (1) {
    *(p++) = 'a';
  }
  
  printf("Loop completed\n");     /* Should never happen */
  return 0;
}
