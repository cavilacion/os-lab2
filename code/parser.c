#include "shell.h"

int isError (Token tok) {
	return (tok==MEMORY_ERROR || tok==PARSE_ERROR);
}

int parseInput (command *cmd, procTable *pt) {
	Token tok = parsePipeline (cmd, pt);
	switch (tok) {
		case NEWLINE:					 // end of command
			return 1;
			break;
		case BACKGROUND: 
			cmd->bg=1;
			tok = yylex();
			if (tok == NEWLINE)  // end of command
				return 1;
			else 								 // parse error
				return 0;					
			break;
		case MEMORY_ERROR: 
			return -1;
			break;
		case PARSE_ERROR: default:	// IN, OUT, WORD, PIPE are all parse errors
			return 0;
			break;			
	}
}

Token parsePipeline (command *cmd, procTable *pt) {
	Token tok = parseCommand (cmd, pt);
	if (isError (tok)) return tok;
	return parsePipeline2 (cmd, pt, tok);
}

Token parsePipeline2 (command *cmd, procTable *pt, Token tok) {
	int status;
	if (tok == PIPE) {
		cmd->numPipes++;
		if (cmd->numPipes >= cmd->maxPipes) {
			status = resizeCommand (cmd);
			if (status == -1) return MEMORY_ERROR;
		}
		status = initPipeline (&(cmd->pl[cmd->numPipes]));
		if (status == -1) return MEMORY_ERROR;
		return parsePipeline (cmd, pt);
	}
	return tok;
}

Token parseCommand (command *cmd, procTable *pt) {
	Token tok = yylex();
	if (tok == NEWLINE) return NEWLINE;
	Token next = parseCmdElement (cmd, pt, tok);
	if (isError (next)) return next;
	return parseCommand2 (cmd, pt, next);
}

Token parseCmdElement (command *cmd, procTable *pt, Token tok) {
	Token next;
	int status, n;
	switch (tok) {
		case WORD:
			n = cmd->numPipes;
			status = addArg (&(cmd->pl[n]));
			next = yylex();
			break;
		case IN:
			next = yylex();
			if (next!=WORD) return 0; // parse error
			n = cmd->numPipes;
			status = changeInput (&(cmd->pl[n]));
			next = yylex();
			break;
		case OUT:
			next = yylex();
			if (next!=WORD) return 0; // parse error
			n = cmd->numPipes;
			status = changeOutput (&(cmd->pl[n]));
			next = yylex();
			break;
		default:
			next = PARSE_ERROR;
			break;
	}
	if (status==-1) return MEMORY_ERROR;
	return next;
}

Token parseCommand2 (command *cmd, procTable *pt, Token tok) {
	Token next;
	if (tok == WORD || tok == IN || tok == OUT) {
		next = parseCmdElement (cmd, pt, tok);
		return parseCommand2 (cmd, pt, next);
	}
	return tok; 
}	
	
	
	
		
			
				
