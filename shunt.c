#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

const char EOF       = '\0';
const char CHARSEP   = '·';
const char TOKFLDSEP = '·';
const char TOKSEP    = '¦';
const char UNARY     = '.';

void error( const char* format, ... ) {
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
void line2char( int fdin, int fdout ) {
    char c;
    while(read( fdin, &c, 1 ) > 0) write( fdout, &c, 1 );
}

//================================================================
//This function juxtaposes the next character to a character,
//in order to lookahead one character.
//================================================================
void readChar( int fdin, char& c, char& c2 ) {
    c = c2;
    int len = read( fdin, &c, 1 );
    if( len == 0 ) c2 = EOF;
    if( c2  == 0 ) c2 = ' ';
}
char* lookahead_char( int fdin, int fdout ) {
    char c = 0, c2 = 0;
    readChar(c, c2);
    readChar(c, c2);
    while( c != EOF ) {
        char buffer[3] = { c, CHARSEP, c2 };
        write( fdout, &buffer, 3 );
        readChar(c, c2);
    }
}

//================================================================
//This program is a simplistic lexer, but it can handle 
//single quoted and double quoted strings, with embedded quotes
//escaped by a backslash character
//
//Punctuation characters are considered operators. 
//It distinguishes the following named operators: '(' ')' '->' ',' 
//These operators are treated speci}cally in the RPN parser
//Everything else are operands.
//However, an operand followed by a '(' is considered a function
//================================================================
typedef enum {
    STATE_NONE,
    STATE_OPERAND,
    STATE_FUNCTION,
    STATE_BRACKET_LEFT,
    STATE_BRACKET_RIGHT,
    STATE_COMMA,
    STATE_DEREF,
    STATE_QUOTE_SINGLE,
    STATE_QUOTE_DOUBLE,
    STATE_OPERATOR,
    STATE_SPACE,
    STATE_SEMICOLON,
    STATE_BLOCK_START,
    STATE_BLOCK_END,
} STATE_t;

const char* state_name( STATE_t state ) {
	switch(state) {
		case STATE_OPERAND:       return "OPRND";     break;
		case STATE_FUNCTION:      return "FUNCT";     break;
		case STATE_BRACKET_LEFT:  return "BRLFT";     break;
		case STATE_BRACKET_RIGHT: return "BRGHT";     break;
		case STATE_COMMA:         return "COMMA";     break;
		case STATE_DEREF:         return "DEREF";     break;
		case STATE_SEMICOLON:     return "SEMICOLON"; break;
		case STATE_QUOTE_SINGLE:  return "STRNG";     break;
		case STATE_QUOTE_DOUBLE:  return "STRNG";     break;
		case STATE_BLOCK_START:   return "BLKST";     break;
		case STATE_BLOCK_END:     return "BLKEND";    break;
		case STATE_OPERATOR:      return "OPER";      break;
        default:                  error( "BAD STATE" );
    }
}

void buffer_output( STATE_t state, int fdout, char*& buffer ) {
	if( *buffer ) { dprintf( fdout, "%s%c%s", state_name(state), TOKFLDSEP, buffer)
	*buffer = 0;		
}

void lex( int fdin, int fdout ) {
    char buffer[1024];
    int charcount = 0;
    int bufferlen = 0;
    char charpair[3] = { 0 };
    char c, c2;
    STATE_t state_previous;
    STATE_t state = STATE_OPERAND;

    while(read( fdin, charpair, 3 ) == 3) {
        charcount++;
        c = charpair[0];
        c2 = charpair[2];
        
        state_previous = state;
        if(buffer[0] == 0) bufferlen = 0;

        switch(c) {
            case 'a' ... 'z' : state = STATE_OPERAND; break;
            case 'A' ... 'Z' : state = STATE_OPERAND; break;
            case '0' ... '9' : state = STATE_OPERAND; break;
            case '_': state = STATE_OPERAND; break;
            case '.': state = STATE_OPERAND; break;
            case '$': state = STATE_OPERAND; break;
            case ' ': state = STATE_SPACE; break;
            case '(': state = STATE_BRACKET_LEFT; break;
            case ')': state = STATE_BRACKET_RIGHT; break;
            case ',': state = STATE_COMMA; break;
            case ';': state = STATE_SEMICOLON; break;
            case '{': state = STATE_BLOCK_START; break;
            case '}': state = STATE_BLOCK_END; break;
            case '"': state = STATE_QUOTE_DOUBLE; break;
            case '\'':state = STATE_QUOTE_SINGLE; break;
            default: state = STATE_OPERAND;
        }
        
    	//------------------------------------------------------------------
    	//handle state change, for operand
    	//------------------------------------------------------------------
    	if(( state != state_previous ) && ( state_previous == STATE_OPERAND )) {
            if( state == STATE_BRACKET_LEFT ) buffer_output( fdout, STATE_FUNCTION, buffer );
            else buffer_output( fdout, STATE_OPERAND, buffer );
            buffer_output( fdout, state, buffer );
        }

    
    	//------------------------------------------------------------------
    	//handle operand
    	//------------------------------------------------------------------
    	if( state == STATE_OPERAND ) {
    		buffer[bufferlen++] = &c;
    		continue;
    	}
    
    	//------------------------------------------------------------------
    	//handle unary plus/minus
    	//------------------------------------------------------------------
    	if(( charcount == 1 ) && ( c == '+' || c == '-')) {
    		buffer[0] = UNARY; buffer[1] = c; buffer[2] = 0;
    		buffer_output( fdout, state, buffer );
    		continue;
    	}	
    
    	if(( state == STATE_BRACKET_LEFT && ( c2 == '+' || c2 == '-'))) {
    		buffer[0] = c; buffer[1] = 0;
    		buffer_output( fdout, STATE_BRACKET_LEFT, buffer );
    		buffer[0] = UNARY; buffer[1] = c2; buffer[2] = 0;
    		buffer_output( fdout, STATE_OPERATOR, buffer );
    		read( fdin, charpair, 3 ) ; // drop next char
    		continue;
        }

    	//------------------------------------------------------------------
    	//handle named punctuation
    	//------------------------------------------------------------------
        if((state == STATE_BRACKET_LEFT) || (state == STATE_BRACKET_RIGHT)
        || (state == STATE_COMMA)        || (state == STATE_SEMICOLON)
        || (state == STATE_BLOCK_START)  || (state == STATE_BLOCK_END)) {
            buffer[0] = c; buffer[1] = 0;
            buffer_output( fdout, state, buffer );
            continue;
        }
    
    	//------------------------------------------------------------------
    	//handle quote single
    	//------------------------------------------------------------------
    	if( state == STATE_QUOTE_SINGLE ) {
            while(read( fdin, charpair, 3 ) == 3) {
                c = charpair[0];
                c2 = charpair[2];
    			//handle escaped single quote
                if((c == '\') && (c2 == '\'')) {
                    buffer[bufferlen++] = c2;
                    read( fdin, charpair, 3 ); //skip next charpair
                    continue;
                }
    			if( c == '\'') break;
                buffer[bufferlen++] = c;
            }
            buffer_output( fdout, state, buffer );
            continue;
        }
    
    	//------------------------------------------------------------------
    	//handle quote double
    	//------------------------------------------------------------------
    	if( state == STATE_QUOTE_DOUBLE ) {
            while(read( fdin, charpair, 3 ) == 3) {
                c = charpair[0];
                c2 = charpair[2];
    			//handle escaped double quote
                if((c == '\') && (c2 == '"')) {
                    buffer[bufferlen++] = c2;
                    read( fdin, charpair, 3 ); //skip next charpair
                    continue;
                }
    			if( c == '"') break;
                buffer[bufferlen++] = c;
            }
            buffer_output( fdout, state, buffer );
            continue;
        }
    
    	//------------------------------------------------------------------
    	//handle operators
    	//------------------------------------------------------------------
    	if( state == STATE_OPERATOR ) {
    		char cp[3] = { c, c2, 0 };
            if(streq(cp, "&&") || streq(cp, "||") || streq(cp, "<=") || streq(cp, ">=") || streq(cp, "==") || streq(cp, "<<") || streq(cp, ">>")) {
                buffer[0] = c; buffer[1] = c2; buffer[2] = 0;
        		read( fdin, charpair, 3 ) ; // drop next char
            }
            else if(streq(cp, "->")) {
                buffer[0] = c; buffer[1] = c2; buffer[2] = 0;
                state = STATE_DEREF;
        		read( fdin, charpair, 3 ) ; // drop next char
            }
            else {
                buffer[0] = c; buffer[1] = 0;
            }
    		buffer_output( fdout, state, buffer );
    	}
        
}

//================================================================
//This program combines the entire chain required to translate
//an infix expression into an RPN postfix expression.
//================================================================
int main(int argc, char *argv[]) {
    int fd[9][2];
    
    // open the pipes
    for(int i=0; i<9; i++) pipe(fd[i]);
    
    // fork the processes for each pipe
    if(fork() == 0) line2char         ( stdin,    fd[0][0] );
    if(fork() == 0) lookahead_char    ( fd[0][1], fd[1][0] );
    else            lex               ( fd[1][1], stdout   );
    // if(fork() == 0) lex               ( fd[1][1], fd[2][0] );
    // if(fork() == 0) lookahead_2tokens ( fd[2][1], fd[3][0] );
    // if(fork() == 0) fix_func0         ( fd[3][1], fd[4][0] );
    // if(fork() == 0) lookahead_2tokens ( fd[4][1], fd[5][0] );
    // if(fork() == 0) fix_funcdef       ( fd[5][1], fd[6][0] );
    // if(fork() == 0) rpn               ( fd[6][1], fd[7][0] );
    // if(fork() == 0) lookahead_2tokens ( fd[7][1], fd[8][0] );
    // else            fix_invoke        ( fd[8][1], stdout   );
    
    // close the pipes
    for(int i=0; i<9; i++) { close(fd[i][0]); close(fd[i][1]); }
}
