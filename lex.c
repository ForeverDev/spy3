#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "lex.h"

typedef struct LexState LexState;

struct LexState {
	TokenList* tokens;
	char* source;
	unsigned int current_line;
};

static void 
lex_die(LexState* L, const char* message, ...) {
	va_list args;
	va_start(args, message);
	printf("** SPYRE LEX ERROR **\n\tmessage: ");
	vprintf(message, args);
	printf("\n\tline: %d\n\n\n", L->current_line);
	va_end(args);
	exit(1);
}

static void
append_token(LexState* L, Token* token) {
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
	if (!isdigit(scan)) {
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
	
}

static void
lex_int(LexState* L) {
	int base = (*L->contents == '0' && L->contents[1] == 'x') ? 16 : 10;
	if (base == 16) {
		L->contents += 2;
	}
}

static void
lex_identifier(LexState* L) {

}

static void
lex_operator(LexState* L) {

}

TokenList*
generate_tokens_from_source(const char* filename) {
	
	LexState L;
	L.current_line = 1;
	L.source = NULL;
	L.tokens = malloc(sizeof(TokenList));
	L.tokens->token = NULL;
	L.tokens->next = NULL;
	L.tokens->prev = NULL;
		
	FILE* handle;
	unsigned long long flen;
	
	handle = fopen(filename, "rb");
	if (!handle) {
		lex_die(&L, "couldn't open '%s' for reading", filename);
	}		
	fseek(handle, 0, SEEK_END);
	flen = ftell(handle);
	rewind(handle);
	L.source = malloc(flen + 1);
	fread(L.source, 1, flen, handle);
	fclose(handle);

	while (*L.source) {
		if (on_float(&L)) {
			lex_float(&L);
		} else if (on_int(&L)) {
			lex_int(&L);
		} else if (on_identifier(&L)) {
			lex_identifier(&L);
		} else if (on_operator(&L)) {
			lex_operator(&L);
		} else {
			lex_die(&L, "unknown token '%c'", *L.source);
		}
	}

	free(L.source);
	return L.tokens;

}
