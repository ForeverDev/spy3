#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "parse.h"

typedef struct ExpStack ExpStack;
typedef struct OpEntry OpEntry;

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
static void parse_struct(ParseState*);
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
static void register_local(ParseState*, VarDeclaration*);
static VarDeclaration* find_local(ParseState*, const char*);
static const Datatype* typecheck_expression(ParseState*, ExpNode*);
static int types_match(const Datatype*, const Datatype*);
static TreeStruct* find_struct(ParseState*, const char*);
static void print_debug_info(ParseState*);
static void print_struct_info(TreeStruct*, unsigned int);
static char* tostring_datatype(const Datatype*);
static VarDeclaration* find_field(const TreeStruct*, const char*);

static void
parse_die(ParseState* P, const char* message, ...) {
	va_list args;
	va_start(args, message);	
	printf("\n\n** SPYRE PARSE ERROR **\n\tmessage: ");
	vprintf(message, args);
	if (P->tokens) {
		printf("\n\tline: %d\n", P->tokens->token->line);
	}
	printf("\n\n");
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
		parse_die(P, "expected token (%s), got token (%s)", tokcode_tostring(type), token_tostring(P->tokens->token));
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
	return P->tokens && P->tokens->token->type == TOK_IDENTIFIER;
}

static int
on_ident(ParseState* P, const char* word) {
	return is_ident(P) && !strcmp(P->tokens->token->sval, word);
}

static int
is_op(ParseState* P) {
	return P->tokens && P->tokens->token->type == TOK_OPERATOR;
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
			case NODE_FUNC_IMPL:
				/* found function? reset offset */
				P->current_offset = target->funcval->desc->stack_space;
				P->current_function = target;
				target->funcval->child = node;
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
		|| node->type == NODE_FOR
		|| node->type == NODE_FUNC_IMPL) {
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
print_debug_info(ParseState* P) {
	printf("DEFINED STRUCTS:\n");
	for (TreeStructList* i = P->defined_structs; i; i = i->next) {
		print_struct_info(i->str, 1);
		INDENT(1);
		printf("(sizeof(%s) == %d)\n", i->str->name, i->str->desc->size);
	}
	printf("\nSYNTAX TREE:\n");
	print_tree(P->root_node, 1);
}

static void
print_struct_info(TreeStruct* str, unsigned int indent) {
	INDENT(indent);
	printf("struct %s {\n", str->name);
	for (VarDeclarationList* i = str->desc->fields; i; i = i->next) {
		VarDeclaration* var = i->decl;
		char* data_str = tostring_datatype(var->datatype);
		INDENT(indent + 1);
		printf("%s: %s\n", var->name, data_str);
		free(data_str);
	}
	INDENT(indent);
	printf("}\n");
}

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
			INDENT(indent + 1);
			printf("LOCALS: [\n");
			for (VarDeclarationList* i = tree->blockval->locals; i; i = i->next) {
				VarDeclaration* var = i->decl;
				char* data_str = tostring_datatype(var->datatype);
				INDENT(indent + 2);
				printf("%s: %s\n", var->name, data_str);
				free(data_str);
			}
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("CHILDREN: [\n");
			for (TreeNode* i = tree->blockval->child; i; i = i->next) {
				print_tree(i, indent + 2);
			}
			INDENT(indent + 1);
			printf("]\n");
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
		case NODE_FUNC_IMPL:	
			printf("FUNC IMPL: [\n");
			INDENT(indent + 1);
			printf("NAME: %s\n", tree->funcval->name);
			INDENT(indent + 1);
			printf("ARGUMENTS: [\n");
			for (DatatypeList* i = tree->funcval->desc->arguments; i; i = i->next) {
				print_datatype(i->data, indent + 2);
			}
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("RETURN TYPE: \n");
			print_datatype(tree->funcval->desc->return_type, indent + 1);
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_tree(tree->funcval->child, indent + 2);
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
		case EXP_IDENTIFIER:
			printf("%s\n", exp->sval);
			break;
		case EXP_CALL:
			printf("CALL (computed = %d)\n", exp->cval->computed);
			print_expression(exp->cval->fptr, indent + 1);
			print_expression(exp->cval->arguments, indent + 1);
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
		case DATA_VOID:
			printf("void\n");
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

static char*
tostring_datatype(const Datatype* data) {
	char* buf = calloc(1, 1024); /* definitely enough space */
	for (int i = 0; i < data->array_dim; i++) {
		strcat(buf, "[]");
	}
	for (int i = 0; i < data->ptr_dim; i++) {
		strcat(buf, "^");
	}
	switch (data->type) {
		case DATA_INT:
			strcat(buf, "int");
			break;
		case DATA_FLOAT:
			strcat(buf, "float");
			break;
		case DATA_BYTE:
			strcat(buf, "byte");
			break;
		case DATA_VOID:
			strcat(buf, "void");
			break;
		case DATA_STRUCT: 
			strcat(buf, data->sdesc->name);
			break;
		case DATA_FPTR:
			strcat(buf, "(");
			for (DatatypeList* i = data->fdesc->arguments; i; i = i->next) {
				char* arg = tostring_datatype(i->data);
				sprintf(buf + strlen(buf), "%s", arg);
				free(arg);
				if (i->next) {
					strcat(buf, ", ");
				}
			}
			strcat(buf, ") -> ");
			char* ret = tostring_datatype(data->fdesc->return_type);
			sprintf(buf + strlen(buf), "%s", ret);
			free(ret);
			break;
	}
	return buf;
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

	if (on_ident(P, "int") || on_ident(P, "float") || on_ident(P, "byte") || on_ident(P, "struct")) {
		MATCH_TRUE();
	}

	/* check defined structs */
	if (is_ident(P) && find_struct(P, P->tokens->token->sval)) {
		MATCH_TRUE();
	} 
	
	MATCH_FALSE();
}

static int
matches_array(ParseState* P) {
	return 0;
}

static TreeStruct*
find_struct(ParseState* P, const char* name) {
	for (TreeStructList* i = P->defined_structs; i; i = i->next) {
		if (!strcmp(i->str->name, name)) {
			return i->str;
		}
	}
	return NULL;
}

static VarDeclaration*
find_field(const TreeStruct* str, const char* name) {
	for (VarDeclarationList* i = str->desc->fields; i; i = i->next) {
		if (!strcmp(i->decl->name, name)) {
			return i->decl;
		}
	}
	return NULL;
}

static VarDeclaration*
find_local(ParseState* P, const char* name) {
	for (TreeNode* i = P->current_block; i; i = i->parent) {
		if (i->type != NODE_BLOCK) {
			continue;
		}
		for (VarDeclarationList* j = i->blockval->locals; j; j = j->next) {
			if (!strcmp(j->decl->name, name)) {
				return j->decl;
			}
		}
	}
	return NULL;
}

static void
register_local(ParseState* P, VarDeclaration* local) {
	if (P->append_target) {
		parse_die(P, "variables can only be declared in a block");
	}
	TreeBlock* block = P->current_block->blockval;
	VarDeclarationList* new = malloc(sizeof(VarDeclarationList));
	new->decl = local;
	new->next = NULL;
	if (!block->locals) {
		block->locals = new;
	} else {
		VarDeclarationList* scan = block->locals;
		while (scan->next) {
			scan = scan->next;
		}
		scan->next = new;
	}
}

static int
types_match(const Datatype* a, const Datatype* b) {
	if (a->type != b->type) {
		return 0;
	}
	if (a->array_dim != b->array_dim) {
		return 0;
	}
	if (a->ptr_dim != b->ptr_dim) {
		return 0;
	}
	return 1;
}

static const Datatype*
typecheck_expression(ParseState* P, ExpNode* exp) {
	if (!exp) return NULL;
	switch (exp->type) {
		case EXP_NOEXP:
			return NULL;
		case EXP_INTEGER:
			return exp->eval = P->type_int;
		case EXP_FLOAT:
			return exp->eval = P->type_float;
		case EXP_UNARY:
			return exp->eval = typecheck_expression(P, exp->uval->operand);
		case EXP_IDENTIFIER: {
			/* TODO handle parent being '.' */
			VarDeclaration* var = find_local(P, exp->sval);
			if (!var) {
				parse_die(P, "undeclared identifier '%s'", exp->sval);
			}
			return exp->eval = var->datatype;
		}
		case EXP_BINARY: {
			switch (exp->bval->optype) {
				case '.': {
					/* LHS must be either an identifier or another '.' */
					ExpNode* lhs = exp->bval->left;
					ExpNode* rhs = exp->bval->right;
					if (rhs->type != EXP_IDENTIFIER) {
						parse_die(P, "the right side of the '.' operator must be an identifier");
					}
					if (lhs->type == EXP_IDENTIFIER) {
						/* now we know both of them are identifiers... this is the lowest level of
						 * the dot chain... typecheckable */
						VarDeclaration* var = find_local(P, lhs->sval);
						if (!var) {
							parse_die(P, "undeclared identifier '%s'", lhs->sval);
						}
						if (var->datatype->type != DATA_STRUCT) {
							parse_die(P, "attempt to use '.' operator on a non-object");
						}
						VarDeclaration* field = find_field(var->datatype->sdesc, rhs->sval);
						if (!field) {
							parse_die(P, 
								"'%s' is not a valid field of '%s' (type of '%s' is '%s')",
								rhs->sval,
								lhs->sval,
								lhs->sval,
								tostring_datatype(var->datatype)
							);
						}
						/* identifiers can't get evaluated types but the parent operator can */
						lhs->eval = NULL;
						rhs->eval = NULL;
						return exp->eval = field->datatype;
					} else if (lhs->type == EXP_BINARY && lhs->bval->optype == '.') {
						/* it's a dot chain... the struct we need to check will be returned
						 * from a recursive typecheck call */
						const Datatype* str = typecheck_expression(P, lhs);
						VarDeclaration* field = find_field(str->sdesc, rhs->sval);
						if (!field) {
							parse_die(P, 
								"'%s' is not a valid field of struct '%s'", 
								rhs->sval,
								tostring_datatype(str)
							);
						}
						rhs->eval = NULL;
						return exp->eval = field->datatype;
					} else {
						parse_die(P, "the left side of the '.' operator must be an identifier");
					}
					break;
				}
				default: {
					const Datatype* left = typecheck_expression(P, exp->bval->left);
					const Datatype* right = typecheck_expression(P, exp->bval->right);
					if (!types_match(left, right)) {
						parse_die(P, 
							"attempt to use operator (%s) on mismatched types '%s' and '%s'", 
							tokcode_tostring(exp->bval->optype),
							tostring_datatype(left),
							tostring_datatype(right)
						);
					}
					return exp->eval = left; /* they're both identical, doesn't matter which is returned */
				}
			}
		}
	}
	return NULL;
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
		[SPEC_DEC_ONE]		= {11, ASSOC_LEFT, OP_UNARY},
		[SPEC_CALL]			= {11, ASSOC_LEFT, OP_UNARY}
	};

	ExpStack* operators = NULL;	
	ExpStack* postfix = NULL;

	/* first, just use shunting yard to organize the tokens */

	for (; P->tokens != P->marked; P->tokens = P->tokens->next) {
		Token* tok = P->tokens->token;	
		Token* prev = NULL;
		if (P->tokens->prev) {
			prev = P->tokens->prev->token;
		}
		/* function call? */
		if (prev && prev->type == TOK_IDENTIFIER && tok->type == TOK_OPERATOR && tok->oval == '(') {
			P->tokens = P->tokens->next; /* advance to first argument token */
			/* save mark */
			TokenList* marksave = P->marked;
			mark_operator(P, '(', ')');
			ExpNode* push = malloc(sizeof(ExpNode));
			push->type = EXP_CALL;
			push->parent = NULL;
			push->cval = malloc(sizeof(FuncCall));
			push->cval->fptr = NULL;
			push->cval->arguments = parse_expression(P);
			push->cval->arguments->parent = push;
			push->cval->computed = 0;
			/* revert mark */
			P->marked = marksave;
			ExpNode* top;
			const OpEntry* info = &prec[SPEC_CALL];
			/* TODO !!!get rid of repetition!!! */
			while (1) {
				top = expstack_top(&operators);
				if (!top) break;
				int top_is_unary = top->type == EXP_UNARY;
				int top_is_binary = top->type == EXP_BINARY;
				int top_is_call = top->type == EXP_CALL;
				const OpEntry* top_info;
				if (top_is_unary) {
					/* unary operator is on top of stack */
					top_info = &prec[top->uval->optype];
				} else if (top_is_binary) {
					/* binary operator is on top of stack */
					top_info = &prec[top->bval->optype];
				} else if (top_is_call) {
					top_info = &prec[SPEC_CALL];
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
			expstack_push(&operators, push);
		} else if (tok->type == TOK_OPERATOR) {
			/* use assoc to make sure it exists */
			if (prec[tok->oval].assoc) {
				ExpNode* top;
				const OpEntry* info = &prec[tok->oval];
				while (1) {
					top = expstack_top(&operators);
					if (!top) break;
					int top_is_unary = top->type == EXP_UNARY;
					int top_is_binary = top->type == EXP_BINARY;
					int top_is_call = top->type == EXP_CALL;
					const OpEntry* top_info;
					if (top_is_unary) {
						/* unary operator is on top of stack */
						top_info = &prec[top->uval->optype];
					} else if (top_is_binary) {
						/* binary operator is on top of stack */
						top_info = &prec[top->bval->optype];
					} else if (top_is_call) {
						top_info = &prec[SPEC_CALL];
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
				/* we reached here if an open parenthesis was found but
				 * it is NOT a cast */
				ExpNode* push = malloc(sizeof(ExpNode));
				push->type = EXP_UNARY; /* just consider a parenthesis as a unary operator */
				push->uval = malloc(sizeof(UnaryOp));
				push->uval->optype = '(';
				push->uval->operand = NULL;
				expstack_push(&operators, push);
			} else if (tok->oval == ')') {
				ExpNode* top;
				while (1) {
					top = expstack_top(&operators);
					if (!top) {
						parse_die(P, "unexpected parenthesis ')'");
						break;
					}
					/* pop and push until an open parenthesis is reached */
					if (top->type == EXP_UNARY && top->uval->optype == '(') break;
					expstack_push(&postfix, expstack_pop(&operators));
				}
				expstack_pop(&operators);
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
		} else if (tok->type == TOK_IDENTIFIER) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;
			push->type = EXP_IDENTIFIER;
			push->sval = malloc(strlen(tok->sval) + 1);
			strcpy(push->sval, tok->sval);
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
		at->side = LEAF_NA;
		if (at->type == EXP_INTEGER || at->type == EXP_FLOAT || at->type == EXP_IDENTIFIER) {
			/* just a literal? append to tree */
			expstack_push(&tree, at);	
		} else if (at->type == EXP_CALL) {
			at->cval->fptr = expstack_pop(&tree);
			if (at->cval->fptr->type != EXP_IDENTIFIER) {
				at->cval->computed = 1;
			}
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
			if (at->type == EXP_BINARY) {
				at->bval->left = leaf[1];
				at->bval->right = leaf[0];
				break;
			}
			/* throw the branch back onto the stack */
			expstack_push(&tree, at);
		}
	}

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
	node->blockval->locals = NULL;

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
	fdesc->nargs = 0;
	fdesc->stack_space = 0;

	P->tokens = P->tokens->next; /* skip ( */
	
	while (!on_op(P, ')')) {
		Datatype* data = parse_datatype(P);
		fdesc->nargs++;
		fdesc->stack_space += data->size;
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

		data->fdesc = parse_function_descriptor(P);
	} else {
		/* not function... primitive type or struct (struct not handeled yet) */
		if (on_ident(P, "int")) {
			data->type = DATA_INT;
			data->size = 8;
		} else if (on_ident(P, "float")) {
			data->type = DATA_FLOAT;
			data->size = 8;
		} else if (on_ident(P, "byte")) {
			data->type = DATA_BYTE;
			data->size = 1;
		} else if (on_ident(P, "void")) {
			data->type = DATA_VOID;
			data->size = 0;
		} else {
			if (!is_ident(P)) {
				parse_die(P, "expected typename");
			}
			TreeStruct* str = find_struct(P, P->tokens->token->sval);
			if (!str) {
				parse_die(P, "unknown type '%s'", P->tokens->token->sval);
			}
			data->type = DATA_STRUCT;
			data->sdesc = str;
			data->size = str->desc->size;
		}

		/* overwrite size if it's a pointer */
		if (data->ptr_dim > 0) {
			data->size = 8;
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

	P->tokens = P->tokens->next;

	return decl;
}

static void
parse_struct(ParseState* P) {
	/* starts on token 'struct' */
	/* example struct declaration:
	 *
	 * struct Foo {
	 *   x: int;
	 *   y: ^float;
	 * }
	 */
	
	StructDescriptor* desc;
	TreeStruct* str = malloc(sizeof(TreeStruct));
	str->name = NULL;
	desc = str->desc = malloc(sizeof(StructDescriptor));
	desc->fields = NULL;
	desc->size = 0;
	
	/* skip token 'struct' */ 
	P->tokens = P->tokens->next;	

	/* record name */
	if (!is_ident(P)) {
		parse_die(P, "expected identifier after token 'struct'");
	}
	str->name = malloc(strlen(P->tokens->token->sval) + 1);
	strcpy(str->name, P->tokens->token->sval);
	P->tokens = P->tokens->next;

	eat_operator(P, '{');

	/* get children */
	while (matches_declaration(P)) {
		VarDeclaration* field = parse_declaration(P);
		field->offset = desc->size;
		desc->size += field->datatype->size;
		eat_operator(P, ';');

		VarDeclarationList* append = malloc(sizeof(VarDeclarationList));
		append->decl = field;
		append->next = NULL;
		
		/* append field to field list */
		if (!desc->fields) {
			desc->fields = append;
		} else {
			VarDeclarationList* scan = desc->fields;
			while (scan->next) {
				scan = scan->next;
			}
			scan->next = append;	
		}
	}

	if (!desc->fields) {
		parse_die(P, "struct '%s' must have at least one field", str->name);
	}

	eat_operator(P, '}');
	eat_operator(P, ';');

	/* append the struct to list on knowns */
	TreeStructList* append = malloc(sizeof(TreeStructList));
	append->str = str;
	append->next = NULL;
	if (!P->defined_structs) {
		P->defined_structs = append;
	} else {
		TreeStructList* scan = P->defined_structs;
		while (scan->next) {
			scan = scan->next;
		}
		scan->next = append;
	}

}

static void
parse_statement(ParseState* P) {
	TreeNode* node = malloc(sizeof(TreeNode));
	node->type = NODE_STATEMENT;
	node->stateval = malloc(sizeof(TreeStatement));
	
	/* starts on first token of expression... parse it */
	mark_operator(P, SPEC_NULL, ';');
	node->stateval->exp = parse_expression(P);
	typecheck_expression(P, node->stateval->exp);
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
	typecheck_expression(P, node->ifval->condition);
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
	typecheck_expression(P, node->whileval->condition);
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

ParseState*
generate_syntax_tree(TokenList* tokens) {

	ParseState* P = malloc(sizeof(ParseState));
	P->tokens = tokens;
	P->defined_structs = NULL;
	P->marked = NULL;
	P->root_node = malloc(sizeof(TreeNode));
	P->root_node->type = NODE_BLOCK;
	P->root_node->blockval = malloc(sizeof(TreeBlock));
	P->root_node->blockval->child = NULL;
	P->root_node->blockval->locals = NULL;
	P->root_node->parent = NULL;
	P->root_node->next = NULL;
	P->current_function = NULL;
	P->current_block = P->root_node;
	P->append_target = NULL;
	P->current_offset = 0;

	P->type_int = calloc(1, sizeof(Datatype));
	P->type_int->type = DATA_INT;
	P->type_int->size = 8;
	
	P->type_float = calloc(1, sizeof(Datatype));
	P->type_float->type = DATA_FLOAT;
	P->type_float->size = 8;

	P->type_byte = calloc(1, sizeof(Datatype));
	P->type_byte->type = DATA_BYTE;
	P->type_byte->size = 1;

	while (P->tokens && P->tokens->token) {
		if (on_ident(P, "if")) {
			parse_if(P);
		} else if (on_ident(P, "while")) {
			parse_while(P);
		} else if (on_ident(P, "for")) {
			parse_for(P);
		} else if (on_ident(P, "struct")) {
			parse_struct(P);
		} else if (matches_declaration(P)) {
			VarDeclaration* var = parse_declaration(P);
			var->offset = P->current_offset;
			unsigned int inc = var->datatype->size;
			if (var->datatype->type == DATA_STRUCT && var->datatype->ptr_dim == 0) {
				/* an extra 8 bytes are needed for a struct because it is implemented
				 * on the stack with a pointer */
				inc += 8;
			}
			P->current_offset += inc;
			if (P->current_function) {
				P->current_function->funcval->desc->stack_space += inc;
			}
			if (var->datatype->type == DATA_FPTR && on_op(P, '{')) {

				/* make sure it's in global scope */
				if (P->current_block != P->root_node) {
					parse_die(P, "functions must be declared in the global scope");
				}

				/* if it's an implementation, it can't be array or pointer type */
				if (var->datatype->array_dim > 0) {
					parse_die(P, "an array of functions cannot be implemented");
				}
				if (var->datatype->ptr_dim > 0) {
					parse_die(P, "a function pointer cannot be implemented");
				}

				/* now that we know it's an implementation, wrap it in
				 * a node and append it to the tree */
				TreeNode* node = malloc(sizeof(TreeNode));
				node->parent = NULL;
				node->next = NULL;
				node->prev = NULL;
				node->type = NODE_FUNC_IMPL;
				node->funcval = malloc(sizeof(TreeFunction));
				node->funcval->desc = var->datatype->fdesc;
				node->funcval->name = var->name;
				
				/* no need for declaration anymore, free it.
				 *
				 * **NOTE** name and descriptor not freed */	
				free(var);

				append_node(P, node);
			} else {
				register_local(P, var);
				eat_operator(P, ';');
			}
		} else if (on_op(P, '{')) {
			parse_block(P);
		} else if (on_op(P, '}')) {
			if (P->current_block == P->root_node) {
				parse_die(P, "token '}' doesn't close anything");
			}
			jump_out(P);
		} else {
			parse_statement(P);
		}
	}

	if (P->current_block != P->root_node) {
		parse_die(P, "expected '}' before EOF");
	}
	
	print_debug_info(P);

	/* TODO cleanup tokens + other stuff */

	return P;

}

