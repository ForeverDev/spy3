#ifndef ASMLEX_H
#define ASMLEX_H

#include <stdint.h>

typedef struct Token Token;
typedef struct TokenList TokenList;

struct Token {
	unsigned int line;
	enum TokenType {
		TOK_NOTYPE = 0,
		TOK_INTEGER = 1,
		TOK_FLOAT = 2,
		TOK_STRING = 3,
		TOK_OPERATOR = 4,
		TOK_IDENTIFIER = 5
	} type;
	union {
		int64_t ival;
		double fval;
		char oval;
		char* sval; /* identifier and string */
	};
};

struct TokenList {
	Token* token;
	TokenList* head;
	TokenList* next;
};

TokenList* generate_tokens(const char*);
void print_tokens(TokenList*);
int tok_istype(Token*, enum TokenType);

#endif
