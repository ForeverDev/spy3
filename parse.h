#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef struct TreeNode TreeNode;
typedef struct ExpNode ExpNode;
typedef struct BinaryOp BinaryOp;
typedef struct UnaryOp UnaryOp;
typedef enum ExpNodeType ExpNodeType;

enum ExpNodeType {
	NODE_NONODE = 0,
	NODE_UNARY = 1,
	NODE_BINARY = 2,
	NODE_CAST = 3
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

struct TreeNode {
	
};

TreeNode* generate_syntax_tree(TokenList*);

#endif
