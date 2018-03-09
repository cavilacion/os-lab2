%{
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "parser.tab.h"
#include "lex.yy.c"

#define MAX_ARG 256
#define TRUE 1

char **argv;
int argc = 0;

int yyerror() {
  printf("PARSE ERROR (%d)\n",yylineno);
  exit(EXIT_SUCCESS);
}

void type_prompt () {
	printf ("mini-shell $ ");
}

void newCommand () {
	for (int i=0; i<argc; i++) {
		free (argv[i]);
	}
	argv = malloc (MAX_ARG*sizeof(char*));
	argc=0;
}

void addArg () {
	argv[argc++]=strdup (yylval.str);
}

void run () {
	int pid, stat;
	pid=fork ();
	if (pid!=0) { // parent
		waitpid (-1, &stat, 0);
	}
	else { // child
		execvp (argv[0], argv);
	}
}

%}

/* Bison declarations.  */
%union {
  char *str;
}

%token PIPE IN OUT BACKGROUND
%token WORD

%token INVALID

%start command
%locations

%%

command : pipeline
				| pipeline BACKGROUND
				;

pipeline : simple_command { run (); } pipeline2
				 ;

pipeline2 : PIPE newlines pipeline
					| /* eps */
					;

newlines : '\n' newlines
				 | /* eps */
				 ;

simple_command : { newCommand (); }
								 cmd_element simple_command2
							 ;

simple_command2 : cmd_element simple_command2
								| /* eps */
								;

cmd_element : WORD				{ addArg(); }
						| redirection
						;

redirection : IN WORD		
						| OUT WORD		
						;



%%

int main (int argc, char *argv[]) {
	while (TRUE) {
		type_prompt();
		yyparse();
	}
	return 0;
}
