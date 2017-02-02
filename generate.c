#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "generate.h"
#include "vm.h"

#define FORMAT_LABEL ".L%d"
#define FORMAT_STATIC ".S%d"
#define FORMAT_FUNC  "%s"

#define FORMAT_DEF_FUNC FORMAT_FUNC ":\n"
#define FORMAT_DEF_STATIC FORMAT_STATIC ": db \"%s\\0\"\n"
#define FORMAT_LOC_LABEL "." FORMAT_LABEL ":\n"
#define FORMAT_DEF_LABEL FORMAT_LABEL ":\n"
#define FORMAT_DEF_LOC_LABEL FORMAT_LOC_LABEL "\n"

typedef struct CompileState CompileState;
typedef struct InstructionStack InstructionStack;
typedef struct LiteralList LiteralList;

struct CompileState {
	TreeNode* focus;
	TreeNode* root_node;
	TreeNode* current_function;
	InstructionStack* ins_stack;
	TreeStructList* defined_structs;
	LiteralList* string_list;
	unsigned int static_count;
	unsigned int label_count;
	unsigned int return_label;
	unsigned int cont_label;
	unsigned int break_label;
	unsigned int bottom_label;
	int exp_push; /* which writer function for generate_expression to use */
	int cond_jmp; /* if 1, generate_expression will jump to bottom_label with a top-level comparison */
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
static void generate_functtion(CompileState*);
static void generate_while(CompileState*);
static void generate_for(CompileState*);
static void generate_condition(CompileState*, ExpNode*);
static void generate_continue(CompileState*);
static void generate_break(CompileState*);
static void generate_return(CompileState*);
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
	writeb(C, "; ins pop\n; ----------\n");
	while (i) {
		/* write the instruction */
		writeb(C, i->literal);	

		/* cleanup and move to next */	
		LiteralList* to_free = i;
		i = i->next;
		free(to_free->literal);
		free(to_free);
	}
	writeb(C, "; ----------\n");

	free(tail);
}

static char
get_prefix(const Datatype* data) {
	return (data->type == DATA_FLOAT && data->ptr_dim == 0) ? 'f' : 'i';
}

static char
get_prefix_b(const Datatype* data) {
	return (data->type == DATA_FLOAT && data->ptr_dim == 0) ? 'f' :
	       (data->type == DATA_BYTE && data->ptr_dim == 0) ? 'b' : 'i';
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
	if (C->current_function) {
		for (VarDeclarationList* i = C->current_function->funcval->desc->fdesc->arguments; i; i = i->next) {
			if (!strcmp(i->decl->name, identifier)) {
				return i->decl;
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
	C->focus = C->focus->parent;
	while (C->focus) {
		/* generate static strings if jumping out of function */
		if (C->focus->type != NODE_BLOCK) {
			popb(C);
		}
		if (C->focus->type == NODE_FUNC_IMPL) {
			/* generate static labels */
			int index = 0;
			for (LiteralList* i = C->string_list; i; i = i->next) {
				writeb(C, FORMAT_DEF_STATIC, index, i->literal);	
				index++;
			}

			/* free static strings */
			LiteralList* next = NULL;
			for (LiteralList* i = C->string_list; i;) {
				free(i->literal);
				next = i->next;
				free(i);
				i = next;
			}
			C->string_list = NULL;
		}
		if (C->focus->next) {
			C->focus = C->focus->next;
			return 1;
		}
		C->focus = C->focus->parent;
	}
	return 0;
}

static void
generate_expression(CompileState* C, ExpNode* exp) {
	if (!exp) return;
	ExpNode* parent = exp->parent;
	void (*writer)(CompileState*, const char*, ...); 
	int is_top = parent == NULL;
	int is_assign;
	if (C->exp_push) {
		writer = pushb;
	} else {
		writer = writeb;
	}
	if (parent) {
		is_assign = IS_ASSIGN(parent->bval);
	} else {
		is_assign = 0;
	}
	int dont_der = is_assign && exp->side == LEAF_LEFT;
	/* don't dereference dot chain on left side of assignment operator */
	if (parent && parent->type == EXP_BINARY && parent->bval->optype == '.' && exp->side == LEAF_LEFT) {
		ExpNode* scan = parent;
		while (scan && scan->type == EXP_BINARY && scan->bval->optype == '.') {
			scan = scan->parent;
		}
		if (scan && scan->type == EXP_BINARY && IS_ASSIGN(scan->bval)) {
			dont_der = 1;
		}
	}
	if (parent && parent->type == EXP_UNARY && parent->uval->optype == '@') {
		dont_der = 1;
	}
	if (parent && parent->type == EXP_BINARY && IS_ASSIGN(parent->bval) &&
		exp->side == LEAF_LEFT && exp->type == EXP_UNARY && exp->uval->optype == '$'){
		dont_der = 1;
	}
	switch (exp->type) {
		case EXP_INTEGER:
			writer(C, "iconst %lld\n", exp->ival);
			break;
		case EXP_FLOAT:
			writer(C, "fconst %f\n", exp->fval);
			break;
		case EXP_STRING: {
			unsigned int label = C->static_count++;	
			writer(C, "iconst " FORMAT_STATIC "\n", label);
			LiteralList* lit = malloc(sizeof(LiteralList));
			lit->literal = malloc(strlen(exp->sval) + 1);
			strcpy(lit->literal, exp->sval);
			lit->next = NULL;
			if (!C->string_list) {
				C->string_list = lit;
			} else {
				LiteralList* scan = C->string_list;
				while (scan->next) {
					scan = scan->next;
				}
				scan->next = lit;
			}
			break;
		}
		case EXP_IDENTIFIER: {
			VarDeclaration* var = get_local(C, exp->sval);
			Datatype* d = var->datatype;

			if (d->type == DATA_FPTR && d->fdesc->is_global) {
				writer(C, "iconst " FORMAT_FUNC "\n", var->name);
			} else if (d->type == DATA_FPTR && d->mods & MOD_FOREIGN) {
				writer(C, "iconst " FORMAT_FUNC "\n", var->name);
			} else if ((dont_der && !IS_STRUCT(d)) || d->array_dim > 0) {
				/* structs are pointers, use locall */
				writer(C, "lea %d\n", var->offset);
			} else {
				char prefix = get_prefix(d);
				writer(C, "%clocall %d\n", prefix, var->offset);
			}
			break;
		}
		case EXP_CAST: {
			const Datatype* to = exp->cxval->d;	
			const Datatype* from = exp->cxval->operand->eval;
			int from_f = from->type == DATA_FLOAT && from->ptr_dim == 0;
			int from_i = from->type != DATA_FLOAT;
			int to_f = to->type == DATA_FLOAT && to->ptr_dim == 0;
			int to_i = to->type != DATA_FLOAT;
			generate_expression(C, exp->cxval->operand);
			if (from_f && to_i) {
				writer(C, "ftoi\n");
			} else if (from_i && to_f) {
				writer(C, "itof\n");
			}
			break;
		}
		case EXP_CALL: {
			FuncCall* call = exp->cval;
			generate_expression(C, call->arguments);
			/* !!!IMPORTANT!!! CCALL ADDRESS COMES __AFTER__ ARGUMENTS! */
			if (call->computed) {
				generate_expression(C, call->fptr);
			}
			if (call->computed) {
				if (call->fptr->eval->mods & MOD_FOREIGN) {
					writer(C, "ccfcall %d\n", call->nargs);
				} else {
					writer(C, "ccall %d\n", call->nargs);
				}
			} else {
				VarDeclaration* f = get_local(C, call->fptr->sval);
				if (f->datatype->mods & MOD_FOREIGN) {
					writer(C, "cfcall " FORMAT_FUNC ", %d\n", call->fptr->sval, call->nargs);
				} else {
					writer(C, "call " FORMAT_FUNC ", %d\n", call->fptr->sval, call->nargs);
				}
			}
			break;
		}
		case EXP_INDEX: {
			ArrayIndex* index = exp->aval;
			generate_expression(C, index->array);
			generate_expression(C, index->index);
			/* ... pointer arithmetic ... */
			writer(C, "iconst %d\n", exp->eval->size);
			writer(C, "imul\n");
			writer(C, "iadd\n");
			/* struct just exists on the stack, don't dereference */
			if (!dont_der && !IS_STRUCT(exp->eval)) {
				writer(C, "ider\n");
			}
			break;
		}
		case EXP_UNARY: {
			ExpNode* operand = exp->uval->operand;
			generate_expression(C, operand);
			switch (exp->uval->optype) {
				case '$': {
					/* don't dereference if the parent is an assignment */
					/* @TODO problem? */
					if (!dont_der) {
						int prefix = get_prefix_b(exp->eval);
						writer(C, "%cder\n", prefix);
					}
					break;
				}
				case '!':
					writer(C, "not\n");
					break;
			}
			break;
		}
		case EXP_BINARY: {
			ExpNode* lhs = exp->bval->left;
			ExpNode* rhs = exp->bval->right;
			if (exp->bval->optype == '.') {
				int is_bottom = lhs->type == EXP_IDENTIFIER;
				generate_expression(C, exp->bval->left);				
				VarDeclaration* field;
				if (is_bottom) {
					VarDeclaration* obj = get_local(C, lhs->sval);
					field = get_field(obj->datatype->sdesc, rhs->sval);
				} else {
					/* if it's a chain (not at the bottom), grab the field from the eval
					 * instead of finding the struct from its identifier */
					field = get_field(lhs->eval->sdesc, rhs->sval);
				}
				if (DO_OPTIMIZE && field->offset > 0) {
					writer(C, "iinc %d\n", field->offset);	
				}
				if (!dont_der) {
					writer(C, "%cder\n", get_prefix_b(exp->eval));	
				}
			} else if (exp->bval->optype == '=') {
				generate_expression(C, lhs);
				generate_expression(C, rhs);
				if (!is_top) {
					writer(C, "dup\n");
				}
				writer(C, "%csave\n", get_prefix_b(lhs->eval)); 
			} else if (IS_ASSIGN(exp->bval)) {
				char lp = get_prefix_b(lhs->eval);
				generate_expression(C, lhs);
				writer(C, "dup\n");
				writer(C, "%cder\n", lp);
				generate_expression(C, rhs);
				switch (exp->bval->optype) {
					case SPEC_INC_BY:
						writer(C, "%cadd\n", lp);
						break;
					case SPEC_DEC_BY:
						writer(C, "%csub\n", lp);
						break;
					case SPEC_MUL_BY:
						writer(C, "%cmul\n", lp);
						break;
					case SPEC_DIV_BY:
						writer(C, "%cdiv\n", lp);
						break;
					case SPEC_MOD_BY:
						writer(C, "mod\n");
						break;
					case SPEC_SHL_BY:
						writer(C, "shl\n");
						break;
					case SPEC_SHR_BY:
						writer(C, "shr\n");
						break;
					case SPEC_AND_BY:
						writer(C, "and\n");
						break;
					case SPEC_OR_BY:
						writer(C, "or\n");
						break;
					case SPEC_XOR_BY:
						writer(C, "xor\n");
						break;

				}
				if (!is_top) {
					writer(C, "dup\n");
				}
				writer(C, "%csave\n", lp);
			} else { 
				generate_expression(C, lhs);
				generate_expression(C, rhs);
				const Datatype* leval = lhs->eval;
				const Datatype* reval = rhs->eval;
				if (exp->bval->optype == ',') {
					break;
				}
				char prefix = get_prefix(exp->eval);
				switch (exp->bval->optype) {
					case '+':
						if (IS_PTR(leval) && IS_INT(reval)) {
							/* pointer arithmetic, multiply by size of pointer */
							writer(C, "iconst %d\nimul\n", leval->size);
						}
						writer(C, "%cadd", prefix);	
						break;
					case '-':
						writer(C, "%csub", prefix);	
						break;
					case '*':
						writer(C, "%cmul", prefix);	
						break;
					case '/':
						writer(C, "%cdiv", prefix);	
						break;
					case SPEC_SHL:
						writer(C, "shl");
						break;
					case SPEC_SHR:
						writer(C, "shr");
						break;
					case '%':
						writer(C, "mod");
						break;
					
					case '>':
					case '<':
					case SPEC_GE:
					case SPEC_LE:
					case SPEC_EQ:
					case SPEC_NEQ: {
						char op = exp->bval->optype;
						const char* ins = (
							op == '>' ? "gt" :
							op == '<' ? "lt" :
							op == SPEC_GE ? "ge" :
							op == SPEC_LE ? "le" :
							op == SPEC_EQ ? "e" : "ne"	
						);	
						const char* inverted = (
							op == '>' ? "le" :
							op == '<' ? "ge" :
							op == SPEC_GE ? "lt" :
							op == SPEC_LE ? "gt" :
							op == SPEC_EQ ? "ne" : "e"	
						);	
						writer(C, "%ccmp\n", prefix);
						if (C->cond_jmp && is_top) {
							/* if it's a conditional expression and the cond operator is at
							 * the top of the tree, a simple conditional jump can be generated */
							writer(C, "j%s " FORMAT_LABEL, inverted, C->bottom_label);
						} else {
							if (C->cond_jmp) {
								writer(C, "p%s\n", inverted);
								writer(C, "%ctest", prefix); 
							} else {
								writer(C, "p%s", ins);
							}
						}
						break;
					}
				}
				writer(C, "\n");
			}
			break;
		}
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
generate_return(CompileState* C) {
	generate_expression(C, C->focus->stateval->exp);
	writeb(C, "jmp " FORMAT_LABEL "\n", C->return_label);
}

/* helper function for while, if, for */
static void
generate_condition(CompileState* C, ExpNode* condition) {
	if (condition) {
		C->cond_jmp = 1;
		generate_expression(C, condition);
		C->cond_jmp = 0;
		if (!IS_COMPARE(condition)) {
			writeb(C, "%ctest\n", get_prefix(condition->eval));
			writeb(C, "jz " FORMAT_LABEL "\n", C->bottom_label);
		}
	} else {
		writeb(C, "iconst 0\n");
		writeb(C, "itest\n");
	}
}

static void
generate_if(CompileState* C) {
	C->bottom_label = C->label_count++;
	writeb(C, "; if statement\n;\tbot: " FORMAT_LABEL "\n", C->bottom_label);
	generate_condition(C, C->focus->ifval->condition);
	pushb(C, FORMAT_DEF_LABEL, C->bottom_label);	
}

static void
generate_while(CompileState* C) {
	C->cont_label = C->label_count++;
	C->bottom_label = C->break_label = C->label_count++;
	writeb(C, 
		"; while loop\n;\ttop: " FORMAT_LABEL "\n;\tbot: " FORMAT_LABEL "\n",
		C->cont_label,
		C->bottom_label
	);
	writeb(C, FORMAT_DEF_LABEL, C->cont_label);
	generate_condition(C, C->focus->whileval->condition);
	pushb(C, "jmp " FORMAT_LABEL "\n", C->cont_label);
	pushb(C, FORMAT_DEF_LABEL, C->break_label);
}

static void
generate_for(CompileState* C) {
	C->cont_label = C->label_count++;
	C->bottom_label = C->break_label = C->label_count++;
	writeb(C, 
		"; for loop\n;\ttop: " FORMAT_LABEL "\n;\tbot: " FORMAT_LABEL "\n",
		C->cont_label,
		C->bottom_label
	);
	generate_expression(C, C->focus->forval->init);
	writeb(C, FORMAT_DEF_LABEL, C->cont_label);
	generate_condition(C, C->focus->forval->condition);
	C->exp_push = 1;
	generate_expression(C, C->focus->forval->statement);	
	C->exp_push = 0;	
	pushb(C, "jmp " FORMAT_LABEL "\n", C->cont_label);
	pushb(C, FORMAT_DEF_LABEL, C->break_label);
}

/* generate_function helper function */
static void 
print_stack_map(CompileState* C, TreeNode* node) {
	VarDeclarationList* list = NULL;
	switch (node->type) {
		case NODE_BLOCK:
			list = node->blockval->locals;
			break;
		case NODE_FUNC_IMPL:
			list = node->funcval->desc->fdesc->arguments;
			break;	
	}
	if (list) {
		for (; list; list = list->next) {
			VarDeclaration* var = list->decl;
			char* dt = tostring_datatype(var->datatype);
			writeb(C, ";\t [%04d] %s: %s\n", var->offset, var->name, dt);
			free(dt);
		}
	}
	for (TreeNode* i = get_child(node); i; i = i->next) {
		print_stack_map(C, i);
	}

}

static void
generate_function(CompileState* C) {
	C->current_function = C->focus;
	C->label_count = 0; /* reset label count, local labels are used */
	C->static_count = 0;
	C->return_label = C->label_count++;

	/* todo make this debug less nasty */
	char* header = tostring_datatype(C->focus->funcval->desc);
	writeb(C, "\n; %s: ", C->focus->funcval->name);
	writeb(C, header);
	writeb(C, "\n");
	free(header);
	writeb(C, "; return label: " FORMAT_LABEL "\n", C->return_label);
	writeb(C, "; reserved space: %d bytes\n", C->focus->funcval->desc->fdesc->stack_space);
	writeb(C, "; stack map:\n");
	print_stack_map(C, C->focus);

	TreeFunction* func = C->focus->funcval;
	FunctionDescriptor* desc = func->desc->fdesc;
	writeb(C, FORMAT_DEF_FUNC, func->name);	
	int index = 0;
	for (VarDeclarationList* i = desc->arguments; i; i = i->next) {
		writeb(C, "%carg %d\n", get_prefix(i->decl->datatype), index++);
	}
	if (DO_OPTIMIZE && desc->stack_space > 0) {
		writeb(C, "res %d\n", desc->stack_space);
	}
	pushb(C, FORMAT_DEF_LABEL, C->return_label);
	const Datatype* ret = desc->return_type;
	if (ret->type == DATA_VOID) {
		pushb(C, "vret\n");	
	} else {
		pushb(C, "%cret\n", get_prefix(ret));
	}

}

static void
initialize_local(CompileState* C, VarDeclaration* var) {
	/* global functions don't get initialized */
	Datatype* d = var->datatype;
	if (d->type == DATA_FPTR /*&& d->desc->is_global*/) {
		return;
	}
	int is_ptr = var->datatype->ptr_dim > 0;
	writeb(C, "; initialize '%s'\n", var->name);
	if (d->array_dim > 0) {
		writeb(C, "lea %d\n", var->offset);
	} else if (d->type == DATA_STRUCT && !is_ptr) {
		/* if it's a struct, initialize it as a pointer to stack space */
		/* note a struct's stack space exists 8 bytes after its pointer */
		writeb(C, "lea %d\n", var->offset + 8);
	} else {
		/* otherwise just initialize it as 0... no need to initialize a float
		 * differently because a 0 int is a 0 float */
		writeb(C, "iconst 0\n");
	}
	writeb(C, "ilocals %d\n", var->offset);
	writeb(C, "; -----------\n");
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
	C.exp_push = 0;
	C.cond_jmp = 0;
	C.static_count = 0;
	C.current_function = NULL;
	C.string_list = NULL;

	if (!C.root_node) {
		fclose(C.handle);
		return;
	}

	writeb(&C, "jmp __ENTRY__\n");

	/* generate c function names */
	for (VarDeclarationList* i = C.root_node->blockval->locals; i; i = i->next) {
		VarDeclaration* var = i->decl;
		Datatype* d = var->datatype;
		if (d->type == DATA_FPTR && d->mods & MOD_FOREIGN) {
			char* d = tostring_datatype(var->datatype);
			writeb(&C, "; %s: %s\n", var->name, d);
			free(d);
			writeb(&C, "%s: db \"%s\\0\"\n", var->name, var->name);
		}
	}
	
	do {
		writeb(&C, "; @%d\n", C.focus->line);
		switch (C.focus->type) {
			case NODE_IF:
				generate_if(&C);
				break;	
			case NODE_WHILE:
				generate_while(&C);
				break;
			case NODE_FOR:
				generate_for(&C);
				break;
			case NODE_FUNC_IMPL:
				generate_function(&C);
				break;
			case NODE_STATEMENT: {
				ExpNode* exp = C.focus->stateval->exp;
				generate_expression(&C, exp);
				/*
				if (exp->eval->type != DATA_VOID && !(exp->type == EXP_BINARY && IS_ASSIGN(exp->bval))) {
					writeb(&C, "pop\n");
				}
				*/
				break;
			}
			case NODE_BREAK:
				generate_break(&C);
				break;
			case NODE_CONTINUE:
				generate_continue(&C);
				break;
			case NODE_RETURN:
				generate_return(&C);
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
	
	writeb(&C, "\n__ENTRY__:\n");	
	writeb(&C, "call " FORMAT_FUNC ", 0\n", "main");
	writeb(&C, "exit\n");

	fclose(C.handle);

}
