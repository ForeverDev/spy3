#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "asmlex.h"

typedef struct Lexer Lexer;

struct Lexer {
	unsigned int current_line;
	FILE* handle;
	char* contents;
	AsmTokenList* tokens;
};

static void
lex_die(Lexer* L, const char* msg, ...) {
	va_list args;
	va_start(args, msg);
	printf("\n\n*** SPYRE ASM-LEX ERROR ***\n\tmessage: ");
	vprintf(msg, args);
	printf("\n\tline: %d\n\n\n", L->current_line);
	va_end(args); /* is this really necessary? */
	exit(1);
}

static void
append_token(Lexer* L, AsmToken* token) {
	/* set line accordingly */
	token->line = L->current_line;
	/* if it's the first token, L->tokens->token is null */
	if (!L->tokens->token) {
		L->tokens->token = token;
	} else {
		AsmTokenList* new = malloc(sizeof(AsmTokenList));
		new->token = token;
		new->head = L->tokens;
		new->next = NULL;
		AsmTokenList* scanner = L->tokens;
		while (scanner->next) {
			scanner = scanner->next;
		}
		scanner->next = new;
	}
}

static int
matches_integer(Lexer* L) {
	char* check = L->contents;
	if (*check == '-') {
		check++;
	}
	return isdigit(*check) || (*check == '0' && check[1] == 'x');
}

static int
matches_float(Lexer* L) {
	char* check = L->contents;
	if (*check == '-') {
		check++;
	}
	if (!isdigit(*check)) {
		return 0;
	}
	char* scan = &check[1];
	while (isdigit(*scan)) {
		scan++;
	}
	/* now are we on a period? */
	return *scan == '.';
}

static int
matches_identifier(Lexer* L) {
	return isalpha(*L->contents) || *L->contents == '_' || (*L->contents == '.' && isalnum(L->contents[1]));
}

static int
matches_string(Lexer* L) {
	return *L->contents == '"';
}

static int
matches_operator(Lexer* L) {
	return ispunct(*L->contents) && *L->contents != '_';
}

static int
matches_character(Lexer* L) {
	return *L->contents == '\'';
}

static void
lex_float(Lexer* L) {
	char* buf;
	size_t dist;
	AsmToken* token;
	char* start;
	int sign = 1;
	if (*L->contents == '-') {
		sign = -1;
		L->contents++;
	}
	start = L->contents;
	while (isdigit(*L->contents)) {
		L->contents++;
	}
	L->contents++; /* here, we KNOW it is a period.. confirmed by matches_float */
	while (isdigit(*L->contents)) {
		L->contents++;
	}
	dist = (size_t)(L->contents - start);
	buf = malloc(dist + 1);
	memcpy(buf, start, dist);
	buf[dist] = 0;
	token = malloc(sizeof(AsmToken));
	token->type = ASMTOK_FLOAT;
	token->fval = strtod(buf, NULL) * sign;
	append_token(L, token);
	free(buf);
}

static void
lex_integer(Lexer* L) {
	char* buf;
	size_t dist;
	AsmToken* token;
	char* start;
	int sign = 1;
	if (*L->contents == '-') {
		sign = -1;
		L->contents++;
	}
	int base = (*L->contents == '0' && L->contents[1] == 'x') ? 16 : 10;
	/* if it's hex, ignore first two characters */
	if (base == 16) {
		L->contents += 2;
	}
	start = L->contents;
	while (isdigit(*L->contents) || (base == 16 && ((*L->contents >= 'a' && *L->contents <= 'f') || (*L->contents >= 'A' && *L->contents <= 'F')))) {
		L->contents++;
	}
	dist = (size_t)(L->contents - start);
	buf = malloc(dist + 1);
	memcpy(buf, start, dist);
	buf[dist] = 0;
	token = malloc(sizeof(AsmToken));
	token->type = ASMTOK_INTEGER;
	token->ival = (int64_t)strtoll(buf, NULL, base) * sign;
	append_token(L, token);
	free(buf);
}

static void
lex_character(Lexer* L) {
	/* starts on ' */
	AsmToken* token;
	L->contents++;
	token = malloc(sizeof(AsmToken));
	token->type = ASMTOK_INTEGER;
	token->ival = (int64_t)*L->contents++;
	if (*L->contents != '\'') {
		lex_die(L, "expected closing single quote");
	}
	append_token(L, token);
	L->contents++;
}	

static void
lex_string(Lexer* L) {
	char* buf;
	size_t dist;
	AsmToken* token;
	char* start;
	L->contents++; /* skip over opening quote */
	start = L->contents;
	while (*L->contents != '"') {
		L->contents++;
	}
	dist = (size_t)(L->contents - start);
	buf = malloc(dist + 1);
	memcpy(buf, start, dist);
	buf[dist] = 0;
	token = malloc(sizeof(AsmToken));
	token->type = ASMTOK_STRING;
	token->sval = buf;
	append_token(L, token);
	L->contents++; /* skip closing quote */
}

static void
lex_identifier(Lexer* L) {
	char* buf;
	size_t dist;
	AsmToken* token;
	char* start = L->contents;
	/* identifiers can start with a '.' (e.g. local label) */
	if (*L->contents == '.') {
		L->contents++;
	}
	while (isalnum(*L->contents) || *L->contents == '_') {
		L->contents++;
	}
	dist = (size_t)(L->contents - start);
	buf = malloc(dist + 1);
	memcpy(buf, start, dist);
	buf[dist] = 0;
	token = malloc(sizeof(AsmToken));
	token->type = ASMTOK_IDENTIFIER;
	token->sval = buf;
	append_token(L, token);
}

static void
lex_operator(Lexer* L) {
	AsmToken* token = malloc(sizeof(AsmToken));
	token->type = ASMTOK_OPERATOR;
	token->oval = *L->contents++;
	append_token(L, token);
}

int
tok_istype(AsmToken* token, enum AsmTokenType type) {
	/* token could be NULL... */
	return token && token->type == type;
}

AsmTokenList*
generate_tokens(const char* filename) {
	
	Lexer L;
	L.current_line = 1;
	L.handle = fopen(filename, "rb");
	if (!L.handle) {
		lex_die(&L, "couldn't open file %s for writing", filename);
	}
	L.tokens = malloc(sizeof(AsmTokenList));
	L.tokens->token = NULL;
	L.tokens->head = L.tokens;
	L.tokens->next = NULL;

	/* load contents into L.contents */
	uint64_t flen;
	fseek(L.handle, 0, SEEK_END);
	flen = ftell(L.handle);
	rewind(L.handle);
	L.contents = malloc(flen + 1);
	fread(L.contents, 1, flen, L.handle);
	L.contents[flen] = 0;
	fclose(L.handle); /* no need for the handle anymore */

	char* start = L.contents; /* used to free later */

	while (*L.contents) {
		if (*L.contents == ' ' || *L.contents == '\t' || *L.contents == 13) {
			L.contents++;
			continue;
		}
		if (*L.contents == '\n') {
			L.contents++;
			L.current_line++;
			continue;
		}
		if (*L.contents == ';') {
			L.contents++;
			while (*L.contents) {
				if (*L.contents == '\n') {
					L.contents++;
					L.current_line++;
					break;
				}
				L.contents++;
			}
			continue;
		}
		if (matches_identifier(&L)) {
			lex_identifier(&L);
		} else if (matches_character(&L)) {
			lex_character(&L);
		} else if (matches_float(&L)) {
			lex_float(&L);
		} else if (matches_integer(&L)) {
			lex_integer(&L);
		} else if (matches_string(&L)) {
			lex_string(&L);
		} else if (matches_operator(&L)) {
			lex_operator(&L);
		} else {
			lex_die(&L, "unknown character '%c' (%d)", *L.contents, *L.contents);
		}
	}

	free(start);
	return L.tokens;

}
