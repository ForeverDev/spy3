#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "parse.h"

typedef struct ParseState ParseState;

struct ParseState {
	TokenList* tokens;
	TokenList* marked;
	TreeNode* root_node; /* type == NODE_BLOCK */
	TreeNode* current_block;
};

/* parse functions */
static void parse_if(ParseState*);
static ExpNode* parse_expression(ParseState*);

/* misc */
static void parse_die(ParseState*, const char*, ...);
static void assert_operator(ParseState*, unsigned int);
static void eat_operator(ParseState*, unsigned int);
static void mark_operator(ParseState*, unsigned int, unsigned int);
static int is_keyword(const char*);

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

static int
is_keyword(const char* word) {
	static const char* keywords[] = {
		"if", "while", "for", "do", "struct",
		"return", "continue", "break", NULL
	};
	for (const char** i = keywords; *i; i++) {
		if (!strcmp(*i, word)) {
			return 1;
		}
	}
	return 0;
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

/* ends on marked operator */
static void
mark_operator(ParseState* P, char inc, char dec) {

	int count = 1;
	TokenList* i;
	
	for (i = P->tokens; i && count > 0; i = i->next) {
		if (i->token->type != TOK_OPERATOR) {
			continue;
		}
		if (i->token->oval == inc) {
			count++;
		} else if (i->token->oval == dec) {
			count--;
		}
	}

	P->marked = i;	

}

/* expects end of expression to be marked... NOT inclusive */
static ExpNode*
parse_expression(ParseState* P) {

}

static void 
parse_if(ParseState* P) {
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_IF;
	node->ifval = malloc(sizeof(TreeIf));
	node->ifval->child = NULL;

	/* starts on token IF */
	P->tokens = P->tokens->next; /* skip IF */
	eat_operator(P, '(');
	mark_operator(P, SPEC_NULL, ')');
	node->ifval->condition = parse_expression(P);
	P->tokens = P->tokens->next; /* skip ')' */

}

TreeNode*
generate_syntax_tree(TokenList* tokens) {

	ParseState P;
	P.tokens = tokens;
	P.marked = NULL;
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
