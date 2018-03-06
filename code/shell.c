#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

/* Function splits a string into pieces, seperated by spaces
 * Input is a string, output is an array of strings
 */
char **strspl (char *str) {
	char **argv=malloc (256*sizeof(char*));
	int i=0;
	char *arg;
	do {
		arg=strtok(i==0?str:NULL," "); 
		argv[i++]=arg;
	} while (arg!=NULL);
	return argv;
}

void type_prompt () {
	printf ("mini-shell $ ");
}

void read_command (char *cmd, char **param) {
	char ch;
	int i = 0, cmdSize = 256, numParam = 64, paramSize = 256;
	do {
		ch = getch ();
		if (ch
	} while (

int main (int argc, char *argv[]) {
	int status;
	char *command;
	char **parameters;
	while (TRUE) {
		type_prompt();
		read_command(command, parameters);
		
		if (fork() != 0) {
			waitpid (-1, &status, 0);
		}
		else {
			execve (command, parameters, 0);
		}
	}
	return 0;
}
