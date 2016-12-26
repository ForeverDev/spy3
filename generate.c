#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "generate.h"

#define FORMAT_LABEL "__L%04d__"
#define FORMAT_LOC_LABEL "." FORMAT_LABEL ":\n"
#define FORMAT_DEF_LABEL FORMAT_LABEL ":\n"
#define FORMAT_DEF_LOC_LABEL FORMAT_LOC_LABEL "\n"

typedef struct CompileState CompileState;
typedef struct InstructionStack InstructionStack;
typedef struct LiteralList LiteralList;

struct CompileState {
	TreeNode* focus;
	TreeNode* root_node;
	InstructionStack* ins_stack;
	unsigned int label_count;
	FILE* handle;
};

struct LiteralList {
	char* literal;
	LiteralList* next;
};

struct InstructionStack {
	LiteralList* ins;
	TreeNode* correspond;
	InstructionStack* next;
	InstructionStack* prev;
};

/* generate functions */
static void generate_expression(CompileState*, ExpNode*);
static void generate_if(CompileState*);

/* misc function */
static int advance(CompileState*);
static TreeNode* get_child(TreeNode*);
static VarDeclaration* get_local(CompileState*, const char*);

/* writer functions */
static void writeb(CompileState*, const char*, ...);
static void pushb(CompileState*, const char*, ...);
static void popb(CompileState*);

static void
writeb(CompileState* C, const char* format, ...) {
	va_list list;
	va_start(list, format);
	vfprintf(C->handle, format, list);
	va_end(list);
}

static void
pushb(CompileState* C, const char* format, ...) {

	/* setup format list */
	va_list arg_list;
	va_start(arg_list, format);

	/* get a pointer to the last object on the stack... we are
	 * either going to append a literal to it, or append a whole
	 * new object to the stack */
	InstructionStack* list = C->ins_stack;
	while (list && list->next) {
		list = list->next;
	}
	/* regardless of what happens, we will need a LiteralList */
	LiteralList* lit_list = malloc(sizeof(LiteralList));
	lit_list->next = NULL;
	lit_list->literal = malloc(512); /* 512 bytes is EASILY enough */
	vsprintf(lit_list->literal, format, arg_list);
	if (!list || list->correspond != C->focus) {
		/* ... append a whole new object ... */
		InstructionStack* new = malloc(sizeof(InstructionStack));
		new->correspond = C->focus;
		new->ins = lit_list;
		new->next = NULL;
		new->prev = NULL;
		if (C->ins_stack) {
			/* if a stack exists, just append */
			list->next = new;
			new->prev = list;
		} else {
			/* otherwise just assign to stack */
			C->ins_stack = new;
		}
	} else {
		/* just append a new instruction */
		LiteralList* lit_tail = list->ins;
		while (lit_tail->next) {
			lit_tail = lit_tail->next;
		}
		lit_tail->next = lit_list;
	}

	va_end(arg_list);
}

static void
popb(CompileState* C) {
	if (!C->ins_stack) {
		return;
	}
	InstructionStack* tail = C->ins_stack;
	while (tail->next) {
		tail = tail->next;
	}
	
	/* detach the tail, (pop it off) */
	if (tail->prev) {
		tail->prev->next = NULL;
	} else {
		C->ins_stack = NULL;
	}

	/* now write the instructions */
	LiteralList* i = tail->ins;
	while (i) {
		/* write the instruction */
		writeb(C, i->literal);	

		/* cleanup and move to next */	
		LiteralList* to_free = i;
		i = i->next;
		free(to_free->literal);
		free(to_free);
	}

	free(tail);
}

static VarDeclaration*
get_local(CompileState* C, const char* identifier) {
	for (TreeNode* i = C->focus; i; i = i->parent) {
		if (i->type != NODE_BLOCK) {
			continue;
		}
		for (VarDeclarationList* j = C->focus->blockval->locals; j; j = j->next) {
			if (!strcmp(j->decl->name, identifier)) {
				return j->decl;
			}
		}
	}
	return NULL;
}

static TreeNode*
get_child(TreeNode* node) {
	switch (node->type) {
		case NODE_IF:
			return node->ifval->child;
		case NODE_WHILE:
			return node->whileval->child;
		case NODE_FOR:
			return node->forval->child;
		case NODE_FUNC_IMPL:
			return node->funcval->child;
		case NODE_BLOCK:
			return node->blockval->child;
		default:
			return NULL;
	}
}

/* return 1 if advance success */
static int
advance(CompileState* C) {
	TreeNode* focus = C->focus;
	TreeNode* child;
	/* child? jump into it */
	if ((child = get_child(focus))) {
		C->focus = child;
		return 1;
	}
	/* next thing in block? jump to it */
	if (focus->next) {
		C->focus = focus->next;
		return 1;
	}
	/* parent->next? */
	C->focus = focus->parent;
	while (C->focus) {
		if (C->focus->next) {
			C->focus = C->focus->next;
			return 1;
		}
		popb(C);
		C->focus = C->focus->parent;
	}
	return 0;
}

static void
generate_expression(CompileState* C, ExpNode* exp) {
	int is_top = exp->parent == NULL;
	switch (exp->type) {
		case EXP_INTEGER:
			writeb(C, "iconst %d", exp->ival);
			break;
		case EXP_FLOAT:
			writeb(C, "fconst %f", exp->fval);
			break;
		case EXP_IDENTIFIER: {
			int is_object = (
				exp->side == LEAF_LEFT &&
				exp->parent &&
				exp->parent->type == EXP_BINARY &&
				exp->parent->bval->optype == '.'
			);	
			VarDeclaration* var = get_local(C, exp->sval);
			break;
		}
		case EXP_BINARY:
			generate_expression(C, exp->bval->left);
			generate_expression(C, exp->bval->right);
			char prefix = exp->eval->type == DATA_FLOAT ? 'f' : 'i';
			switch (exp->bval->optype) {
				case '+':
					writeb(C, "%cadd", prefix);	
					break;
				case '-':
					writeb(C, "%csub", prefix);	
					break;
				case '*':
					writeb(C, "%cmul", prefix);	
					break;
				case '/':
					writeb(C, "%cdiv", prefix);	
					break;

				/* TODO change how comparison operators work... right now,
				 * icmp is used and the result is pushed onto the stack.  In
				 * the future, the push flag instruction should be removed in
				 * most cases */
				case '>':
					writeb(C, "%ccmp\npgt", prefix);
					break;
				case '<':
					writeb(C, "%ccmp\nplt", prefix);
					break;
				case SPEC_GE:
					writeb(C, "%ccmp\npge", prefix);
					break;
				case SPEC_LE:
					writeb(C, "%ccmp\nple", prefix);
					break;
				case SPEC_EQ:
					writeb(C, "%ccmp\npe", prefix);
					break;
				case SPEC_NEQ:
					writeb(C, "%ccmp\npne", prefix);
					break;
			}
			break;
		case EXP_UNARY:
			generate_expression(C, exp->uval->operand);
			break;
		default:
			return;	
	}
	writeb(C, "\n");
}

static void
generate_if(CompileState* C) {
	unsigned int test_neg = C->label_count++;
	generate_expression(C, C->focus->ifval->condition);
	writeb(C, "jz " FORMAT_LABEL "\n", test_neg);
	pushb(C, FORMAT_DEF_LABEL, test_neg);	
}

void
generate_instructions(ParseState* P, const char* outfile_name) {

	CompileState C;

	C.handle = fopen(outfile_name, "wb");
	if (!C.handle) {
		printf("couldn't open '%s' for writing", outfile_name);
		exit(1);
	}

	C.root_node = P->root_node;
	C.focus = C.root_node;
	C.ins_stack = NULL;
	C.label_count = 0;

	if (!C.root_node) {
		return;
	}
	
	do {
		switch (C.focus->type) {
			case NODE_IF:
				generate_if(&C);
				break;	
			case NODE_BLOCK:
				break;
		}
	} while (advance(&C));

	while (C.ins_stack) {
		popb(&C);
	}

	writeb(&C, "exit\n");

	fclose(C.handle);

}
