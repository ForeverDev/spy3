#include <stdio.h>
#include <stdlib.h>
#include "generate.h"

typedef struct CompileState CompileState;

struct CompileState {
	TreeNode* focus;
	TreeNode* root_node;
	FILE* handle;
};

/* generate functions */
static void generate_if(CompileState*);

/* misc function */
static int advance(CompileState*);
static TreeNode* get_child(TreeNode*);

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
	/* next thing in block? jump to it */
	} else if (focus->next) {
		C->focus = focus->next;
	/* parent->next? */
	} else {
		while (focus != C->root_node && !focus->next) {
			focus = focus->parent;	
		}
		if (focus == C->root_node) {
			return 0;
		}
	}
	return 1;	
}

static void
generate_if(CompileState* C) {

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

	if (!C.root_node) {
		return;
	}
	
	do {
		switch (C.focus->type) {
			case NODE_IF:
				generate_if(&C);
				break;	
		}
	} while (advance(&C));

	fclose(C.handle);

}
