%{
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "parser.tab.h"
#include "lex.yy.c"

#define MAX_PROC 256

#define FAIL -1
#define SUCCESS 0
#define QUIT 1

typedef struct processData {
	int pid;
	char **argv;
	int argc;
	char *fdinPath;
	char *fdoutPath;
} processData;

typedef struct processData* process;

process *ptab;
int numProc=0;
int fdpipe[2];

int yyerror() {
  printf("wrong syntax\n");
  exit(EXIT_SUCCESS);
}

void type_prompt () {
	printf ("minishell > ");
}

process newProcess () {
	process p=malloc (sizeof(processData));
	p->argc = 0;
	p->argv=NULL;
	p->fdinPath=NULL;
	p->fdoutPath=NULL;
	return p;
}


void freeProcess (int i) {
	process p = ptab[i];
	for (int i=0; i<p->argc; i++) {
		free (p->argv[i]);
	}
	if (p->argv) 
		free (p->argv);
	if (p->fdinPath)
		free (p->fdinPath);
	if (p->fdoutPath) 
		free (p->fdoutPath);
}

void initProcTable () {
	ptab = malloc (MAX_PROC*sizeof(process));
	if (!ptab) {
		printf ("memory allocation failed.\n");
		exit(EXIT_FAILURE);
	}
	for (int i=0; i<MAX_PROC; i++) {
		ptab[i]=newProcess();
	}
}

void cleanup () {
	close(fdpipe[0]);
	close(fdpipe[1]);
}

void killchildren () {
	return;
}

void changeInput () {
	ptab[numProc]->fdinPath = strdup (yylval.str);
}

int changeOutput () {
	ptab[numProc]->fdoutPath = strdup (yylval.str);
}

void addArg () {
	process p = ptab[numProc];
	p->argc++;
	p->argv = realloc (p->argv,p->argc*sizeof(char*));
	if (!p->argv) {
		printf ("memory allocation failed.\n");
		exit(EXIT_FAILURE);
	}
	p->argv[p->argc-1]=strdup (yylval.str);
	printf ("added arg: %s\n", p->argv[p->argc-1]);
}

int run () {
	process p = ptab[numProc];
	if (p->argc == 0) {
		printf ("empty command\n");
		return SUCCESS;
	}
	if (strcmp (p->argv[0], "exit")==0) {
		printf ("exiting.");
		return QUIT;
	}
	int pid, stat;
	pid=fork ();
	if (pid!=0) { // parent
		waitpid (-1, &stat, 0);
		printf ("child exited with status %d\n", stat);
		return SUCCESS;
	}
	else { // child
		int fdin, fdout;
		if (p->fdoutPath) {
			fdout=open(p->fdoutPath,O_CREAT);
			if (fdout==-1) {
				printf ("error opening out: %s\n", p->fdoutPath);
				return FAIL;
			}
			dup2(fdout,1);
		}
		if (p->fdinPath) {
			fdin=open(p->fdinPath,O_RDONLY);
			if (fdin==-1) {
				printf ("error opening in: %s\n", p->fdinPath);
				return FAIL;
			}
			dup2(fdin,0);
		}
		printf ("executing command:");
		for (int i=0; i<p->argc; i++) {
			printf (" %s", p->argv[i]);
		}
		printf ("\n");
		execvp (p->argv[0], p->argv);
		return SUCCESS;
	}
}


void exit_shell () {
	cleanup ();
	killchildren (); // sadistic name
	printf ("thank you for using minishell!\n");
}

%}

%union {
  char *str;
  int argc;
  int exit_status;
}

%start command
%locations

%token PIPE IN OUT BACKGROUND WORD

%type<exit_status> command pipeline pipeline2 simple_command
%%

command : { return SUCCESS; } // empty command
				| pipeline { return $1; }
				| pipeline BACKGROUND
				;

pipeline : simple_command pipeline2 { if($2==FAIL) $$ = FAIL;
																			if($2==SUCCESS) $$ = $1;
																			if($2==QUIT) $$ = QUIT; } 
				 ;

pipeline2 : PIPE newlines pipeline2 { $$ = $3; }
					| /* eps */ { $$ = SUCCESS; }
					;

newlines : '\n' newlines
				 | /* eps */
				 ;

simple_command : cmd_element simple_command2 { $$ = run(); numProc++; }
							 ;

simple_command2 : cmd_element simple_command2
								| /* eps */
								;

cmd_element : WORD				{ addArg(); }
						| redirection
						;

redirection : IN WORD { changeInput (); }		
						| OUT WORD { changeOutput (); }
						;



%%

int main (int argc, char *argv[]) {
	initProcTable ();
	while (1) {
		type_prompt();
		int status=yyparse();
		switch (status) {
			case FAIL: /* continue */ break;
			case SUCCESS: /* continue */ break;
			case QUIT: exit_shell(); break;
		}
		if (status==QUIT) break;
	}
	printf ("if here, quit\n");
	return 0;
}
