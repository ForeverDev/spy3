#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "parse.h"

typedef struct ParseState ParseState;

struct ParseState {
	TokenList* tokens;
	TreeNode* root_node; /* type == NODE_BLOCK */
	TreeNode* current_block;
};

/* parse functions */
static void parse_if(ParseState*);

/* misc */
static void parse_die(ParseState*, const char*, ...);
static void assert_operator(ParseState*, unsigned int);
static void eat_operator(ParseState*, unsigned int);

static void
parse_die(ParseState* P, const char* message, ...) {
	va_list args;
	va_start(args, message);	
	printf("\n\n** SPYRE PARSE ERROR **\n\tmessage: ");
	vprintf(message, args);
	if (P->tokens) {
		printf("\n\tline: %d\n", P->tokens->token->line);
	}
	printf("\n\n\n");
	va_end(args);
	exit(1);
}

static void
assert_operator(ParseState* P, unsigned int type) {
	if (!P->tokens || P->tokens->token->type != TOK_OPERATOR || P->tokens->token->oval != type) {
		parse_die(P, "expected something else lol");	
	}
}

/* this is just assert_operator and advance */
static void
eat_operator(ParseState* P, unsigned int type) {
	assert_operator(P, type);
	P->tokens = P->tokens->next;
}

static void parse_if(ParseState* P) {
	/* starts on token IF */
	P->tokens = P->tokens->next; /* skip IF */
	eat_operator(P, '(');
}

TreeNode*
generate_syntax_tree(TokenList* tokens) {

	ParseState P;
	P.tokens = tokens;
	P.root_node = malloc(sizeof(TreeNode));
	P.root_node->type = NODE_BLOCK;
	P.root_node->blockval = malloc(sizeof(TreeBlock));
	P.root_node->blockval->child = NULL;
	P.current_block = P.root_node;

	while (P.tokens) {
		if (P.tokens->token->type == TOK_IDENTIFIER) {
			const char* onword = P.tokens->token->sval;
			if (!strcmp(onword, "if")) {
				parse_if(&P);
			}	
		} else {
			P.tokens = P.tokens->next;
		}
	}
}
