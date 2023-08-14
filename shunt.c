#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>

const char FILEEND   = '\0';
const char CHARSEP   = '~';
const char TOKFLDSEP = ':';
const char TOKSEP    = '|';
const char UNARY     = '.';

static void error( const char* format, ... ) {
    va_list args;
    va_start(args, format);
    printf("***ERROR: ");
    vprintf(format, args);
    va_end (args);
    printf("\n");
    exit(1);
}

static int streq(const char* str1, const char* str2) {
	return 0 == strcmp( str1, str2 );
}


//================================================================
//This function writes reads stdin character by character
//and writes them one per line to stdout
//================================================================
void line2char( int fdin, int fdout ) {
    char c = 1;
    while(c != FILEEND && read( fdin, &c, 1 ) > 0) {
    	if(c == '\n') c = FILEEND;
    	write( fdout, &c, 1 );
    }
    //fprintf( stderr, "Exiting line2char\n" );
    exit(0);
}

//================================================================
//This function juxtaposes the next character to a character,
//in order to lookahead one character.
//================================================================
char* lookahead_char( int fdin, int fdout ) {
    char c = 0, c2 = 0;
    c = c2; read( fdin, &c2, 1);
    c = c2; read( fdin, &c2, 1);
    while( 1 ) {
        char buffer[3] = { c, CHARSEP, c2 };
        write( fdout, buffer, 3 );
        if(c2 == FILEEND) break;
		c = c2; read( fdin, &c2, 1);
    }
    //fprintf( stderr, "Exiting lookahead_char\n" );
    exit(0);
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
    STATE_EOF,
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
        case STATE_SPACE:         return "BLANK";     break;
        case STATE_EOF:           return "EOF";       break;
        default:                  error( "BAD STATE: %d", (int)state );
    }
}

void buffer_output( int fdout, STATE_t state, char* buffer ) {
    if( *buffer ) {
    	dprintf( fdout, "%s%c%s\n", state_name(state), TOKFLDSEP, buffer );
	    *buffer = 0;
    }
}

void lex( int fdin, int fdout ) {
    char buffer[1024];
    int charcount = 0;
    int bufferlen = 0;
    char charpair[3] = { 0 };
    char c, c2 = 1;
    STATE_t state_previous;
    STATE_t state = STATE_OPERAND;

    while(c2 && read( fdin, charpair, 3 ) == 3) {
        charcount++;
        c = charpair[0];
        c2 = charpair[2];
        //fprintf( stderr, "%02x:%02X ", c, c2 );
                
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
            
            default: state = STATE_OPERATOR;
        }
        //fprintf( stderr, "%s ", state_name(state) );
        //------------------------------------------------------------------
        //handle state change, for operand
        //------------------------------------------------------------------
        if(( state != state_previous ) && ( state_previous == STATE_OPERAND )) {
            if( state == STATE_BRACKET_LEFT ) {
            	buffer_output( fdout, STATE_FUNCTION, buffer );
            } else {
            	buffer_output( fdout, STATE_OPERAND, buffer );
        	}
            buffer_output( fdout, state, buffer );
        }
    
        //------------------------------------------------------------------
        //handle operand
        //------------------------------------------------------------------
        if( state == STATE_OPERAND ) {
            buffer[bufferlen++] = c; buffer[bufferlen] = 0;
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
                if((c == '\\') && (c2 == '\'')) {
                    buffer[bufferlen++] = c2; buffer[bufferlen] = 0;
                    read( fdin, charpair, 3 ); //skip next charpair
                    continue;
                }
                if( c == '\'') break;
                buffer[bufferlen++] = c; buffer[bufferlen] = 0;
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
                if((c == '\\') && (c2 == '"')) {
                    buffer[bufferlen++] = c2; buffer[bufferlen] = 0;
                    read( fdin, charpair, 3 ); //skip next charpair
                    continue;
                }
                if( c == '"') break;
                buffer[bufferlen++] = c; buffer[bufferlen] = 0;
            }
            buffer_output( fdout, state, buffer );
            continue;
        }
    
        //------------------------------------------------------------------
        //handle operators
        //------------------------------------------------------------------
        if( state == STATE_OPERATOR ) {
            char cp[3] = { c, c2, 0 };
            if(streq(cp, "&&") || streq(cp, "||")
            || streq(cp, "<=") || streq(cp, ">=")
            || streq(cp, "==")
            || streq(cp, "<<") || streq(cp, ">>")) {
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
            continue;
        }
    } // end while
    buffer_output( fdout, state, buffer );
    dprintf( fdout, "%s%c%s\n", state_name(STATE_EOF), TOKFLDSEP, "EOF" );
    //fprintf( stderr, "Exiting lex\n" );
    exit(0);
}

//================================================================
//This program juxtaposes the next 2 tokens to a token,
//in order to lookahead two tokens.
//================================================================
static char t1[256] = { 0 }, t2[256] = { 0 }, t3[256] = { 0 };
static char* token1 = t1;
static char* token2 = t2;
static char* token3 = t3;
static char tokend[4];
static int die = 0;

void readToken( FILE* fin ) {
	char* tswap = token1;
	token1 = token2;
	token2 = token3;
	token3 = tswap;
	
	fgets( token3, 256, fin );
	token3[strcspn(token3, "\n")] = 0;
}

void lookahead_2tokens( int fdin, int fdout ) {
	char buffer[1024];
	FILE* fin = fdopen( fdin, "r" );
	sprintf( tokend, "%c%c%c", TOKFLDSEP, TOKFLDSEP, TOKFLDSEP );
	readToken(fin);
	readToken(fin);
	readToken(fin);
	while(!streq(token3, "EOF:EOF")) {
		int len = sprintf( buffer, "%s%c%s%c%s\n", token1, TOKSEP, token2, TOKSEP, token3 );
		write( fdout, buffer, len );
		die++; if(die > 20) error("Hung");
		readToken(fin);
	} 
	fclose(fin);
	//fprintf( stderr, "Exiting lookahead_2tokens\n");
	exit(0);
}

//================================================================
//This program reads a token with 2 lookahead tokens
//if it finds a function token FUNCT, followed by a left and a right bracket
//it will replace it by tokentype FUNC0
//
//The RPN parser counts the commas inside the brackets in order
//to determine the number of arguments. The problem is that 
//A function with only one argument has exactly the same number
//of commas as a function with no arguments.
//
//Therefore, we must distinguish between zero and one arguments.
//================================================================
void fix_func0( int fdin, int fdout ) {
	FILE* fin = fdopen( fdin, "r" );
	char tokentriplet[256];
	char tokentriplet_parser[16];
	char tokenfield_parser[16];
	
	char token1[256], token2[256], token3[256];
	char type1[256], type2[256], type3[256];
	char value1[256], value2[256], value3[256];
	char buffer[1024];
	
	sprintf( tokentriplet_parser, "%%s%c%%s%c%%s\n", TOKSEP, TOKSEP );
	sprintf( tokenfield_parser, "%%s%c%%s\n", TOKFLDSEP );

	do {
	
		fgets( buffer, 1024, fin );
		sscanf( buffer, tokentriplet_parser, token1, token2, token3);
		sscanf( token1, tokenfield_parser, type1, value1);
		sscanf( token2, tokenfield_parser, type2, value2);
		sscanf( token3, tokenfield_parser, type3, value3);
		if(streq(type1, "FUNCT") && streq(type2, "BRLFT") && streq(type3, "BRGHT")) {
			
	} while(!streq(token3, "EOF:EOF"));
			
	
	while read -r tokentriplet; do
		OLD_IFS=$IFS
		IFS="${TOKSEP}"; read token1 token2 token3 <<<"$tokentriplet"
		IFS="${TOKFLDSEP}"; read type1 value1 <<<"$token1"
		IFS="${TOKFLDSEP}"; read type2 value2 <<<"$token2"
		IFS="${TOKFLDSEP}"; read type3 value3 <<<"$token3"

		if [[ "$type1" == "FUNCT" && "$type2" == "BRLFT" && "$type3" == "BRGHT" ]] ; then
			echo "FUNC0${TOKFLDSEP}$value1"
			IFS=$OLD_IFS; read -r tokentriplet;  read -r tokentriplet #skip next two tokens
		else
			echo "$type1${TOKFLDSEP}$value1"
		fi
		IFS=$OLD_IFS
	done
}

//================================================================
//This program combines the entire chain required to translate
//an infix expression into an RPN postfix expression.
//================================g================================
int main(int argc, char *argv[]) {
    int fd[9][2];
    
    // open the pipes
    for(int i=0; i<9; i++) pipe(fd[i]);
    
    // fork the processes for each pipe
    if(fork() == 0) line2char         ( STDIN_FILENO, fd[0][1] ); //fd[0][0] 	   );
    if(fork() == 0) lookahead_char    ( fd[0][0],     fd[1][1] );
    if(fork() == 0) lex               ( fd[1][0], 	  fd[2][1] );
    if(fork() == 0) lookahead_2tokens ( fd[2][0],     fd[3][1] );
    if(fork() == 0) fix_func0         ( fd[3][0],     STDERR_FILENO );
    // if(fork() == 0) lookahead_2tokens ( fd[4][1], fd[5][0] );
    // if(fork() == 0) fix_funcdef       ( fd[5][1], fd[6][0] );
    // if(fork() == 0) rpn               ( fd[6][1], fd[7][0] );
    // if(fork() == 0) lookahead_2tokens ( fd[7][1], fd[8][0] );
    // else            fix_invoke        ( fd[8][1], stdout   );
        
    while(wait(NULL) > 0);
    
    // close the pipes
    for(int i=0; i<9; i++) { close(fd[i][0]); close(fd[i][1]); }
}
