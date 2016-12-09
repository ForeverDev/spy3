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
static void parse_while(ParseState*);
static void parse_for(ParseState*);
static void parse_block(ParseState*);
static void parse_statement(ParseState*);
static VarDeclaration* parse_declaration(ParseState*);
static ExpNode* parse_expression(ParseState*);
static FunctionDescriptor* parse_function_descriptor(ParseState*);
static Datatype* parse_datatype(ParseState*);
static void jump_out(ParseState*);

/* match functions (to determine if respective parse function should be called) */
static int matches_declaration(ParseState*);
static int matches_datatype(ParseState*);
static int matches_array(ParseState*);
static int matches_pointer(ParseState*);

/* exp stack functions */
static void expstack_push(ExpStack**, ExpNode*);
static ExpNode* expstack_pop(ExpStack**);
static ExpNode* expstack_top(ExpStack**);

/* token functions */
static int is_ident(ParseState*);
static int is_op(ParseState*);
static int on_ident(ParseState*, const char*);
static int on_op(ParseState*, char);

/* misc */
static void print_expression(ExpNode*, int);
static void print_tree(TreeNode*, int);
static void print_datatype(Datatype*, int);
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
		print_token(P->tokens->token);
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
		Token* at = i->token;
		if (at->type == TOK_IDENTIFIER && is_keyword(at->sval)) {
			parse_die(P, "unexpected keyword '%s' in expression", at->sval);
		}
		if (at->type != TOK_OPERATOR) {
			continue;
		}
		if (at->oval == inc) {
			count++;
		} else if (at->oval == dec) {
			count--;
		}
		if (count == 0) break;
	}

	if (!i) {
		parse_die(P, "unexpected EOF while parsing expression");
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

static int
is_ident(ParseState* P) {
	return P->tokens->token->type == TOK_IDENTIFIER;
}

static int
on_ident(ParseState* P, const char* word) {
	return is_ident(P) && !strcmp(P->tokens->token->sval, word);
}

static int
is_op(ParseState* P) {
	return P->tokens->token->type == TOK_OPERATOR;
}

static int
on_op(ParseState* P, char op) {
	return is_op(P) && P->tokens->token->oval == op;
}

static void
append_node(ParseState* P, TreeNode* node) {
	node->next = NULL;
	node->prev = NULL;
	TreeNode* append_to = NULL;
	TreeNode* target = P->append_target;
	if (target) {
		switch (target->type) {
			case NODE_IF:
				target->ifval->child = node;
				break;
			case NODE_WHILE:
				target->whileval->child = node;
				break;
			case NODE_FOR:
				target->forval->child = node;
				break;
		}
		node->parent = target;
	} else {
		/* if no target, throw into current block */
		TreeBlock* block = P->current_block->blockval;
		if (!block->child) {
			block->child = node;
		} else {
			TreeNode* scan = block->child;
			while (scan->next) {
				scan = scan->next;
			}
			scan->next = node;
			node->prev = scan;
		}
		node->parent = P->current_block;
	}
	/* appendable: function, if, while, for */
	if (node->type == NODE_IF
		|| node->type == NODE_WHILE
		|| node->type == NODE_FOR) {
		P->append_target = node;
	} else {
		P->append_target = NULL;
	}
	if (node->type == NODE_BLOCK) {
		P->current_block = node;
	}
}

/* macro used in print_tree and print_expression */
#define INDENT(n) for(int _=0;_<(n);_++)printf("  ")

static void
print_tree(TreeNode* tree, int indent) {
	if (!tree) {
		printf("\n");
		return;
	};
	INDENT(indent);
	switch (tree->type) {
		case NODE_BLOCK:
			printf("BLOCK: [\n");
			for (TreeNode* i = tree->blockval->child; i; i = i->next) {
				print_tree(i, indent + 1);
			}
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_IF:
			printf("IF: [\n");
			INDENT(indent + 1);
			printf("CONDITION: [\n");
			print_expression(tree->ifval->condition, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_tree(tree->ifval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_WHILE:
			printf("WHILE: [\n");
			INDENT(indent + 1);
			printf("CONDITION: [\n");
			print_expression(tree->whileval->condition, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_tree(tree->whileval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_FOR:
			printf("FOR: [\n");
			INDENT(indent + 1);
			printf("INITIALIZER: [\n");
			print_expression(tree->forval->init, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CONDITION: [\n");
			print_expression(tree->forval->condition, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("STATEMENT: [\n");
			print_expression(tree->forval->statement, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_tree(tree->forval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_STATEMENT:
			printf("STATEMENT: [\n");
			print_expression(tree->stateval->exp, indent + 1);
			INDENT(indent);
			printf("]\n");
			break;
	}
}

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

static void
print_datatype(Datatype* data, int indent) {
	if (!data) return;
	INDENT(indent);
	printf("[\n");
	INDENT(indent + 1);
	printf("TYPE NAME: ");
	switch (data->type) {
		case DATA_INT:
			printf("int\n");
			break;
		case DATA_FLOAT:
			printf("float\n");
			break;	
		case DATA_BYTE:
			printf("byte\n");
			break;
		case DATA_FPTR:
			printf("(function pointer)\n");
			/* print arguments */
			INDENT(indent + 1);
			printf("ARGUMENTS: [\n");
			for (DatatypeList* i = data->fdesc->arguments; i; i = i->next) {
				print_datatype(i->data, indent + 2);
			}
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("RETURN TYPE: [\n");
			print_datatype(data->fdesc->return_type, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			break;
		default:
			printf("(N/A)\n");
			break;
	}	
	INDENT(indent + 1);
	printf("PTR_DIM: %d\n", data->ptr_dim);
	INDENT(indent + 1);
	printf("ARRAY_DIM: %d\n", data->array_dim);
	if (data->array_dim > 0) {
		INDENT(indent + 1);
		printf("ARRAY_DIM_SIZE: [\n");
	}
	for (int i = 0; i < data->array_dim; i++) {
		INDENT(indent + 2);
		printf("DIM_%d: %d\n", i, data->array_size[i]);
	}
	if (data->array_dim > 0) {
		INDENT(indent + 1);
		printf("]\n");
	}
	INDENT(indent);
	printf("]\n");
}

/* helper macro for the matches functions.  requires TokenList
 * 'start' that points to where matches started */
#define MATCH_FALSE() P->tokens = start; return 0
#define MATCH_TRUE() P->tokens = start; return 1

/* example declarations
 *
 * x: int; // integer
 * x: [10]int; // array of 10 ints
 * x: [10]^int; // array of 10 int pointers
 * x: [10](int, float) -> []int; // array of 10 function pointers that take int, float as params and return int array
 */
static int
matches_declaration(ParseState* P) {
	TokenList* start = P->tokens;
	if (!is_ident(P)) {
		MATCH_FALSE();
	}
	P->tokens = P->tokens->next;	
	if (!on_op(P, ':')) {
		MATCH_FALSE();
	}
	P->tokens = P->tokens->next;
	if (!matches_datatype(P)) {
		MATCH_FALSE();
	}
	MATCH_TRUE();
}

/* sample datatypes
 *
 * []int
 * []^int
 * [](int, ^float) -> void
 * []<T>(int, ^T) -> void
 *
 * int
 * float
 *
 * ^^int
 * ^float
 *
*/

static int
matches_datatype(ParseState* P) {
	TokenList* start = P->tokens;
	
	if (on_op(P, '[')) {
		MATCH_TRUE();
	}

	if (on_op(P, '^')) {
		MATCH_TRUE();
	}

	if (on_op(P, '(')) {
		MATCH_TRUE();
	}

	/* TODO detect defined structs as well */
	if (on_ident(P, "int") || on_ident(P, "float") || on_ident(P, "byte")) {
		MATCH_TRUE();
	}
	
	MATCH_FALSE();
}

static int
matches_array(ParseState* P) {
	return 0;
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
	
	if (tree != NULL) {
		parse_die(P, "an expression must not have more than one result");
	}

	return ret;

}

static void
jump_out(ParseState* P) {
	/* skip } */
	P->tokens = P->tokens->next;
	do {
		P->current_block = P->current_block->parent;
	} while (P->current_block->type != NODE_BLOCK);
}

static void
parse_block(ParseState* P) {
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_BLOCK;
	node->blockval = malloc(sizeof(TreeBlock));
	node->blockval->child = NULL;

	/* skip over { */
	P->tokens = P->tokens->next;

	append_node(P, node);
}

static FunctionDescriptor*
parse_function_descriptor(ParseState* P) {
	/* expects to start on token ( */
	/* function descriptor example:
	 * (int, ^float) -> void
	 */
	FunctionDescriptor* fdesc = malloc(sizeof(FunctionDescriptor));
	fdesc->arguments = NULL;
	fdesc->return_type = NULL;

	P->tokens = P->tokens->next; /* skip ( */
	
	while (!on_op(P, ')')) {
		Datatype* data = parse_datatype(P);
		if (!fdesc->arguments) {
			fdesc->arguments = malloc(sizeof(DatatypeList));
			fdesc->arguments->data = data;
			fdesc->arguments->next = NULL;	
		} else {
			DatatypeList* scan = fdesc->arguments;
			while (scan->next) {
				scan = scan->next;
			}
			DatatypeList* new = malloc(sizeof(DatatypeList));
			new->data = data;
			new->next = NULL;
			scan->next = new;
		}
		/* should now be on final token of argument... increase by one */
		P->tokens = P->tokens->next;
		if (!on_op(P, ')')) {
			eat_operator(P, ',');
			if (on_op(P, ')')) {
				parse_die(P, "expected datatype after ','");
			}
		}
	}

	P->tokens = P->tokens->next; /* skip ) */
	eat_operator(P, SPEC_ARROW);
	fdesc->return_type = parse_datatype(P);

	return fdesc;
	
}

static Datatype*
parse_datatype(ParseState* P) {
	Datatype* data = malloc(sizeof(Datatype));
	data->array_dim = 0;
	data->ptr_dim = 0;
	data->fdesc = NULL;
	data->sdesc = NULL;
	data->array_size = NULL;
	data->type = DATA_NOTYPE;
	
	/* is array type */
	/* TODO handle more than one array */
	while (on_op(P, '[')) {
		P->tokens = P->tokens->next;
		data->array_dim++;
		data->array_size = realloc(data->array_size, data->array_dim * sizeof(unsigned int));
		data->array_size[data->array_dim - 1] = P->tokens->token->ival;
		P->tokens = P->tokens->next;
		P->tokens = P->tokens->next; /* skip closing ] */
	}

	/* is pointer type */
	while (on_op(P, '^')) {
		data->ptr_dim++;
		P->tokens = P->tokens->next;
	}

	/* now it should be on the typename or function descriptor... e.g.
	 * int
	 * () -> void
	 * <T>() -> void
	 * etc
	 */
	if (on_op(P, '(')) {
		/* if reached, is function pointer */
		data->type = DATA_FPTR;	

		/* handy dandy function */
		data->fdesc = parse_function_descriptor(P);
	} else {
		/* not function... primitive type or struct (struct not handeled yet) */
		if (on_ident(P, "int")) {
			printf("2\n");
			data->type = DATA_INT;
		} else if (on_ident(P, "float")) {
			printf("3\n");
			data->type = DATA_FLOAT;
		} else if (on_ident(P, "byte")) {
			data->type = DATA_BYTE;
		} else {
			/* TODO handle struct */
			if (is_ident(P)) {
				parse_die(P, "unknown typename '%s'", P->tokens->token->sval);
			}
			parse_die(P, "expected typename");
		}
	}
	
	return data;

}

static VarDeclaration*
parse_declaration(ParseState* P) {
	VarDeclaration* decl = malloc(sizeof(VarDeclaration));

	/* read identifier */
	decl->name = malloc(strlen(P->tokens->token->sval) + 1);
	strcpy(decl->name, P->tokens->token->sval);
	P->tokens = P->tokens->next;

	/* skip ':' */
	P->tokens = P->tokens->next;

	/* read datatype */
	decl->datatype = parse_datatype(P);

	return decl;
}

static void
parse_statement(ParseState* P) {
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_STATEMENT;
	node->stateval = malloc(sizeof(TreeStatement));
	
	/* starts on first token of expression... parse it */
	print_token(P->tokens->token);
	mark_operator(P, SPEC_NULL, ';');
	node->stateval->exp = parse_expression(P);
	P->tokens = P->tokens->next; /* skip ; */

	append_node(P, node);
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
	mark_operator(P, '(', ')');
	node->ifval->condition = parse_expression(P);
	P->tokens = P->tokens->next; /* skip ')' */

	append_node(P, node);

}

static void 
parse_while(ParseState* P) {
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_WHILE;
	node->whileval = malloc(sizeof(TreeWhile));
	node->whileval->child = NULL;

	/* starts on token WHILE */
	P->tokens = P->tokens->next; /* skip WHILE */
	eat_operator(P, '(');
	mark_operator(P, '(', ')');
	node->whileval->condition = parse_expression(P);
	P->tokens = P->tokens->next; /* skip ')' */

	append_node(P, node);

}

static void
parse_for(ParseState* P) {
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_FOR;
	node->forval = malloc(sizeof(TreeFor));
	node->forval->child = NULL;
	
	/* starts on token FOR */
	P->tokens = P->tokens->next; /* skip FOR */
	eat_operator(P, '(');
	mark_operator(P, SPEC_NULL, ';');
	node->forval->init = parse_expression(P);
	P->tokens = P->tokens->next; /* skip ; */
	mark_operator(P, SPEC_NULL, ';');
	node->forval->condition = parse_expression(P);
	P->tokens = P->tokens->next; /* skip ; */
	mark_operator(P, '(', ')');
	node->forval->statement = parse_expression(P);
	P->tokens = P->tokens->next; /* skip ) */

	append_node(P, node);
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
	P.append_target = NULL;

	while (P.tokens) {
		if (on_ident(&P, "if")) {
			parse_if(&P);
		} else if (on_ident(&P, "while")) {
			parse_while(&P);
		} else if (on_ident(&P, "for")) {
			parse_for(&P);
		} else if (matches_declaration(&P)) {
			VarDeclaration* var = parse_declaration(&P);
			print_datatype(var->datatype, 0);
		} else if (on_op(&P, '{')) {
			parse_block(&P);
		} else if (on_op(&P, '}')) {
			if (P.current_block == P.root_node) {
				parse_die(&P, "token '}' doesn't close anything");
			}
			jump_out(&P);
		} else {
			parse_statement(&P);
		}
	}

	if (P.current_block != P.root_node) {
		parse_die(&P, "expected '}' before EOF");
	}

	print_tree(P.root_node, 0);

	/* TODO cleanup tokens + other stuff */

	return P.root_node;

}

