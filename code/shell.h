#ifndef _SHELL__H
#define _SHELL__H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

/* flex will produce a stream of Tokens */
typedef enum Token { WORD, NEWLINE, IN, OUT, PIPE, BACKGROUND, INVALID, PARSE_ERROR, MEMORY_ERROR } Token;

/* a pipeline contains words as argv and possibly paths to in and out */ 
typedef struct pipeline {
	char **argv;
	int argc;
	int maxArg;
	char *fdinPath;
	char *fdoutPath;
} pipeline;

/* a command contains one or several pipelines */
typedef struct command {
	pipeline *pl;
	int *fd;
	int numPipes;
	int maxPipes;
	int bg;
} command;

/* a process table contains all background processes with their pid */
typedef struct procTable {
	int *pid;
	int numProcs;
	int maxProcs;
} procTable;

void *safeRealloc (void *ptr, int new_size);
void *safeMalloc (int size);
char *strdup (const char *str);

int initPipeline (pipeline *p);
int resizePipeline (pipeline *p);
void destroyPipeline (pipeline *p);

int initCommand (command *c);
int resizeCommand (command *c);
void destroyCommand (command *c);

int initProcTable (procTable *pt);
int resizeProcTable (procTable *pt);
void destroyProcTable (procTable *pt);

void type_prompt ();
void cleanup (command *cmd, procTable *pt);
int changeInput (pipeline *p);
int changeOutput (pipeline *p);
int addArg (pipeline *p);

int executeCmd (command *cmd, procTable *pt);

/* parse functions */

int isError (Token tok);
int parseInput (command *cmd, procTable *pt);
Token parsePipeline (command *cmd, procTable *pt);
Token parsePipeline2 (command *cmd, procTable *pt, Token tok);
Token parseCommand (command *cmd, procTable *pt);
Token parseCmdElement (command *cmd, procTable *pt, Token tok);
Token parseCommand2 (command *cmd, procTable *pt, Token tok);

#endif

