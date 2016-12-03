#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef struct TreeNode TreeNode;
typedef struct TreeIf TreeIf;
typedef struct TreeWhile TreeWhile;
typedef struct TreeFor TreeFor;
typedef struct TreeFunctionImpl TreeFunctionImpl;
typedef struct TreeBreak TreeBreak;
typedef struct TreeContinue TreeContinue;
typedef struct TreeReturn TreeReturn;
typedef struct TreeBlock TreeBlock;

typedef struct ExpNode ExpNode;
typedef struct BinaryOp BinaryOp;
typedef struct UnaryOp UnaryOp;

typedef enum ExpNodeType ExpNodeType;
typedef enum TreeNodeType TreeNodeType;

enum ExpNodeType {
	EXP_NOEXP = 0,
	EXP_UNARY = 1,
	EXP_BINARY = 2,
	EXP_CAST = 3
};

enum TreeNodeType {
	NODE_IF = 1,
	NODE_FOR = 2,
	NODE_WHILE = 3,
	NODE_FUNC_IMPL = 4,
	NODE_STATEMENT = 5,
	NODE_BREAK = 6,
	NODE_CONTINUE = 7,
	NODE_RETURN = 8,
	NODE_BLOCK = 9
};

struct BinaryOp {
	char optype;
	ExpNode* left;
	ExpNode* right;
};

struct UnaryOp {
	char optype;
	ExpNode* operand;
};

struct ExpNode {
	ExpNodeType type;
	union {
		BinaryOp* bval;
		UnaryOp* uval;
	};
};

struct TreeBlock {
	TreeNode* child;
};

struct TreeNode {
	TreeNode* parent;
	TreeNode* next;
	TreeNodeType type;
	union {
		TreeBlock* blockval;	
	};
};

TreeNode* generate_syntax_tree(TokenList*);

#endif
