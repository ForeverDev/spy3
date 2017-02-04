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
static void parse_else(ParseState*);
static void parse_while(ParseState*);
static void parse_for(ParseState*);
static void parse_block(ParseState*);
static void parse_statement(ParseState*);
static void parse_struct(ParseState*);
static void parse_break(ParseState*);
static void parse_continue(ParseState*);
static void parse_return(ParseState*);
static void parse_do_until(ParseState*);
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
static void print_tree(TreeNode*, int);
static void print_datatype(Datatype*, int);
static void parse_die(ParseState*, const char*, ...);
static void assert_operator(ParseState*, char);
static void eat_op(ParseState*, char);
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
static VarDeclaration* find_field(const TreeStruct*, const char*);
static TreeNode* empty_node(ParseState*);
static void fold_expression(ParseState*, ExpNode*);
static void safe_eat(ParseState*);
static int actual_data_size(Datatype*);

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
	['@']				= {10, ASSOC_RIGHT, OP_UNARY},
	['$']				= {10, ASSOC_RIGHT, OP_UNARY},
	['!']				= {10, ASSOC_RIGHT, OP_UNARY},
	[SPEC_TYPENAME]		= {10, ASSOC_RIGHT, OP_UNARY},
	[SPEC_CAST]			= {10, ASSOC_RIGHT, OP_UNARY},
	['.']				= {11, ASSOC_LEFT, OP_BINARY},
	[SPEC_INC_ONE]		= {11, ASSOC_LEFT, OP_UNARY},
	[SPEC_DEC_ONE]		= {11, ASSOC_LEFT, OP_UNARY},
	[SPEC_CALL]			= {11, ASSOC_LEFT, OP_UNARY},
	[SPEC_INDEX]		= {11, ASSOC_LEFT, OP_UNARY}
};

static void
parse_die(ParseState* P, const char* message, ...) {
	va_list args;
	va_start(args, message);	
	printf("\n\n** SPYRE PARSE ERROR **\n\tmessage: ");
	vprintf(message, args);
	if (P->tokens && P->tokens->token) {
		printf("\n\tline: %d\n", P->tokens->token->line);
	}
	printf("\n\n");
	va_end(args);
	exit(1);
}

static void
safe_eat(ParseState* P) {
	if (!P->tokens) {
		parse_die(P, "unexpected EOF\n");
	}
	P->tokens = P->tokens->next;
}

static int
is_keyword(const char* word) {
	static const char* keywords[] = {
		"if", "while", "for", "do", "struct",
		"return", "continue", "break", "const",
		"static", "foreign", NULL
	};
	for (const char** i = keywords; *i; i++) {
		if (!strcmp(*i, word)) {
			return 1;
		}
	}
	return 0;
}

static int
is_typename(const char* word) {
	static const char* types[] = { 
		"int", "byte", "float", "file", NULL
	};
	for (const char** i = types; *i; i++) {
		if (!strcmp(*i, word)) {
			return 1;
		}
	}
	return 0;
}

static TreeNode*
empty_node(ParseState* P) {
	TreeNode* node = malloc(sizeof(TreeNode));
	node->parent = NULL;
	node->next = NULL;
	node->prev = NULL;
	node->is_else = 0;
	node->line = P->tokens->token->line;
	return node;
}

static void
assert_operator(ParseState* P, char type) {
	if (!P->tokens || P->tokens->token->type != TOK_OPERATOR || P->tokens->token->oval != type) {
		parse_die(P, "expected token (%s), got token (%s)", tokcode_tostring(type), token_tostring(P->tokens->token));
	}
}

/* this is just assert_operator and advance */
static void
eat_op(ParseState* P, char type) {
	assert_operator(P, type);
	safe_eat(P);
}

static int
actual_data_size(Datatype* d) {
	switch (d->type) {
		case DATA_INT:
		case DATA_FLOAT:
		case DATA_FILE:
			return 8;
		case DATA_BYTE:
			return 1;
		case DATA_STRUCT:
			return d->sdesc->desc->size;		
	}
	return 8; /* unknown type? */
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
	/*
	if (node->type == NODE_UNTIL && !P->expect_until) {
		parse_die(P, "unexpected 'until' node");
	} else if (P->expect_until && node->type != NODE_UNTIL) {
		parse_die(P, "expected 'until' node");
	}
	*/
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
			case NODE_DO:
				target->doval->child = node;
				P->expect_until = 1;
				break;
			case NODE_FOR:
				target->forval->child = node;
				break;
			case NODE_FUNC_IMPL:
				/* found function? reset offset */
				P->current_offset = target->funcval->desc->fdesc->arg_space;
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
		|| node->type == NODE_DO
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
		case NODE_CONTINUE:
			printf("CONTINUE\n");
			break;
		case NODE_BREAK:
			printf("BREAK\n");
			break;
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
			for (VarDeclarationList* i = tree->funcval->desc->fdesc->arguments; i; i = i->next) {
				print_datatype(i->decl->datatype, indent + 2);
			}
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent + 1);
			printf("RETURN TYPE: \n");
			print_datatype(tree->funcval->desc->fdesc->return_type, indent + 1);
			INDENT(indent + 1);
			printf("CHILD: [\n");
			print_tree(tree->funcval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_DO:
			printf("DO/UNTIL: [\n");
			INDENT(indent + 1);
			printf("CONDITION: [\n");
			print_expression(tree->doval->condition, indent + 2);
			printf("CHILD: [\n");
			print_tree(tree->doval->child, indent + 2);
			INDENT(indent + 1);
			printf("]\n");
			INDENT(indent);
			printf("]\n");
			break;
		case NODE_RETURN:
			printf("RETURN [\n");
			if (tree->stateval) {
				print_expression(tree->stateval->exp, indent + 1);
			}
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

void
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
		case EXP_STRING:
			printf("\"%s\"\n", exp->sval);
			break;
		case EXP_CALL:
			printf("CALL (computed = %d)\n", exp->cval->computed);
			print_expression(exp->cval->fptr, indent + 1);
			print_expression(exp->cval->arguments, indent + 1);
			break;
		case EXP_INDEX:
			printf("INDEX\n");
			print_expression(exp->aval->array, indent + 1);
			print_expression(exp->aval->index, indent + 1);
			break;
		case EXP_CAST: {
			char* d = tostring_datatype(exp->cxval->d);
			printf("# %s\n", d);
			free(d);	
			print_expression(exp->cxval->operand, indent + 1);
			break;
		}
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
		case DATA_FILE:
			printf("file\n");
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
			for (VarDeclarationList* i = data->fdesc->arguments; i; i = i->next) {
				print_datatype(i->decl->datatype, indent + 2);
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

char*
tostring_datatype(const Datatype* data) {
	char* buf = calloc(1, 1024); /* definitely enough space */
	unsigned int mods = data->mods;
	for (int i = 0; i < data->array_dim; i++) {
		strcat(buf, "[]");
	}
	for (int i = 0; i < data->ptr_dim; i++) {
		strcat(buf, "^");
	}
	if (mods & MOD_STATIC) {
		strcat(buf, "static ");
	}
	if (mods & MOD_CONST) {
		strcat(buf, "const ");
	}
	if (mods & MOD_FOREIGN) {
		strcat(buf, "foreign ");
	}
	switch (data->type) {
		case DATA_INT:
			strcat(buf, "int");
			break;
		case DATA_FILE:
			strcat(buf, "file");
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
			for (VarDeclarationList* i = data->fdesc->arguments; i; i = i->next) {
				char* arg = tostring_datatype(i->decl->datatype);
				sprintf(buf + strlen(buf), "%s", arg);
				free(arg);
				if (i->next) {
					strcat(buf, ", ");
				}
			}
			if (data->fdesc->vararg) {
				if (data->fdesc->nargs > 0) {
					strcat(buf, ", ");
				}
				strcat(buf, "...");
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
	safe_eat(P);	
	if (!on_op(P, ':')) {
		MATCH_FALSE();
	}
	safe_eat(P);
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
		safe_eat(P);
		if (on_op(P, ')')) {
			MATCH_TRUE();
		}
		int result = matches_declaration(P);
		if (result) {
			MATCH_TRUE();
		}
		MATCH_FALSE();
	}

	if (   on_ident(P, "int") 
		|| on_ident(P, "float") 
		|| on_ident(P, "byte") 
		|| on_ident(P, "file")
		|| on_ident(P, "struct")
		|| on_ident(P, "const")
		|| on_ident(P, "static")
		|| on_ident(P, "foreign")) {
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

static FunctionDescriptor*
find_function(ParseState* P, const char* name) {
	for (TreeNode* i = P->root_node->blockval->child; i; i = i->next) {
		if (i->type != NODE_FUNC_IMPL) {
			continue;
		}
		if (!strcmp(i->funcval->name, name)) {
			return i->funcval->desc->fdesc;
		}
	}
	return NULL;
}

static int
is_foreign(ParseState* P, const char* name) {
	for (VarDeclarationList* i = P->root_node->blockval->locals; i; i = i->next) {
		VarDeclaration* var = i->decl;
		if (!strcmp(var->name, name) && var->datatype->type == DATA_FPTR && var->datatype->mods & MOD_FOREIGN) {
			return 1;
		}
	}
	return 0;
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
	if (P->current_function) {
		for (VarDeclarationList* i = P->current_function->funcval->desc->fdesc->arguments; i; i = i->next) {
			if (!strcmp(i->decl->name, name)) {
				return i->decl;
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
	/* TODO check for redeclaration */
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
types_match_strict(const Datatype* a, const Datatype* b) {
	if (a->type != b->type) {
		return 0;
	}
	if (a->array_dim != b->array_dim) {
		return 0;
	}
	if (a->ptr_dim != b->ptr_dim) {
		return 0;
	}
	if (a->type == DATA_FPTR) {
		FunctionDescriptor* da = a->fdesc;
		FunctionDescriptor* db = b->fdesc;
		if (da->nargs != db->nargs) {
			return 0;
		}
		if (!types_match(da->return_type, db->return_type)) {
			return 0;
		}
		VarDeclarationList* aa = da->arguments;
		VarDeclarationList* ab = db->arguments;
		while (aa) {
			if (!types_match(aa->decl->datatype, ab->decl->datatype)) {
				return 0;
			}
			aa = aa->next;
			ab = ab->next;
		}
	}
	return 1;
}

static int
types_match(const Datatype* a, const Datatype* b) {
	if ((IS_INT(a) && IS_PTR(b)) || (IS_INT(b) && IS_PTR(a))) {
		return 1;
	}
	return types_match_strict(a, b);
}

/* helper function for typecheck_expression */
static void
check_function_arg(ParseState* P, Datatype* expected, const Datatype* check, int arg_index, const char* func_id) {
	if (!types_match_strict(expected, check)) {
		if (func_id) {
			parse_die(P,
				"argument #%d to function '%s' should evaluate to (%s), got (%s)",
				arg_index,
				func_id,
				tostring_datatype(expected),
				tostring_datatype(check)
			);	
		} else {
			parse_die(P,
				"argument #%d should evaluate to (%s), got (%s)",
				arg_index,
				tostring_datatype(expected),
				tostring_datatype(check)
			);	
		}
	}
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
		case EXP_STRING:
			return exp->eval = P->type_string;
		case EXP_UNARY:
			switch (exp->uval->optype) {
				case '@': {
					const Datatype* result = typecheck_expression(P, exp->uval->operand);
					/*
					if (IS_LITERAL(result)) {
						parse_die(P, "attempt to take the address of a literal value");
					}
					*/
					Datatype* d = malloc(sizeof(Datatype));
					memcpy(d, result, sizeof(Datatype));
					d->ptr_dim++;
					return exp->eval = d;
				}
				case '$': {
					const Datatype* result = typecheck_expression(P, exp->uval->operand);
					/*
					if (IS_LITERAL(result)) {
						parse_die(P, "attempt to dereference a literal value");
					}
					*/
					if (result->ptr_dim == 0) {
						parse_die(P, "attempt to dereference a non-pointer");
					}
					Datatype* d = malloc(sizeof(Datatype));
					memcpy(d, result, sizeof(Datatype));
					d->ptr_dim--;
					return exp->eval = d;
				}
				case '!':
					typecheck_expression(P, exp->uval->operand);
					return exp->eval = P->type_int;
				default:
					return exp->eval = typecheck_expression(P, exp->uval->operand);
			}
			break;
		case EXP_IDENTIFIER: {
			/* TODO handle parent being '.' */
			VarDeclaration* var = find_local(P, exp->sval);
			if (!var) {
				parse_die(P, "undeclared identifier '%s'", exp->sval);
			}
			return exp->eval = var->datatype;
		}
		case EXP_INDEX: {
			const Datatype* array = typecheck_expression(P, exp->aval->array);
			const Datatype* index = typecheck_expression(P, exp->aval->index);
			if (!IS_INT(index)) {
				parse_die(P, "an array index must evaluate to an integer");
			}
			if (!IS_ARRAY(array) && !IS_PTR(array)) {
				parse_die(P, "attempt to index a non-pointer/array");
			}
			Datatype* ret = malloc(sizeof(Datatype));
			memcpy(ret, array, sizeof(Datatype));
			if (ret->array_dim > 0) { 
				ret->array_dim--;
			} else {
				ret->ptr_dim--;
			}
			/* no longer an address? fill actual size */
			if (!IS_ARRAY(ret) && !IS_PTR(ret)) {
				ret->size = actual_data_size(ret);
			}
			return exp->eval = ret;
		}
		case EXP_CAST: {
			const Datatype* from = typecheck_expression(P, exp->cxval->operand);
			const Datatype* to = exp->cxval->d;
			
			return exp->eval = to;
		}
		case EXP_CALL: {
			FunctionDescriptor* desc = NULL;
			const char* func_id = NULL;
					
			/* we need to find the proper function descriptor and assign it
			 * to desc... we can use the computed flag to determine if the
			 * function is simply a local or if it is a variable/struct member */

			if (exp->cval->computed) {
				const Datatype* lhs = typecheck_expression(P, exp->cval->fptr);
				if (lhs->type != DATA_FPTR) {
					parse_die(P, "attempt to call a non-function (evaluated to '%s')", tostring_datatype(lhs));
				}
				desc = lhs->fdesc;
			} else {
				func_id = exp->cval->fptr->sval;
				desc = find_function(P, func_id);	
				if (!desc) {
					/* not a global function? maybe its a simple id pointer */
					VarDeclaration* var = find_local(P, func_id);
					if (var && var->datatype->type == DATA_FPTR) {
						desc = var->datatype->fdesc;	
					}
				}
				if (!desc) {
					parse_die(P, "function '%s' does not exist", func_id);
				}
			}

			/* call_args = num_of_commas + 1 (unless arguments == NULL, then 0) */
			FuncCall* call = exp->cval;
			ExpNode* arg = call->arguments;
			typecheck_expression(P, arg);
			if (arg) call->nargs++;
			while (arg && arg->type == EXP_BINARY && arg->bval->optype == ',') {
				call->nargs++;
				arg = arg->bval->left;
			}
			
			/* make sure number of args is correct */
			if (desc->vararg) {
				if (call->nargs < desc->nargs) {
					if (func_id) {
						parse_die(P, 
							"vararg function '%s' expected atleast %d arguments, got %d",
							func_id,
							desc->nargs,
							call->nargs
						);
					} else {
						parse_die(P, 
							"vararg function expected atleast %d arguments, got %d",
							desc->nargs,
							call->nargs
						);
					}
				}
			} else if (call->nargs != desc->nargs) {
				if (func_id) {
					parse_die(P,
						"incorrect number of arguments passed to function '%s'; expected %d, got %d",
						func_id,
						desc->nargs,
						call->nargs
					);
				} else {
					parse_die(P,
						"incorrect number of arguments passed to function; expected %d, got %d",
						desc->nargs,
						call->nargs
					);
				}
			}

			/* typecheck each argument */
			if (call->nargs > 0) {
				ExpNode* farthest_comma = NULL;
				arg = call->arguments;
				if (IS_BIN_OP(arg, ',')) {
					farthest_comma = arg;
				}
				while (farthest_comma) {
					ExpNode* lft = farthest_comma->bval->left;
					if (lft && IS_BIN_OP(lft, ',')) {
						farthest_comma = lft;
					} else {
						break;
					}
				}
				ExpNode* checking = farthest_comma ? farthest_comma->bval->left : arg;
				VarDeclarationList* cur_arg = desc->arguments;
				for (int i = 0; i < desc->nargs; i++) {
					check_function_arg(P, cur_arg->decl->datatype, checking->eval, i + 1, func_id);		
					cur_arg = cur_arg->next;
					if (!checking->parent) {
						break;
					}
					if (i == 0) {
						checking = checking->parent->bval->right;
					} else {
						if (!checking->parent->parent) {
							break;
						}
						checking = checking->parent->parent->bval->right;
					}
				}
				
			}

			return exp->eval = desc->return_type;
		}
		case EXP_BINARY: {
			switch (exp->bval->optype) {
				case '.': {
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
					} else {
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
					}
					break;
				}
				default: {
					ExpNode* lhs = exp->bval->left;
					ExpNode* rhs = exp->bval->right;
					const Datatype* left = typecheck_expression(P, lhs);
					const Datatype* right = typecheck_expression(P, rhs);
					const Datatype* ret_type = left;
					if (IS_BIN_OP(exp, ',')) {
						return exp->eval = NULL;
					}
					if (IS_BIN_OP(exp, SPEC_LOG_AND) || IS_BIN_OP(exp, SPEC_LOG_OR)) {
						return exp->eval = P->type_int;
					}
					int left_const = left->mods & MOD_CONST;
					int is_assign = IS_ASSIGN(exp->bval);	
					int p_l = left->ptr_dim > 0;
					int p_r = right->ptr_dim > 0;
					int lf = left->type == DATA_FLOAT && !p_l;
					int li = left->type == DATA_INT && !p_l;
					int lb = left->type == DATA_BYTE && !p_l;
					int rf = right->type == DATA_FLOAT && !p_r;
					int ri = right->type == DATA_INT && !p_r;
					int rb = right->type == DATA_BYTE && !p_r;
					if (exp->bval->optype == '=') {
						if (lb && ri) {
							//parse_die(P, "possible loss of data assigning integer to byte... use an explicit cast if you want do to that");
						}	
					}
					if (is_assign) {
						if (left_const) {
							parse_die(P, "attempt to assign to a const memory address");
						}
						if (IS_LITERAL(lhs)) {
							parse_die(P, "the left side of an assignment operator must not be a literal");
						}
					}
					if ((lf && ri) || (rf && li)) {
						/* if one is a float and one is an int, an implicit cast will be done
						 * and the expression will evaluate to a float */
						 return exp->eval = P->type_float;
					} else if (p_l && ri) {
						/* allow pointer arithmetic */
						return exp->eval = left;
					} else if (p_r && li) {
						/* allow pointer arithmetic */
						return exp->eval = right;
					} else if (rb && li) {
						/* promote byte -> int */
						return exp->eval = left;
					} else if (lb && ri) {
						/* promote byte -> int */
						return exp->eval = right;
					} else if (!types_match(left, right)) {
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

static void
shunting_pops(ExpStack** postfix, ExpStack** operators, const OpEntry* info) {
	ExpNode* top;
	while (1) {
		top = expstack_top(operators);
		if (!top) break;
		int top_is_unary = top->type == EXP_UNARY;
		int top_is_binary = top->type == EXP_BINARY;
		int top_is_call = top->type == EXP_CALL;
		int top_is_cast = top->type == EXP_CAST;
		int top_is_index = top->type == EXP_INDEX;
		const OpEntry* top_info;
		if (top_is_unary) {
			/* unary operator is on top of stack */
			top_info = &prec[top->uval->optype];
		} else if (top_is_binary) {
			/* binary operator is on top of stack */
			top_info = &prec[top->bval->optype];
		} else if (top_is_call) {
			top_info = &prec[SPEC_CALL];
		} else if (top_is_cast) {
			top_info = &prec[SPEC_CAST];
		} else if (top_is_index) {
			top_info = &prec[SPEC_INDEX];
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
		expstack_push(postfix, expstack_pop(operators));
	}
}

/* expects end of exprecsion to be marked... NOT inclusive */
static ExpNode*
parse_expression(ParseState* P) {

	/* first, this function links ExpNodes together via a stack (ExpStack)
	 * next, the stacks are converted into a tree and linked together by the
	 * proper fields in the actual ExpNode object */

	ExpStack* operators = NULL;	
	ExpStack* postfix = NULL;

	/* first, just use shunting yard to organize the tokens */

	for (; P->tokens != P->marked; safe_eat(P)) {
		Token* tok = P->tokens->token;	
		Token* prev = NULL;
		if (P->tokens->prev) {
			prev = P->tokens->prev->token;
		}
		if (tok->type == TOK_OPERATOR && tok->oval == SPEC_SIZEOF) {
			safe_eat(P);
			if (!matches_datatype(P)) {
				parse_die(P, "expected datatype to follow token 'sizeof'");
			}
			Datatype* d = parse_datatype(P);
			ExpNode* push = malloc(sizeof(ExpNode));
			push->type = EXP_INTEGER;
			push->parent = NULL;
			push->ival = d->size; 
			free(d);
			expstack_push(&postfix, push);
		/* function call? (TODO doesnt work in all cases) */
		} else if (prev && tok->type == TOK_OPERATOR && tok->oval == '(' && 
			(
				(prev->type == TOK_OPERATOR && prev->oval == ')') ||
				(prev->type == TOK_IDENTIFIER && !is_keyword(prev->sval) && !is_typename(prev->sval))
			)
		) {
			safe_eat(P); /* advance to first argument token */
			/* save mark */
			TokenList* marksave = P->marked;
			mark_operator(P, '(', ')');
			ExpNode* push = malloc(sizeof(ExpNode));
			push->type = EXP_CALL;
			push->parent = NULL;
			push->cval = malloc(sizeof(FuncCall));
			push->cval->fptr = NULL;
			push->cval->arguments = parse_expression(P);
			push->cval->nargs = 0;
			if (push->cval->arguments) {
				push->cval->arguments->parent = push;
			}
			push->cval->computed = 0;
			/* revert mark */
			P->marked = marksave;
			const OpEntry* info = &prec[SPEC_CALL];
			shunting_pops(&postfix, &operators, info);
			expstack_push(&operators, push);
		} else if (tok->type == TOK_OPERATOR && tok->oval == '[') {
			safe_eat(P); /* advance to first token in index */
			if (on_op(P, ']')) {
				parse_die(P, "expected array index, got token ']'");
			}
			/* TODO implement array indexing */	
			TokenList* marksave = P->marked;
			mark_operator(P, '[', ']');
			ExpNode* push = malloc(sizeof(ExpNode));
			push->type = EXP_INDEX;
			push->parent = NULL;
			push->aval = malloc(sizeof(ArrayIndex));
			push->aval->array = NULL;
			push->aval->index = parse_expression(P);
			P->marked = marksave;
			const OpEntry* info = &prec[SPEC_INDEX];
			shunting_pops(&postfix, &operators, info);
			expstack_push(&operators, push);
		} else if (tok->type == TOK_OPERATOR && tok->oval == '#') {
			safe_eat(P);
			if (!matches_datatype(P)) {
				parse_die(P, "expected datatype to follow token '#'");
			}
			shunting_pops(&postfix, &operators, &prec[SPEC_CAST]);
			ExpNode* push = malloc(sizeof(ExpNode));
			push->type = EXP_CAST;
			push->cxval = malloc(sizeof(Cast));
			push->cxval->d = parse_datatype(P);
			expstack_push(&operators, push);
		} else if (tok->type == TOK_OPERATOR) {
			/* use assoc to make sure it exists */
			if (prec[tok->oval].assoc) {
				const OpEntry* info = &prec[tok->oval];
				shunting_pops(&postfix, &operators, info);
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
		} else if (tok->type == TOK_STRING) {
			ExpNode* push = malloc(sizeof(ExpNode));
			push->parent = NULL;	
			push->type = EXP_STRING;
			push->sval = malloc(strlen(tok->sval) + 1); 
			strcpy(push->sval, tok->sval);
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
		ExpNode* pop = expstack_pop(&operators);
		if (pop->type == EXP_UNARY && (pop->uval->optype == '(' || pop->uval->optype == ')')) {
			parse_die(P, "mismatched parenthesis in expression");
		}
		expstack_push(&postfix, pop);
	}
	
	/* === STAGE TWO ===
	 * convert from postix to a tree
	 */
	
	ExpStack* tree = NULL;

	static const char* malformed = "malfored expression";

	for (ExpStack* i = postfix; i; i = i->next) {
		ExpNode* at = i->node;
		at->side = LEAF_NA;
		if (at->type == EXP_INTEGER || at->type == EXP_FLOAT || at->type == EXP_IDENTIFIER || at->type == EXP_STRING) {
			/* just a literal? append to tree */
			expstack_push(&tree, at);	
		} else if (at->type == EXP_CALL) {
			at->cval->fptr = expstack_pop(&tree);
			at->cval->computed = 1; /* assume computed at first */
			char* name = at->cval->fptr->sval;
			if (at->cval->fptr->type == EXP_IDENTIFIER && (find_function(P, name) || is_foreign(P, name))) {
				at->cval->computed = 0;
			}
			expstack_push(&tree, at);
		} else if (at->type == EXP_INDEX) {
			at->aval->array = expstack_pop(&tree);
			expstack_push(&tree, at);
		} else if (at->type == EXP_UNARY) {
			ExpNode* operand = expstack_pop(&tree);
			if (!operand) {
				parse_die(P, malformed);
			}
			operand->parent = at;
			at->uval->operand = operand;
			expstack_push(&tree, at);
		} else if (at->type == EXP_CAST) {
			ExpNode* operand = expstack_pop(&tree);
			if (!operand) {
				parse_die(P, malformed);
			}
			operand->parent = at;
			at->cxval->operand = operand;
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
				if (j == 1) {
					leaf[j]->side = LEAF_LEFT;
				} else {
					leaf[j]->side = LEAF_RIGHT;
				}
			}
			/* swap order */
			at->bval->left = leaf[1];
			at->bval->right = leaf[0];
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
fold_expression(ParseState* P, ExpNode* exp) {
	if (!exp) return;
	switch (exp->type) {
		case EXP_BINARY: {	
			ExpNode* lhs = exp->bval->left;
			ExpNode* rhs = exp->bval->right;
			int li, lf, ri, rf;
			fold_expression(P, lhs);
			fold_expression(P, rhs);
			/* re-evaluate */
			li = lhs->type == EXP_INTEGER;
			lf = lhs->type == EXP_FLOAT;
			ri = rhs->type == EXP_INTEGER;
			rf = rhs->type == EXP_FLOAT;
			/* only fold if both are literals that are of the same type */
			if (li && ri) {
				spy_int result;
				spy_int l = lhs->ival;
				spy_int r = rhs->ival;
				switch (exp->bval->optype) {
					case '+':
						result = l + r;
						break;
					case '-':
						result = l - r;
						break;
					case '*':
						result = l * r;
						break;
					case '/':
						result = l / r;
						break;
					case SPEC_EQ:
						result = l == r;
						break;
					case SPEC_NEQ:
						result = l != r;
						break;
					case '>':
						result = l > r;
						break;
					case '<':
						result = l < r;
						break;
					case SPEC_GE:
						result = l >= r;
						break;
					case SPEC_LE:
						result = l <= r;
						break;
					case SPEC_LOG_AND:
						result = l && r;
						break;
					case SPEC_LOG_OR:
						result = l || r;
						break;
				}
				/* cleanup children that are not needed anymore */
				free(exp->bval);
				exp->type = EXP_INTEGER;
				exp->ival = result;
				free(lhs);
				free(rhs);
			} else if (lf && rf) {
				exp->type = EXP_FLOAT;
			} else {
				
			}
			break;	
		}
		case EXP_UNARY:
			fold_expression(P, exp->uval->operand);
			break;
		case EXP_CAST:
			fold_expression(P, exp->cxval->operand);
			break;
		case EXP_CALL:
			fold_expression(P, exp->cval->fptr);
			fold_expression(P, exp->cval->arguments);
			break;
		case EXP_INTEGER:
		case EXP_FLOAT:
		case EXP_STRING:
		case EXP_IDENTIFIER:
			return;
	}
}

static void
jump_out(ParseState* P) {
	/* skip } */
	safe_eat(P);
	do {
		P->current_block = P->current_block->parent;
	} while (P->current_block->type != NODE_BLOCK);
}

static void
parse_break(ParseState* P) {
	TreeNode* node = empty_node(P);
	node->type = NODE_BREAK;

	safe_eat(P);
	eat_op(P, ';');

	append_node(P, node);
}

static void
parse_continue(ParseState* P) {
	TreeNode* node = empty_node(P);
	node->type = NODE_CONTINUE;

	safe_eat(P);
	eat_op(P, ';');

	append_node(P, node);
}

static void
parse_return(ParseState* P) {
	TreeNode* node = empty_node(P);
	node->type = NODE_RETURN;
	node->stateval = malloc(sizeof(TreeStatement));

	TreeFunction* func = P->current_function->funcval;
	Datatype* expected_type = func->desc->fdesc->return_type;
	
	safe_eat(P);
	mark_operator(P, SPEC_NULL, ';');
	node->stateval->exp = parse_expression(P);
	typecheck_expression(P, node->stateval->exp);
	if (!types_match(node->stateval->exp->eval, expected_type)) {
		parse_die(P, 
			"function '%s' should return expression of type (%s), got type (%s)",
			func->name,
			tostring_datatype(expected_type),
			tostring_datatype(node->stateval->exp->eval)
		);
	}
	fold_expression(P, node->stateval->exp);
	safe_eat(P); /* skip ; */

	append_node(P, node);
	
}

static void
parse_block(ParseState* P) {
	TreeNode* node = empty_node(P);
	node->type = NODE_BLOCK;
	node->blockval = malloc(sizeof(TreeBlock));
	node->blockval->child = NULL;
	node->blockval->locals = NULL;

	/* skip over { */
	safe_eat(P);

	append_node(P, node);
}

static FunctionDescriptor*
parse_function_descriptor(ParseState* P) {

	/* thing: (a: int, b: int) -> float; */

	FunctionDescriptor* fdesc = malloc(sizeof(FunctionDescriptor));
	fdesc->arguments = NULL;
	fdesc->return_type = NULL;
	fdesc->nargs = 0;
	fdesc->stack_space = 0;
	fdesc->arg_space = 0;
	fdesc->is_global = 0;
	fdesc->vararg = 0;

	safe_eat(P); /* skip ( */

	while (!on_op(P, ')')) {
		if (!matches_declaration(P) && !on_op(P, SPEC_DOTS)) {
			parse_die(P, "expected declaration in argument list");
		}
		if (on_op(P, SPEC_DOTS)) {
			fdesc->vararg = 1;
			safe_eat(P);
			break;
		}
		VarDeclaration* arg = parse_declaration(P);
		arg->offset = fdesc->arg_space;
		fdesc->nargs++;
		fdesc->arg_space += arg->datatype->size;
		if (IS_STRUCT(arg->datatype) && !IS_PTR(arg->datatype)) {
			/* if a struct is an argument it is implicitly a pointer */
			arg->datatype->size = 8;
		}
		if (!fdesc->arguments) {
			fdesc->arguments = malloc(sizeof(VarDeclarationList));
			fdesc->arguments->decl = arg;
			fdesc->arguments->next = NULL;	
		} else {
			VarDeclarationList* scan = fdesc->arguments;
			while (scan->next) {
				scan = scan->next;
			}
			VarDeclarationList* new = malloc(sizeof(VarDeclarationList));
			new->decl = arg;
			new->next = NULL;
			scan->next = new;
		}
		if (!on_op(P, ')')) {
			eat_op(P, ',');
			if (on_op(P, ')')) {
				parse_die(P, "expected datatype after ','");
			}
		}
	}

	eat_op(P, ')');
	eat_op(P, SPEC_ARROW);
	fdesc->return_type = parse_datatype(P);

	return fdesc;
	
}

static uint32_t
parse_modifiers(ParseState* P) {

	uint32_t mod = 0;

	while (1) {
		if (P->tokens->token->type != TOK_IDENTIFIER) {
			break;
		}
		char* id = P->tokens->token->sval;
		if (!strcmp(id, "static")) {
			mod |= MOD_STATIC;
		} else if (!strcmp(id, "const")) {
			mod |= MOD_CONST;
		} else if (!strcmp(id, "foreign")) {
			mod |= MOD_FOREIGN;
		} else {
			break;
		}
		safe_eat(P);
	}
	
	return mod;

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
	data->mods = parse_modifiers(P);

	/* is array type */
	/* TODO handle more than one array */
	while (on_op(P, '[')) {
		safe_eat(P);
		if (on_op(P, ']')) {
			parse_die(P, "expected size of array to follow token '['");
		}
		mark_operator(P, '[', ']');
		ExpNode* size = parse_expression(P);
		typecheck_expression(P, size);
		fold_expression(P, size);
		if (!IS_CT_CONSTANT(size)) {
			parse_die(P, "the size of an array must evaluate to a compile-time constant");
		}
		data->array_dim++;
		data->array_size = realloc(data->array_size, data->array_dim * sizeof(unsigned int));
		data->array_size[data->array_dim - 1] = size->ival;
		safe_eat(P);
	}

	/* is pointer type */
	while (on_op(P, '^')) {
		data->ptr_dim++;
		safe_eat(P);
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
		data->size = 8;
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
		} else if (on_ident(P, "file")) {
			data->type = DATA_FILE;
			data->size = 8;
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

	/* make sure modifiers aren't used illegally */
	if (data->type == DATA_FPTR) {
		if (data->mods & MOD_STATIC) {
			parse_die(P, "the modifier 'static' cannot be applied to functions");
		}
		if (data->mods & MOD_CONST) {
			parse_die(P, "the modifier 'const' cannot be applied to functions");
		}
	} else {
		if (data->mods & MOD_FOREIGN) {
			parse_die(P, "the modifier 'foreign' can only be applied to functions");
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
	safe_eat(P);

	/* skip ':' */
	safe_eat(P);

	/* read datatype */
	decl->datatype = parse_datatype(P);

	safe_eat(P);

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
	safe_eat(P);	

	/* record name */
	if (!is_ident(P)) {
		parse_die(P, "expected identifier after token 'struct'");
	}
	str->name = malloc(strlen(P->tokens->token->sval) + 1);
	strcpy(str->name, P->tokens->token->sval);
	safe_eat(P);

	eat_op(P, '{');

	/* get children */
	while (matches_declaration(P)) {
		VarDeclaration* field = parse_declaration(P);
		field->offset = desc->size;
		desc->size += field->datatype->size;
		eat_op(P, ';');

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

	eat_op(P, '}');
	eat_op(P, ';');

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
	TreeNode* node = empty_node(P);
	node->type = NODE_STATEMENT;
	node->stateval = malloc(sizeof(TreeStatement));
	
	/* starts on first token of expression... parse it */
	mark_operator(P, SPEC_NULL, ';');
	node->stateval->exp = parse_expression(P);
	typecheck_expression(P, node->stateval->exp);
	fold_expression(P, node->stateval->exp);
	safe_eat(P); /* skip ; */

	append_node(P, node);
}

static void 
parse_if(ParseState* P) {
	TreeNode* node = empty_node(P);
	node->type = NODE_IF;
	node->ifval = malloc(sizeof(TreeIf));
	node->ifval->child = NULL;
	node->ifval->has_else = 0;

	/* starts on token IF */
	safe_eat(P); /* skip IF */
	eat_op(P, '(');
	mark_operator(P, '(', ')');
	node->ifval->condition = parse_expression(P);
	typecheck_expression(P, node->ifval->condition);
	fold_expression(P, node->ifval->condition);
	safe_eat(P); /* skip ')' */

	append_node(P, node);

}

static void
parse_do(ParseState* P) {
	TreeNode* node = empty_node(P);
	node->type = NODE_DO;
	node->doval = malloc(sizeof(TreeDoUntil));
	node->doval->child = NULL;
	
	safe_eat(P); /* skip DO */
	
	/* just append the node, the rest will be dealt with when children are read */	
	append_node(P, node);
}

static void
parse_else(ParseState* P) {

}

static void 
parse_while(ParseState* P) {
	TreeNode* node = empty_node(P);
	node->type = NODE_WHILE;
	node->whileval = malloc(sizeof(TreeWhile));
	node->whileval->child = NULL;

	/* starts on token WHILE */
	safe_eat(P); /* skip WHILE */
	eat_op(P, '(');
	mark_operator(P, '(', ')');
	node->whileval->condition = parse_expression(P);
	typecheck_expression(P, node->whileval->condition);
	fold_expression(P, node->whileval->condition);
	safe_eat(P); /* skip ')' */

	append_node(P, node);

}

static void
parse_for(ParseState* P) {
	TreeNode* node = empty_node(P);
	node->type = NODE_FOR;
	node->forval = malloc(sizeof(TreeFor));
	node->forval->child = NULL;
	
	/* starts on token FOR */
	safe_eat(P); /* skip FOR */
	eat_op(P, '(');
	mark_operator(P, SPEC_NULL, ';');
	node->forval->init = parse_expression(P);
	typecheck_expression(P, node->forval->init);
	fold_expression(P, node->forval->init);
	safe_eat(P); /* skip ; */
	mark_operator(P, SPEC_NULL, ';');
	node->forval->condition = parse_expression(P);
	typecheck_expression(P, node->forval->condition);
	fold_expression(P, node->forval->condition);
	safe_eat(P); /* skip ; */
	mark_operator(P, '(', ')');
	node->forval->statement = parse_expression(P);
	typecheck_expression(P, node->forval->statement);
	fold_expression(P, node->forval->statement);
	safe_eat(P); /* skip ) */

	append_node(P, node);
}

ParseState*
generate_syntax_tree(TokenList* tokens) {

	ParseState* P = malloc(sizeof(ParseState));
	P->tokens = tokens;
	P->defined_structs = NULL;
	P->marked = NULL;
	P->root_node = empty_node(P);
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
	P->expect_until = 0;

	P->type_int = malloc(sizeof(Datatype));
	P->type_int->type = DATA_INT;
	P->type_int->ptr_dim = 0;
	P->type_int->array_dim = 0;
	P->type_int->size = 8;

	P->type_file = malloc(sizeof(Datatype));
	P->type_file->type = DATA_FILE;
	P->type_file->ptr_dim = 0;
	P->type_file->array_dim = 0;
	P->type_file->size = 8;
	
	P->type_float = malloc(sizeof(Datatype));
	P->type_float->type = DATA_FLOAT;
	P->type_float->ptr_dim = 0;
	P->type_float->array_dim = 0;
	P->type_float->size = 8;

	P->type_byte = malloc(sizeof(Datatype));
	P->type_byte->ptr_dim = 0;
	P->type_byte->array_dim = 0;
	P->type_byte->type = DATA_BYTE;
	P->type_byte->size = 1;

	P->type_string = malloc(sizeof(Datatype));
	P->type_string->type = DATA_BYTE;
	P->type_string->ptr_dim = 1;
	P->type_string->array_dim = 0;
	P->type_string->size = 1;

	while (P->tokens && P->tokens->token) {
		if (on_ident(P, "if")) {
			parse_if(P);
		} else if (on_ident(P, "do")) {
			parse_do(P);
		} else if (on_ident(P, "until")) {
			TreeNode* last = P->current_block->blockval->child;
			while (last->next) {
				last = last->next;
			}
			if (last->type != NODE_DO) {
				parse_die(P, "an 'until' node can only follow a 'do' node");
			}
			safe_eat(P); /* skip UNTIL */
			eat_op(P, '(');
			mark_operator(P, '(', ')');
			last->doval->condition = parse_expression(P);
			typecheck_expression(P, last->doval->condition);
			fold_expression(P, last->doval->condition);
			safe_eat(P);
			eat_op(P, ';');
		} else if (on_ident(P, "else")) {
			parse_else(P);
		} else if (on_ident(P, "while")) {
			parse_while(P);
		} else if (on_ident(P, "for")) {
			parse_for(P);
		} else if (on_ident(P, "struct")) {
			parse_struct(P);
		} else if (on_ident(P, "break")) {
			parse_break(P);
		} else if (on_ident(P, "continue")) {
			parse_continue(P);
		} else if (on_ident(P, "return")) {
			parse_return(P);
		} else if (matches_declaration(P)) {
			VarDeclaration* var = parse_declaration(P);
			var->offset = P->current_offset;
			unsigned int inc = var->datatype->size;
			if (var->datatype->type == DATA_STRUCT && var->datatype->ptr_dim == 0) {
				/* an extra 8 bytes are needed for a struct because it is implemented
				 * on the stack with a pointer */
				inc += 8;
			}
			if (var->datatype->array_dim > 0) {
				int size = 1;
				for (int i = 0; i < var->datatype->array_dim; i++) {
					size *= var->datatype->array_size[i];
				}
				inc = size*inc;
			}
			if (var->datatype->type == DATA_FPTR) {
				if (var->datatype->mods & MOD_FOREIGN && P->current_block != P->root_node) {
					parse_die(P, "foreigntions must be declared in the global scope");
				}	
			} else {
				if (!var->datatype->mods & MOD_STATIC && P->current_block == P->root_node) {
					parse_die(P, "variables declared in the global scope must be declared static");
				}
			}
			if (var->datatype->type == DATA_FPTR && on_op(P, '{')) {

				var->datatype->fdesc->is_global = 1;

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
				if (var->datatype->mods & MOD_FOREIGN) {
					parse_die(P, "a function with the modifier 'foreign' cannot be implemented");
				}

				/* now that we know it's an implementation, wrap it in
				 * a node and append it to the tree */
				TreeNode* node = empty_node(P);
				node->parent = NULL;
				node->next = NULL;
				node->prev = NULL;
				node->type = NODE_FUNC_IMPL;
				node->funcval = malloc(sizeof(TreeFunction));
				node->funcval->desc = var->datatype;
				node->funcval->name = var->name;

				/* also append it as a var so that it can be referenced */
				register_local(P, var);

				append_node(P, node);


			} else {
				register_local(P, var);
				eat_op(P, ';');
				P->current_offset += inc;
				if (P->current_function) {
					P->current_function->funcval->desc->fdesc->stack_space += inc;
				}
			}

			/* TODO now that the declaration has been parsed, if on token '=', assign
			 * to the newly declared variable.  Throw an error if the token is not '=' or ';' */
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

	/* make sure there is an entry point */
	if (!find_function(P, "main")) {
		parse_die(P, "function 'main' not found");
	}

	//print_debug_info(P);

	/* TODO cleanup tokens + other stuff */

	return P;

}

