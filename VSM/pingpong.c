#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

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

int main3(int argc, char *argv[]) {
  int chanPing[2], chanPong[2], status;
  
  pipe(chanPing);
  pipe(chanPong);
  if (fork() == 0) {
    procPing(chanPong[0], chanPing[1]);
  }
  if (fork() == 0) {
    procPong(chanPing[0], chanPong[1]);
  }
  waitpid(-1, &status, 0);
  waitpid(-1, &status, 0);
  return EXIT_SUCCESS;
}
