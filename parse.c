#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "parse.h"

typedef struct ParseState ParseState;
typedef struct ExpStack ExpStack;
typedef struct OpEntry OpEntry;

struct ParseState {
	TokenList* tokens;
	TokenList* marked;
	TokenList* focus; /* used for parse_exprecsion and helper funcs */
	TreeNode* root_node; /* type == NODE_BLOCK */
	TreeNode* current_block;
	TreeNode* append_target; /* what to append to */
};

struct ExpStack {
	ExpNode* node;
	ExpStack* prev;
	ExpStack* next;
};

struct OpEntry {
	unsigned int prec;
	enum {
		ASSOC_LEFT = 1,
		ASSOC_RIGHT = 2
	} assoc;
	enum {
		OP_UNARY = 1,
		OP_BINARY = 2	
	} operands;
};

/* parse functions */
static void parse_if(ParseState*);
static ExpNode* parse_expression(ParseState*);

/* exp stack functions */
static void expstack_push(ExpStack**, ExpNode*);
static ExpNode* expstack_pop(ExpStack**);
static ExpNode* expstack_top(ExpStack**);

/* misc */
static void print_expression(ExpNode*, int);
static void print_tree(TreeNode*, int);
static void parse_die(ParseState*, const char*, ...);
static void assert_operator(ParseState*, char);
static void eat_operator(ParseState*, char);
static void mark_operator(ParseState*, char, char);
static void append_node(ParseState*, TreeNode*);
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
assert_operator(ParseState* P, char type) {
	if (!P->tokens || P->tokens->token->type != TOK_OPERATOR || P->tokens->token->oval != type) {
		parse_die(P, "expected something else lol");	
	}
}

/* this is just assert_operator and advance */
static void
eat_operator(ParseState* P, char type) {
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

static void
expstack_push(ExpStack** stack, ExpNode* node) {
	ExpStack* append = malloc(sizeof(ExpStack));
	append->node = node;
	append->next = NULL;
	append->prev = NULL;
	if (!(*stack)) {
		/* if stack is null, just set to append */
		*stack = append;
	} else {
		/* otherwise, append it */
		ExpStack* scan = *stack;
		while (scan->next) {
			scan = scan->next;
		}
		scan->next = append;
		append->prev = scan;
	}
}

static ExpNode*
expstack_pop(ExpStack** stack) {
	if (!(*stack)) {
		return NULL;
	}
	ExpStack* scan = *stack;
	while (scan->next) {
		scan = scan->next;
	}
	ExpNode* ret = scan->node;
	/* is scan == stack? if so, stack should become NULL */
	if (scan == *stack) {
		*stack = NULL;
	} else {
		scan->prev->next = NULL;
	}
	free(scan);
	return ret;
}

static ExpNode*
expstack_top(ExpStack** stack) {
	if (!(*stack)) {
		return NULL;
	}
	ExpStack* scan = *stack;
	while (scan->next) {
		scan = scan->next;
	}
	return scan->node;
}

static void
append_node(ParseState* P, TreeNode* node) {
	TreeNode* target = P->append_target ?: P->current_block;
	TreeNode* append_to = NULL;
	switch (target->type) {
		case NODE_IF:
			append_to = target->ifval->child;
			break;
				
	}
}

#define INDENT(n) for(int _=0;_<(n);_++)fputc('\t',stdout)
static void
print_expression(ExpNode* exp, int indent) {
	if (!exp) return;
	INDENT(indent);
	switch (exp->type) {
		case EXP_NOEXP:
			break;
		case EXP_UNARY: {
			char* str = tokcode_tostring(exp->uval->optype);
			printf("%s\n", str);
			free(str);
			print_expression(exp->uval->operand, indent + 1);
			break;	
		}
		case EXP_BINARY: {
			char* str = tokcode_tostring(exp->bval->optype);
			printf("%s\n", str);
			free(str);
			print_expression(exp->bval->left, indent + 1);
			print_expression(exp->bval->right, indent + 1);
			break;
		}	
		case EXP_INTEGER:
			printf("%lld\n", exp->ival);	
			break;
		case EXP_FLOAT:
			printf("%f\n", exp->fval);	
			break;
	}
}

/* expects end of exprecsion to be marked... NOT inclusive */
static ExpNode*
parse_expression(ParseState* P) {

	/* first, this function links ExpNodes together via a stack (ExpStack)
	 * next, the stacks are converted into a tree and linked together by the
	 * proper fields in the actual ExpNode object */

	static const OpEntry prec[127] = {
		[',']				= {1, ASSOC_LEFT, OP_BINARY},
		['=']				= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_INC_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_DEC_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_MUL_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_DIV_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_MOD_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_SHL_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_SHR_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_AND_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_OR_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_XOR_BY]		= {2, ASSOC_RIGHT, OP_BINARY},
		[SPEC_LOG_AND]		= {3, ASSOC_LEFT, OP_BINARY},
		[SPEC_LOG_OR]		= {3, ASSOC_LEFT, OP_BINARY},
		[SPEC_EQ]			= {4, ASSOC_LEFT, OP_BINARY},
		[SPEC_NEQ]			= {4, ASSOC_LEFT, OP_BINARY},
		['>']				= {6, ASSOC_LEFT, OP_BINARY},
		[SPEC_GE]			= {6, ASSOC_LEFT, OP_BINARY},
		['<']				= {6, ASSOC_LEFT, OP_BINARY},
		[SPEC_LE]			= {6, ASSOC_LEFT, OP_BINARY},
		['|']				= {7, ASSOC_LEFT, OP_BINARY},
		[SPEC_SHL]			= {7, ASSOC_LEFT, OP_BINARY},
		[SPEC_SHR]			= {7, ASSOC_LEFT, OP_BINARY},
		['+']				= {8, ASSOC_LEFT, OP_BINARY},
		['-']				= {8, ASSOC_LEFT, OP_BINARY},
		['*']				= {9, ASSOC_LEFT, OP_BINARY},
		['%']				= {9, ASSOC_LEFT, OP_BINARY},
		['/']				= {9, ASSOC_LEFT, OP_BINARY},
		['&']				= {10, ASSOC_RIGHT, OP_UNARY},
		['^']				= {10, ASSOC_RIGHT, OP_UNARY},
		['!']				= {10, ASSOC_RIGHT, OP_UNARY},
		[SPEC_TYPENAME]		= {10, ASSOC_RIGHT, OP_UNARY},
		['.']				= {11, ASSOC_LEFT, OP_BINARY},
		[SPEC_INC_ONE]		= {11, ASSOC_LEFT, OP_UNARY},
		[SPEC_DEC_ONE]		= {11, ASSOC_LEFT, OP_UNARY}
	};

	ExpStack* operators = NULL;	
	ExpStack* postfix = NULL;

	/* first, just use shunting yard to organize the tokens */

	for (P->focus = P->tokens; P->focus != P->marked; P->focus = P->focus->next) {
		Token* tok = P->focus->token;	
		if (tok->type == TOK_OPERATOR) {
			/* use assoc to make sure it exists */
			if (prec[tok->oval].assoc) {
				ExpNode* top;
				const OpEntry* info = &prec[tok->oval];
				while (1) {
					top = expstack_top(&operators);
					if (!top) break;
					int top_is_unary = top->type == EXP_UNARY;
					int top_is_binary = top->type == EXP_BINARY;
					const OpEntry* top_info;
					if (top_is_unary) {
						/* unary operator is on top of stack */
						top_info = &prec[top->uval->optype];
					} else if (top_is_binary) {
						/* binary operator is on top of stack */
						top_info = &prec[top->bval->optype];
					}
					/* found open parenthesis, break */
					if (top_is_unary && top->uval->optype == '(') {
						break;
					}
					if (info->assoc == ASSOC_LEFT) {
						if (info->prec > top_info->prec) break;
					} else {
						if (info->prec >= top_info->prec) break;
					}
					expstack_push(&postfix, expstack_pop(&operators));
				}
				ExpNode* push = malloc(sizeof(ExpNode));
				push->parent = NULL;
				if (info->operands == OP_BINARY) {
					push->type = EXP_BINARY;
					push->bval = malloc(sizeof(BinaryOp));
					push->bval->optype = tok->oval;
					push->bval->left = NULL;
					push->bval->right = NULL;
				} else if (info->operands == OP_UNARY) {
					push->type = EXP_UNARY;
					push->uval = malloc(sizeof(UnaryOp));
					push->uval->optype = tok->oval;
					push->uval->operand = NULL;
				}
				expstack_push(&operators, push);
			} else if (tok->oval == '(') {

			} else if (tok->oval == ')') {

			}
		} else if (tok->type == TOK_INTEGER) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;	
			push->type = EXP_INTEGER;
			push->ival = tok->ival;
			expstack_push(&postfix, push);
		} else if (tok->type == TOK_FLOAT) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			push->type = EXP_FLOAT;
			push->fval = tok->fval;
			expstack_push(&postfix, push);
		}
	}

	while (expstack_top(&operators)) {
		expstack_push(&postfix, expstack_pop(&operators));
	}

	/* === STAGE TWO ===
	 * convert from postix to a tree
	 */

	ExpStack* tree = NULL;

	static const char* malformed = "malfored expression";

	for (ExpStack* i = postfix; i; i = i->next) {
		ExpNode* at = i->node;
		if (at->type == EXP_INTEGER || at->type == EXP_FLOAT) {
			/* just a literal? append to tree */
			expstack_push(&tree, at);	
		} else if (at->type == EXP_UNARY) {
			ExpNode* operand = expstack_pop(&tree);
			if (!operand) {
				parse_die(P, malformed);
			}
			operand->parent = at;
			at->uval->operand = operand;
			expstack_push(&tree, at);
		} else if (at->type == EXP_BINARY) {
			/* pop the leaves off of the stack */
			ExpNode* leaf[2];
			for (int j = 0; j < 2; j++) {
				leaf[j] = expstack_pop(&tree);
				if (!leaf[j]) {
					parse_die(P, malformed);
				}
				leaf[j]->parent = at;
				leaf[j]->side = j == 1 ? LEAF_LEFT : LEAF_RIGHT;
			}
			/* swap order */
			at->bval->left = leaf[1];
			at->bval->right = leaf[0];
			/* throw the branch back onto the stack */
			expstack_push(&tree, at);
		}
	}

	P->tokens = P->focus;

	ExpNode* ret = expstack_pop(&tree);
	print_expression(ret, 0);
	
	if (tree != NULL) {
		parse_die(P, "an expression must not have more than one result");
	}

	return ret;

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
	P.append_target = P.current_block;

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
