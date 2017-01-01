#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "generate.h"

#define FORMAT_LABEL "__L%04d__"
#define FORMAT_FUNC  "__F%s__"

#define FORMAT_DEF_FUNC FORMAT_FUNC ":\n"
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
	TreeStructList* defined_structs;
	unsigned int label_count;
	unsigned int return_label;
	unsigned int cont_label;
	unsigned int break_label;
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
static void generate_function(CompileState*);
static void initialize_local(CompileState*, VarDeclaration*);

/* misc function */
static int advance(CompileState*);
static TreeNode* get_child(TreeNode*);
static VarDeclaration* get_local(CompileState*, const char*);
static TreeStruct* get_struct(CompileState*, const char*);
static VarDeclaration* get_field(const TreeStruct*, const char*);
static char get_prefix(const Datatype*);

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

static char
get_prefix(const Datatype* data) {
	return (data->type == DATA_FLOAT && data->ptr_dim == 0) ? 'f' : 'i';
}

static TreeStruct*
get_struct(CompileState* C, const char* identifier) {
	for (TreeStructList* i = C->defined_structs; i; i = i->next) {
		if (!strcmp(i->str->name, identifier)) {
			return i->str;
		}
	}
	return NULL;
}

static VarDeclaration*
get_field(const TreeStruct* str, const char* name) {
	for (VarDeclarationList* i = str->desc->fields; i; i = i->next) {
		if (!strcmp(i->decl->name, name)) {
			return i->decl;
		}
	}
	return NULL;
}

static VarDeclaration*
get_local(CompileState* C, const char* identifier) {
	for (TreeNode* i = C->focus; i; i = i->parent) {
		if (i->type != NODE_BLOCK) {
			continue;
		}
		for (VarDeclarationList* j = i->blockval->locals; j; j = j->next) {
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
	/* if focus is a block without children, pop once */
	if (focus->type == NODE_BLOCK && !focus->blockval->child) {
		popb(C);
	}
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

/* t is a bval node */
#define IS_ASSIGN(t) ((t)->optype == '=' || \
					  (t)->optype == SPEC_INC_BY || \
					  (t)->optype == SPEC_DEC_BY || \
					  (t)->optype == SPEC_MUL_BY || \
					  (t)->optype == SPEC_DIV_BY || \
					  (t)->optype == SPEC_MOD_BY || \
					  (t)->optype == SPEC_SHL_BY || \
					  (t)->optype == SPEC_SHR_BY || \
					  (t)->optype == SPEC_AND_BY || \
					  (t)->optype == SPEC_OR_BY || \
					  (t)->optype == SPEC_XOR_BY)

static void
generate_expression(CompileState* C, ExpNode* exp) {
	if (!exp) return;
	int is_top = exp->parent == NULL;
	int is_assign = exp->parent ? IS_ASSIGN(exp->parent->bval) : 0;
	int dont_der = (
		exp->parent &&
		exp->parent->type == EXP_BINARY &&
		IS_ASSIGN(exp->parent->bval) &&
		exp->side == LEAF_LEFT
	);
	/* don't dereference dot chain on left side of assignment operator */
	if (exp->parent && exp->parent->type == EXP_BINARY && exp->parent->bval->optype == '.' && exp->side == LEAF_LEFT) {
		ExpNode* scan = exp->parent;
		while (scan && scan->type == EXP_BINARY && scan->bval->optype == '.') {
			scan = scan->parent;
		}
		if (scan && scan->type == EXP_BINARY && IS_ASSIGN(scan->bval)) {
			dont_der = 1;
		}
	}
	switch (exp->type) {
		case EXP_INTEGER:
			writeb(C, "iconst %d\n", exp->ival);
			break;
		case EXP_FLOAT:
			writeb(C, "fconst %f\n", exp->fval);
			break;
		case EXP_IDENTIFIER: {
			/* NOTE generate_expression will __never__ be called on an
			 * identifier that is not a variable... therefore it is safe
			 * to assume that the get_local result is non-null */
			VarDeclaration* var = get_local(C, exp->sval);

			/* TODO @ERR get_local doesn not properly return function declarations */
			
			if (var->datatype->type == DATA_FPTR && var->datatype->fdesc->is_global) {
				writeb(C, "iconst " FORMAT_FUNC "\n", var->name);
			} else if (dont_der && var->datatype->type != DATA_STRUCT) {
				/* structs are pointers, use locall */
				writeb(C, "lea %d\n", var->offset);
			} else {
				char prefix = get_prefix(var->datatype);
				writeb(C, "%clocall %d\n", prefix, var->offset);
			}
			break;
		}
		case EXP_CALL: {
			FuncCall* call = exp->cval;
			if (call->computed) {
				generate_expression(C, call->fptr);
			}
			generate_expression(C, call->arguments);
			if (call->computed) {
				writeb(C, "ccall %d\n", call->num_args);
			} else {
				writeb(C, "call %s, %d\n", call->fptr->sval, call->num_args);
			}
			break;
		}
		case EXP_BINARY: {
			ExpNode* lhs = exp->bval->left;
			ExpNode* rhs = exp->bval->right;
			if (exp->bval->optype == '.') {
				int is_bottom = lhs->type == EXP_IDENTIFIER;
				generate_expression(C, exp->bval->left);				
				if (is_bottom) {
					VarDeclaration* obj = get_local(C, lhs->sval);
					VarDeclaration* field = get_field(obj->datatype->sdesc, rhs->sval);
					/* call to generate made lea, now add offset */
					writeb(C, "iinc %d\n", field->offset);	
				} else {
					/* if it's a chain (not at the bottom), grab the field from the eval
					 * instead of finding the struct from its identifier */
					VarDeclaration* field = get_field(lhs->eval->sdesc, rhs->sval);
					writeb(C, "iinc %d\n", field->offset);	
				}
				if (!is_assign) {
					writeb(C, "%cder\n", get_prefix(exp->eval));	
				}
			} else if (exp->bval->optype == '=') {
				generate_expression(C, lhs);
				generate_expression(C, rhs);
				if (!is_top) {
					writeb(C, "dup\n");
				}
				writeb(C, "%csave\n", get_prefix(lhs->eval)); 
			} else if (IS_ASSIGN(exp->bval)) {
				char lp = get_prefix(lhs->eval);
				generate_expression(C, lhs);
				writeb(C, "dup\n");
				writeb(C, "%cder\n", lp);
				generate_expression(C, rhs);
				switch (exp->bval->optype) {
					case SPEC_INC_BY:
						writeb(C, "%cadd\n", lp);
						break;
					case SPEC_DEC_BY:
						writeb(C, "%csub\n", lp);
						break;
					case SPEC_MUL_BY:
						writeb(C, "%cmul\n", lp);
						break;
					case SPEC_DIV_BY:
						writeb(C, "%cdiv\n", lp);
						break;
					case SPEC_MOD_BY:
						writeb(C, "mod\n");
						break;
					case SPEC_SHL_BY:
						writeb(C, "shl\n");
						break;
					case SPEC_SHR_BY:
						writeb(C, "shr\n");
						break;
					case SPEC_AND_BY:
						writeb(C, "and\n");
						break;
					case SPEC_OR_BY:
						writeb(C, "or\n");
						break;
					case SPEC_XOR_BY:
						writeb(C, "xor\n");
						break;

				}
				if (!is_top) {
					writeb(C, "dup\n");
				}
				writeb(C, "%csave\n", lp);
			} else { 
				generate_expression(C, lhs);
				generate_expression(C, rhs);
				char prefix = get_prefix(exp->eval);
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
					
					/* TODO implement top-level jump for comparison operators */
					case '>':
						writeb(C, "%ccmp\npgt\n%ctest", prefix, prefix);
						break;
					case '<':
						writeb(C, "%ccmp\nplt\n%ctest", prefix, prefix);
						break;
					case SPEC_GE:
						writeb(C, "%ccmp\npge\n%ctest", prefix, prefix);
						break;
					case SPEC_LE:
						writeb(C, "%ccmp\nple\n%ctest", prefix, prefix);
						break;
					case SPEC_EQ:
						writeb(C, "%ccmp\npe\n%ctest", prefix, prefix);
						break;
					case SPEC_NEQ:
						writeb(C, "%ccmp\npne\n%ctest", prefix, prefix);
						break;
				}
				writeb(C, "\n");
			}
			break;
		}
		case EXP_UNARY:
			generate_expression(C, exp->uval->operand);
			break;
		default:
			return;	
	}
}

static void
generate_break(CompileState* C) {
	writeb(C, "jmp " FORMAT_LABEL "\n", C->break_label);
}

static void
generate_continue(CompileState* C) {
	writeb(C, "jmp " FORMAT_LABEL "\n", C->cont_label);
}

static void
generate_if(CompileState* C) {
	unsigned int test_neg = C->label_count++;
	generate_expression(C, C->focus->ifval->condition);
	writeb(C, "jz " FORMAT_LABEL "\n", test_neg);
	pushb(C, FORMAT_DEF_LABEL, test_neg);	
}

static void
generate_while(CompileState* C) {
	unsigned int top = C->label_count++;
	unsigned int test_neg = C->label_count++;
	C->cont_label = top;
	C->break_label = test_neg;
	writeb(C, FORMAT_DEF_LABEL, top);
	generate_expression(C, C->focus->whileval->condition);
	writeb(C, "jz " FORMAT_LABEL "\n", test_neg);
	pushb(C, "jmp " FORMAT_LABEL "\n", top);
	pushb(C, FORMAT_DEF_LABEL, test_neg);
}

static void
generate_function(CompileState* C) {
	C->return_label = C->label_count++;
	TreeFunction* func = C->focus->funcval;
	FunctionDescriptor* desc = func->desc;
	writeb(C, FORMAT_DEF_FUNC, func->name);	
	writeb(C, "res %d\n", desc->stack_space);
	int index = 0;
	for (DatatypeList* i = desc->arguments; i; i = i->next) {
		writeb(C, "%carg %d\n", get_prefix(i->data), index++);
	}
	pushb(C, FORMAT_DEF_LABEL, C->return_label);
	pushb(C, "vret\n");
}

static void
initialize_local(CompileState* C, VarDeclaration* var) {
	int is_ptr = var->datatype->ptr_dim > 0;
	writeb(C, "; initialize '%s'\n", var->name);
	if (var->datatype->type == DATA_STRUCT && !is_ptr) {
		/* if it's a struct, initialize it as a pointer to stack space */
		writeb(C, "lea %d\n", var->offset + 8);
	} else {
		writeb(C, "iconst 0\n");
	}
	writeb(C, "ilocals %d\n\n", var->offset);
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
	C.defined_structs = P->defined_structs;
	C.focus = C.root_node;
	C.ins_stack = NULL;
	C.label_count = 0;
	C.cont_label = 0;
	C.break_label = 0;

	if (!C.root_node) {
		fclose(C.handle);
		return;
	}

	writeb(&C, "jmp __ENTRY__\n");
	
	do {
		switch (C.focus->type) {
			case NODE_IF:
				generate_if(&C);
				break;	
			case NODE_WHILE:
				generate_while(&C);
				break;
			case NODE_FUNC_IMPL:
				generate_function(&C);
				break;
			case NODE_STATEMENT:
				generate_expression(&C, C.focus->stateval->exp);
				break;
			case NODE_BREAK:
				generate_break(&C);
				break;
			case NODE_CONTINUE:
				generate_continue(&C);
				break;
			case NODE_BLOCK:
				for (VarDeclarationList* i = C.focus->blockval->locals; i; i = i->next) {
					initialize_local(&C, i->decl);
				}
				break;
		}
	} while (advance(&C));

	while (C.ins_stack) {
		popb(&C);
	}
	
	writeb(&C, "__ENTRY__:\n");	
	writeb(&C, "call __Fmain__, 0\n");
	writeb(&C, "exit\n");

	fclose(C.handle);

}
