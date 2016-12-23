#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "spy_types.h"

typedef struct TreeNode TreeNode;
typedef struct TreeIf TreeIf;
typedef struct TreeWhile TreeWhile;
typedef struct TreeFor TreeFor;
typedef struct TreeFunctionImpl TreeFunctionImpl;
typedef struct TreeBreak TreeBreak;
typedef struct TreeContinue TreeContinue;
typedef struct TreeReturn TreeReturn;
typedef struct TreeBlock TreeBlock;
typedef struct TreeStatement TreeStatement;
typedef struct TreeWhile TreeWhile;
typedef struct TreeFunction TreeFunction;
typedef struct TreeStruct TreeStruct;
typedef struct TreeStructList TreeStructList;
typedef struct Datatype Datatype;
typedef struct DatatypeList DatatypeList;
typedef struct FunctionDescriptor FunctionDescriptor;
typedef struct StructDescriptor StructDescriptor;
typedef struct VarDeclaration VarDeclaration;
typedef struct VarDeclarationList VarDeclarationList;
typedef struct FuncCall FuncCall;
typedef struct ParseState ParseState;

typedef struct ExpNode ExpNode;
typedef struct BinaryOp BinaryOp;
typedef struct UnaryOp UnaryOp;

typedef enum ExpNodeType ExpNodeType;
typedef enum TreeNodeType TreeNodeType;

enum ExpNodeType {
	EXP_NOEXP = 0,
	EXP_UNARY = 1,
	EXP_BINARY = 2,
	EXP_CAST = 3,
	EXP_INTEGER = 4, /* literal */
	EXP_FLOAT = 5, /* literal */
	EXP_IDENTIFIER,
	EXP_CALL
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

struct FuncCall {
	ExpNode* fptr; /* should evaluate to the address of a function */
	ExpNode* arguments;
	int num_args;
};

struct ExpNode {
	ExpNodeType type;
	ExpNode* parent;
	enum {
		LEAF_LEFT = 1,
		LEAF_RIGHT = 2
	} side; /* NOTE only applicable if child of binop */
	union {
		BinaryOp* bval;
		UnaryOp* uval;
		spy_int ival;
		spy_float fval;
		char* sval;
		FuncCall* cval;
	};
};

struct FunctionDescriptor {
	DatatypeList* arguments;
	Datatype* return_type;
};

struct StructDescriptor {
	VarDeclarationList* fields;
	unsigned int size;
};

struct Datatype {
	enum {
		DATA_NOTYPE = 0,
		DATA_INT = 1,
		DATA_FLOAT = 2,
		DATA_BYTE = 3,
		DATA_FPTR = 4,
		DATA_STRUCT = 5,
		DATA_VOID = 6
	} type;

	/* 0 if not array */
	unsigned int array_dim;
	unsigned int* array_size; /* null if !array_dim.. array_size len == array_dim */

	/* 0 if not pointer */
	unsigned int ptr_dim;
	
	/* number of bytes needed to store a variable of this type...
	 * NOTE: when the res instructions is used, the number of bytes reserved
	 * is (sum locals in function) rounded up to nearest multiple of 8 (for stack alignment)
	 *
	 * a few constants: 
	 *   int:	size=8
	 *   float: size=8
	 *   byte:  size=1
	 *   ptr to any type: size=8
	 *
	 */ 
	unsigned int size;

	union {
		/* only applicable if DATA_FPTR */
		FunctionDescriptor* fdesc;

		/* only applicable if DATA_STRUCT */
		TreeStruct* sdesc;
	};
};

struct DatatypeList {
	Datatype* data;
	DatatypeList* next;
};

struct VarDeclaration {
	char* name;
	Datatype* datatype;
};

struct VarDeclarationList {
	VarDeclaration* decl;
	VarDeclarationList* next;
};

struct TreeBlock {
	TreeNode* child;
	VarDeclarationList* locals;
};

struct TreeIf {
	ExpNode* condition;
	TreeNode* child;	
};

struct TreeWhile {
	ExpNode* condition;
	TreeNode* child;	
};

struct TreeFor {
	ExpNode* init;
	ExpNode* condition;
	ExpNode* statement;
	TreeNode* child;
};

struct TreeStatement {
	ExpNode* exp;
};

struct TreeFunction {
	FunctionDescriptor* desc;
	char* name;
	TreeNode* child;
};

struct TreeStruct {
	StructDescriptor* desc;
	char* name;
};

struct TreeStructList {
	TreeStruct* str;
	TreeStructList* next;
};

struct TreeNode {
	TreeNode* parent;
	TreeNode* next;
	TreeNode* prev;
	TreeNodeType type;
	union {
		TreeBlock* blockval;	
		TreeIf* ifval;
		TreeFor* forval;
		TreeWhile* whileval;
		TreeStatement* stateval;
		TreeFunction* funcval;
	};
};

struct ParseState {
	TokenList* tokens;
	TokenList* marked;
	TokenList* focus; /* used for parse_exprecsion and helper funcs */
	TreeNode* root_node; /* type == NODE_BLOCK */
	TreeNode* current_block;
	TreeNode* append_target; /* what to append to */
	Datatype* type_int;
	Datatype* type_float;
	Datatype* type_byte;
	TreeStructList* defined_structs;
};


ParseState* generate_syntax_tree(TokenList*);

#endif
