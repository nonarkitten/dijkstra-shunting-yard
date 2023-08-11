#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

const char* EOF='\0';
const char* CHARSEP='·';
const char* TOKFLDSEP='·';
const char* TOKSEP='¦';
const char* UNARY='.';

void error(const char* format, ...) {
	va_list args;
	va_start(args, format);
  	printf("***ERROR: ");
	vprintf(format, args);
	va_end (args);
	printf("\n");
	exit(1);
}

//================================================================
//This function writes reads stdin character by character
//and writes them one per line to stdout
//================================================================
void line2char(int fdin, int fdout) {
	char c;
	while(read( fdin, &c, 1 ) > 0) write( fdout, &c, 1 );
}

//================================================================
//This program juxtaposes the next character to a character,
//in order to lookahead one character.
//================================================================
char* lookahead_char(int fdin, int fdout) {
	char c, c2;
	c = c2; if(!read( fdin, &c2, 1 )) c2 = EOF;
	c = c2; if(!read( fdin, &c2, 1 )) c2 = EOF;
	while( c != EOF ) {
		char buffer[3] = { c, CHARSEP, c2 };
		write( fdout, &buffer, 3 );
		c = c2; if(!read( fdin, &c2, 1 )) c2 = EOF;
	}
}

int main(int argc, char *argv[]) {
	int fd[9][2];
	
	// open the pipes
	for(int i=0; i<9; i++) pipe(fd[i]);
	
	// fork the processes for each pipe
	if(fork() == 0) line2char		  ( stdin,    fd[0][0]);
	if(fork() == 0) lookahead_char	  ( fd[0][1], fd[1][0]);
	if(fork() == 0) lex				  ( fd[1][1], fd[2][0]);
	if(fork() == 0) lookahead_2tokens ( fd[2][1], fd[3][0]);
	if(fork() == 0) fix_func0		  ( fd[3][1], fd[4][0]);
	if(fork() == 0) lookahead_2tokens ( fd[4][1], fd[5][0]);
	if(fork() == 0) fix_funcdef		  ( fd[5][1], fd[6][0]);
	if(fork() == 0) rpn				  ( fd[6][1], fd[7][0]);
	if(fork() == 0) lookahead_2tokens ( fd[7][1], fd[8][0]);
	else            fix_invoke		  ( fd[8][1], stdout  );
	
	// close the pipes
	for(int i=0; i<9; i++) { close(fd[i][0]); close(fd[i][1]); }
}
