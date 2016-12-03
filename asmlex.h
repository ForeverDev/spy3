#ifndef ASMLEX_H
#define ASMLEX_H

#include <stdint.h>

typedef struct AsmToken AsmToken;
typedef struct AsmTokenList AsmTokenList;

struct AsmToken {
	unsigned int line;
	enum AsmTokenType {
		ASMTOK_NOTYPE = 0,
		ASMTOK_INTEGER = 1,
		ASMTOK_FLOAT = 2,
		ASMTOK_STRING = 3,
		ASMTOK_OPERATOR = 4,
		ASMTOK_IDENTIFIER = 5
	} type;
	union {
		int64_t ival;
		double fval;
		char oval;
		char* sval; /* identifier and string */
	};
};

struct AsmTokenList {
	AsmToken* token;
	AsmTokenList* head;
	AsmTokenList* next;
};

AsmTokenList* generate_tokens(const char*);
int tok_istype(AsmToken*, enum AsmTokenType);

#endif
