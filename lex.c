#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "lex.h"

typedef struct LexState LexState;
typedef struct TokenListing TokenListing;

struct LexState {
	TokenList* tokens;
	char* contents;
	unsigned int current_line;
};

struct TokenListing {
	const char* word;
	char code;
};

/* NOTE this only lists special cases... regular cases
 * get their ascii code assigned */
static const TokenListing token_listing[] = {
	{"==", SPEC_EQ},
	{"!=", SPEC_NEQ},
	{"++", SPEC_INC_ONE},
	{"--", SPEC_DEC_ONE},
	{"+=", SPEC_INC_BY},
	{"-=", SPEC_DEC_BY},
	{"*=", SPEC_MUL_BY},
	{"/=", SPEC_DIV_BY},
	{"%=", SPEC_MOD_BY},
	{"<<=", SPEC_SHL_BY},
	{">>=", SPEC_SHR_BY},
	{"&=", SPEC_AND_BY},
	{"|=", SPEC_OR_BY},
	{"^=", SPEC_XOR_BY},
	{"<<", SPEC_SHL},
	{">>", SPEC_SHR},
	{"&&", SPEC_LOG_AND},
	{"||", SPEC_LOG_OR},
	{">=", SPEC_GE},
	{"<=", SPEC_LE},
	{"->", SPEC_ARROW},
	{"typename", SPEC_TYPENAME},
	{"sizeof", SPEC_SIZEOF},
	{NULL, 0}
};

static void 
lex_die(LexState* L, const char* message, ...) {
	va_list args;
	va_start(args, message);
	printf("\n\n** SPYRE LEX ERROR **\n\tmessage: ");
	vprintf(message, args);
	printf("\n\tline: %d\n\n\n", L->current_line);
	va_end(args);
	exit(1);
}

static void
append_token(LexState* L, Token* token) {
	token->line = L->current_line;
	if (!L->tokens->token) {
		L->tokens->token = token;
	} else {
		TokenList* scan = L->tokens;
		TokenList* new_list = malloc(sizeof(TokenList));
		new_list->token = token;
		new_list->next = NULL;
		while (scan->next) {
			scan = scan->next;
		}
		scan->next = new_list;
		new_list->prev = scan;
	}
}

/* after a number is lexed, this function is called to determine
 * whether or not the previous token (unary + or -) should be
 * applied to the number and discarded from the token list */
static void
check_make_unary(LexState* L) {

}

static int
on_float(LexState* L) {
	if (!isdigit(*L->contents)) {
		return 0;
	}
	char* scan = L->contents;
	while (isdigit(*scan)) {
		scan++;
	}
	if (*scan != '.') {
		return 0;
	};
	scan++;
	if (!isdigit(*scan)) {
		lex_die(L, "expected digits after '.'");
	}
	return 1;
}

static int
on_int(LexState* L) {
	/* NOTE there is no need to make sure this isn't a float because
	 * on_float is called before on_int is called */
	return isdigit(*L->contents);
}

static int
on_identifier(LexState* L) {
	return isalpha(*L->contents) || *L->contents == '_';
}

static int
on_operator(LexState* L) {
	return ispunct(*L->contents) && *L->contents != '_';
}

static void
lex_float(LexState* L) {
	Token* tok;
	char* buf;
	char* start;
	size_t len = 0;
	start = L->contents;
	while (isdigit(*L->contents)) {
		L->contents++;
		len++;
	}
	L->contents++; /* skip '.' */
	len++;
	while (isdigit(*L->contents)) {
		L->contents++;
		len++;
	}	
	buf = malloc(len + 1);
	memcpy(buf, start, len);
	buf[len] = 0;
	tok = malloc(sizeof(Token));
	tok->type = TOK_FLOAT;
	tok->fval = (spy_float)strtod(buf, NULL);
	append_token(L, tok);
	free(buf);
}

static void
lex_int(LexState* L) {
	Token* tok;
	char* buf;
	char* start;
	size_t len = 0;
	int base = (*L->contents == '0' && L->contents[1] == 'x') ? 16 : 10;
	if (base == 16) {
		L->contents += 2;
	}
	start = L->contents;
	while (isdigit(*L->contents)) {
		len++;
		L->contents++;
	}
	buf = malloc(len + 1);
	memcpy(buf, start, len);
	buf[len] = 0;
	tok = malloc(sizeof(Token));
	tok->type = TOK_INTEGER;
	tok->ival = (spy_int)strtoll(buf, NULL, base);
	free(buf);
	append_token(L, tok);
}

static void
lex_identifier(LexState* L) {
	Token* tok;
	char* buf;
	char* start;
	size_t len = 0;
	start = L->contents;
	while (isalnum(*L->contents) || *L->contents == '_') {
		len++;
		L->contents++;
	}
	buf = malloc(len + 1);
	memcpy(buf, start, len);
	buf[len] = 0;
	tok = malloc(sizeof(Token));
	tok->type = TOK_IDENTIFIER;
	tok->sval = buf;
	append_token(L, tok);
}

static void
lex_operator(LexState* L) {
	Token* tok;
	char ctype;
	char* start = L->contents;
	const TokenListing* found = NULL;
	for (const TokenListing* i = token_listing; i->word; i++) {
		if (!strncmp(L->contents, i->word, strlen(i->word))) {
			found = i;
			L->contents += strlen(i->word);
			break;
		}
	}
	tok = malloc(sizeof(Token));
	tok->type = TOK_OPERATOR;
	if (found) {
		tok->oval = found->code;
	} else {
		tok->oval = *L->contents++;
	}
	append_token(L, tok);
}

char*
token_tostring(Token* t) {
	if (!t) goto unknown;
	switch (t->type) {
		case TOK_OPERATOR:
			return tokcode_tostring(t->oval);
		case TOK_INTEGER: {
			char* buf = malloc(64);
			sprintf(buf, "%lld", t->ival);
			return buf;
		}
		case TOK_FLOAT: {
			char* buf = malloc(64);
			sprintf(buf, "%f", t->fval);
			return buf;
		}
		case TOK_IDENTIFIER: {
			char* buf = malloc(strlen(t->sval) + 1);
			strcpy(buf, t->sval);
			return buf;
		}
	}
	char* buf;
	unknown:
	buf = malloc(2);
	buf[0] = '?';
	buf[1] = 0;
	return buf;
}

char*
tokcode_tostring(char code) {
	for (const TokenListing* i = token_listing; i->word; i++) {
		if (i->code == code) {
			char* ret = malloc(strlen(i->word) + 1);
			strcpy(ret, i->word);
			return ret;
		}
	}
	char* ret = malloc(2);
	ret[0] = code;
	ret[1] = 0;
	return ret;
}

void
print_token(Token* t) {
	switch (t->type) {
		case TOK_INTEGER:
			printf("(%lld)\n", t->ival);
			break;
		case TOK_FLOAT:
			printf("(%f)\n", t->fval);
			break;
		case TOK_IDENTIFIER:
			printf("(%s)\n", t->sval);
			break;
		case TOK_STRING:
			printf("(\"%s\")\n", t->sval);
			break;
		case TOK_OPERATOR:
			printf("(OP %d)\n", t->oval);
			break;
	}
}

void
print_tokens(TokenList* list) {
	for (TokenList* i = list; i; i = i->next) {
		if (!i->token) break;
		print_token(i->token);
	}
}

TokenList*
generate_tokens_from_source(const char* filename) {
	
	LexState L;
	L.current_line = 1;
	L.contents = NULL;
	L.tokens = malloc(sizeof(TokenList));
	L.tokens->token = NULL;
	L.tokens->next = NULL;
	L.tokens->prev = NULL;
		
	FILE* handle;
	unsigned long long flen;
	char* start; /* used to free later */
	
	handle = fopen(filename, "rb");
	if (!handle) {
		lex_die(&L, "couldn't open '%s' for reading", filename);
	}		
	fseek(handle, 0, SEEK_END);
	flen = ftell(handle);
	rewind(handle);
	start = L.contents = malloc(flen + 1);
	fread(L.contents, 1, flen, handle);
	L.contents[flen] = 0;
	fclose(handle);

	while (*L.contents) {
		if (*L.contents == '\n') {
			L.current_line++;
			L.contents++;
			continue;
		} else if (*L.contents == '/' && L.contents[1] == '*') {
			L.contents += 2;
			while (*L.contents != '*' && L.contents[1] != '/') {
				if (*L.contents == '\n') L.current_line++;
				L.contents++;
			}
			L.contents += 2;
			continue;
		} else if (*L.contents == ' ' || *L.contents == '\t' || *L.contents == 13) {
			L.contents++;
			continue;
		}
		if (on_float(&L)) {
			lex_float(&L);
		} else if (on_int(&L)) {
			lex_int(&L);
		} else if (on_identifier(&L)) {
			lex_identifier(&L);
		} else if (on_operator(&L)) {
			lex_operator(&L);
		} else {
			lex_die(&L, "unknown token '%c'", *L.contents);
		}
	}

	free(start);
	return L.tokens;

}
