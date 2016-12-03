/* lexer for the compiler, not for the assembler */

#ifndef LEX_H
#define LEX_H

#include "spy_types.h"

typedef struct Token Token;
typedef struct TokenList TokenList;
typedef enum TokenType TokenType;

/* special operator cases.. for non special code is regular ascii */
#define SPEC_NULL			0
#define SPEC_EQ				1  /* == */
#define SPEC_NEQ			2  /* != */
#define SPEC_UNARY_MINUS	3
#define SPEC_UNARY_PLUS		4
#define SPEC_INC_ONE		5  /* ++ */
#define SPEC_DEC_ONE		6  /* -- */
#define SPEC_INC_BY			7  /* += */
#define SPEC_DEC_BY			8  /* -= */
#define SPEC_MUL_BY			9  /* *= */
#define SPEC_DIV_BY			10 /* /= */
#define SPEC_MOD_BY			11 /* %= */
#define SPEC_SHL_BY			12 /* <<= */
#define SPEC_SHR_BY			13 /* >>= */
#define SPEC_AND_BY			14 /* &= */
#define SPEC_OR_BY			15 /* |= */
#define SPEC_XOR_BY			16 /* ^= */
#define SPEC_SHL			17 /* << */
#define SPEC_SHR			18 /* >> */

enum TokenType {
	TOK_NOTOK = 0,
	TOK_INTEGER = 1,
	TOK_FLOAT = 2,
	TOK_STRING = 3,
	TOK_IDENTIFIER = 4,
	TOK_OPERATOR = 5
};

struct Token {
	unsigned int line;
	TokenType type;
	union {
		spy_int ival;
		spy_float fval;
		char oval;
		char* sval; /* identifier and string */
	};
};

struct TokenList {
	Token* token;
	TokenList* next;
	TokenList* prev;
};

TokenList* generate_tokens_from_source(const char*);
void print_tokens(TokenList*);

#endif
