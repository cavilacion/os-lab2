#include "shell.h"

extern char *word_buff;

/* "safe" memory allocation and reallocation */
void *safeRealloc (void *ptr, int new_size) {
	ptr = realloc (ptr, new_size);
	if (!ptr) {
		printf ("fatal error: memory reallocation failed\n");
		return NULL;
	}
	return ptr;
}
void *safeMalloc (int size) {
	void *ptr = malloc (size);
	if (!ptr) {
		printf ("fatal error: memory allocation failed\n");
		return NULL;
	}
	return ptr;
}

/* strdup does not appear to be ISO C */
char *strdup (const char *str) {
	char *dup=safeMalloc (strlen(str)+1);
	int i;
	if (dup) for (i=0; i<strlen(str); ++i) {
		dup[i]=str[i];
	}
	if (dup) dup[i]='\0';
	return dup;
}

int initPipeline (pipeline *p) {
	p->argc=0;
	p->maxArg=64;
	p->argv=safeMalloc (p->maxArg*sizeof(char*));
	if (p->argv==NULL) return -1;
	p->fdinPath=NULL;
	p->fdoutPath=NULL;
	return 0;
}

int resizePipeline (pipeline *p) {
	p->maxArg *= 2;
	p->argv=safeRealloc (p->argv, p->maxArg*sizeof(char*));
	if (p->argv==NULL) return -1;
	return 0;
}
	
void destroyPipeline (pipeline *p) {
	for (int i=0; i<p->argc; i++) {
		free (p->argv[i]);
	}
	free (p->argv);
	if (p->fdinPath!=NULL) free (p->fdinPath);
	if (p->fdoutPath!=NULL) free (p->fdoutPath);
}

int initCommand (command *c) {
	c->maxPipes = 64;
	c->pl = safeMalloc (c->maxPipes*sizeof(pipeline));
	if (c->pl==NULL) return -1;
	c->fd = safeMalloc (2*c->maxPipes*sizeof(int));
	if (c->fd==NULL) return -1;
	c->numPipes = 0;
	c->bg = 0;
	int r = initPipeline (&(c->pl[0]));
	return r;
}

int resizeCommand (command *c) {
	c->maxPipes *= 2;
	c->pl = safeRealloc (c->pl, c->maxPipes*sizeof(pipeline));
	if (c->pl==NULL) return -1;
	c->fd = safeRealloc (c->fd, 2*c->maxPipes*sizeof(int));
	return 0;
}

void destroyCommand (command *c) {
	for (int i=0; i<=c->numPipes; i++) {
		destroyPipeline (&(c->pl[i]));
	}
	free (c->fd);
	free (c->pl);
}

int initProcTable (procTable *pt) {
	pt->maxProcs = 64;
	pt->pid = safeMalloc (64*sizeof(int));
	if (pt->pid == NULL) return -1;
	pt->numProcs = 0;
	return 0;
}

int resizeProcTable (procTable *pt) {
	pt->maxProcs *= 2;
	pt->pid = safeRealloc (pt->pid, pt->maxProcs*sizeof(int));
	if (pt->pid == NULL) return -1;
	return 0;
}
void destroyProcTable (procTable *pt) {
	free (pt->pid);
}

void type_prompt () {
	printf ("minishell$ ");
}

/* this function kills children (omg) and cleans up command and proctable
 */
void cleanup (command *cmd, procTable *pt) {
	if (pt!=NULL) {
		int n = pt->numProcs;
		for (int i=0; i<n; i++) {
			kill (pt->pid[i], SIGTERM);
		}
		destroyProcTable (pt);
	}
	if (cmd!=NULL) {
		destroyCommand (cmd);
	}
}

/* function copies word buffer to pipeline as fdinPath */
int changeInput (pipeline *p) {
	if (p->fdinPath!=NULL) free (p->fdinPath);
	p->fdinPath = strdup (word_buff);
	if (!p->fdinPath) return -1;
	free (word_buff);
	return 0;
}

/* function copies word buffer to pipeline as fdoutPath */
int changeOutput (pipeline *p) {
	if (p->fdoutPath !=NULL) free (p->fdoutPath);
	p->fdoutPath = strdup (word_buff);
	if (!p->fdoutPath) return -1;
	free (word_buff);
	return 0;
}

/* function adds word buffer to pipeline as an argument */
int addArg (pipeline *p) {
	p->argc++;
	if (p->argc == p->maxArg) {
		p->maxArg *= 2;
		p->argv = safeRealloc (p->argv,p->argc*sizeof(char*));
		if (!p->argv) return -1;
	}
	
	p->argv[p->argc-1]=strdup (word_buff);
	if (!p->argv[p->argc-1]) return -1;
	p->argv[p->argc]='\0';
	
	free (word_buff);
	return 0;
}

int executeCmd (command *cmd, procTable *pt) {
	int stat, pid;
	int n = cmd->numPipes;
	int bg = cmd->bg;
	int *fd = cmd->fd;
	/* initialize pipes */
	for (int i=0; i < n; i++) {
		if (pipe (&fd[2*i]) == -1) {
			printf ("pipe error\n");
			return -1;
		}
	}
	
	for (int i=0; i<=n; i++) {
		
		pid=fork();
		
		/* parent */
		if (pid!=0) { 
			if (!bg) {
				waitpid (pid, &stat, 0);
				if (n > 0 && i < n) {
					close (fd[2*i+1]);
				}
				if (i > 0) {
					close (fd[2*i-2]);
				}
			}
			else { // save process in table
				pt->pid[pt->numProcs++] = pid;
				if (pt->numProcs >= pt->maxProcs) {
					if (resizeProcTable (pt) == -1) {
						return -1;
					}
				}
			}
		}
		
		/* child */
		else {
			pipeline p = cmd->pl[i];
			if (i > 0) { // input from pipe
				close (0);
				dup (fd[2*i-2]);
				close (fd[2*i-2]);
			}
			else if (p.fdinPath != NULL) { // input from file
				int fdin = open (p.fdinPath, O_RDONLY);
				if (fdin == -1) {
					printf ("unable to find %s for input\n", p.fdinPath);
					return -1;
				}
				close (0);
				dup(fdin);
				close (fdin);
			}
			if (n > 0 && i < n) { // output to pipe
				close (1);
				dup (fd[2*i+1]);
				close (fd[2*i+1]);
			}
			else if (p.fdoutPath != NULL) { // output to file
				int fdout = open (p.fdoutPath, O_RDWR);
				if (fdout == -1) {
					printf ("unable to find %s for output\n", p.fdoutPath);
					return -1;
				}
				close (1);
				dup (fdout);
				close (fdout);
			}
			stat = execvp (p.argv[0], p.argv);
			if (stat == -1) {
				printf ("unable to execute command %s\n", p.argv[0]);
			}
			return -1;
		}
		
	}
	
	/* parent closes pipes */
	for (int i=0; i < 2*n; i++) {
		close (fd[2*i]);
	}
	
	return 0;
}
