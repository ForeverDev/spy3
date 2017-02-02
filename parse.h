#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include "lex.h"
#include "spy_types.h"

#define MOD_STATIC (0x1 << 0)
#define MOD_CONST  (0x1 << 1)
#define MOD_FOREIGN  (0x1 << 2)

#define IS_PTR(d) (d->ptr_dim > 0)
#define IS_ARRAY(d) (d->array_dim > 0)
#define IS_STRUCT(d) (d->ptr_dim == 0 && d->array_dim == 0 && d->sdesc != NULL)
#define IS_INT(d) (d->ptr_dim == 0 && d->array_dim == 0 && d->type == DATA_INT)
#define IS_FLOAT(d) (d->ptr_dim == 0 && d->array_dim == 0 && d->type == DATA_FLOAT)
#define IS_BYTE(d) (d->ptr_dim == 0 && d->array_dim == 0 && d->type == DATA_BYTE)
#define IS_STRING(d) (d->ptr_dim == 1 && d->array_dim == 0 && d->type == DATA_BYTE)
#define IS_CT_CONSTANT(e) (e->type == EXP_INTEGER)

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

#define IS_COMPARE(t) ((t)->type == EXP_BINARY && \
						( \
							(t)->bval->optype == '>' || \
							(t)->bval->optype == '<' || \
							(t)->bval->optype == SPEC_GE || \
							(t)->bval->optype == SPEC_LE || \
							(t)->bval->optype == SPEC_EQ || \
							(t)->bval->optype == SPEC_NEQ \
						) \
					)

#define IS_LITERAL(d) ((d)->type == EXP_INTEGER || \
					   (d)->type == EXP_FLOAT || \
					   (d)->type == EXP_STRING)



typedef struct TreeNode TreeNode;
typedef struct TreeIf TreeIf;
typedef struct TreeWhile TreeWhile;
typedef struct TreeFor TreeFor;
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
typedef struct ArrayIndex ArrayIndex;
typedef struct Cast Cast;

typedef enum ExpNodeType ExpNodeType;
typedef enum TreeNodeType TreeNodeType;

enum ExpNodeType {
	EXP_NOEXP = 0,
	EXP_UNARY = 1,
	EXP_BINARY = 2,
	EXP_CAST = 3,
	EXP_INTEGER = 4, /* literal */
	EXP_FLOAT = 5,   /* literal */
	EXP_STRING = 6,  /* literal */
	EXP_IDENTIFIER = 7,
	EXP_CALL = 8,
	EXP_INDEX = 9
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

struct ArrayIndex {
	ExpNode* array;
	ExpNode* index;
};

struct UnaryOp {
	char optype;
	ExpNode* operand;
};

struct Cast {
	Datatype* d;
	ExpNode* operand;
};

struct FuncCall {
	ExpNode* arguments;
	int nargs;
	int computed; /* computed if fptr is not just an identifier */
	ExpNode* fptr; /* should evaluate to the address of a function */
};

struct ExpNode {
	ExpNodeType type;
	ExpNode* parent;
	const Datatype* eval;
	enum {
		LEAF_NA = 0,
		LEAF_LEFT = 1,
		LEAF_RIGHT = 2,
	} side; /* NOTE only applicable if child of binop */
	union {
		BinaryOp* bval;
		UnaryOp* uval;
		spy_int ival;
		spy_float fval;
		char* sval;
		FuncCall* cval;
		Cast* cxval;
		ArrayIndex* aval;
	};
};

struct FunctionDescriptor {
	unsigned int stack_space;
	unsigned int arg_space;
	int is_global;
	int nargs;
	int vararg;
	VarDeclarationList* arguments;
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
		DATA_VOID = 6,
		DATA_FILE = 7
	} type;

	/* 0 if not array */
	unsigned int array_dim;
	unsigned int* array_size; /* null if !array_dim.. array_size len == array_dim */

	/* 0 if not pointer */
	unsigned int ptr_dim;
	
	unsigned int mods;

	/* number of bytes needed to store a variable of this type...
	 * NOTE: when the res instructions is used, the number of bytes reserved
	 * is (sum locals in function) rounded up to nearest multiple of 8 
	 * (for stack alignment)
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
	
	/* offset is a bit special... for function arguments and locals, it is the
	 * offset from the base pointer.  for struct field, it is the offset from
	 * the start structs */
	unsigned int offset;

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
	int has_else;
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
	Datatype* desc;
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
	/* an else statement doesn't evaluate to an actual node... rather,
	 * if a token follows an else token, its is_else field is marked 1 */
	int is_else;
	int line;
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
	TreeNode* current_function;
	Datatype* type_int;
	Datatype* type_float;
	Datatype* type_byte;
	Datatype* type_string;
	Datatype* type_file;
	TreeStructList* defined_structs;
	unsigned int current_offset;
	int next_is_else;
};


ParseState* generate_syntax_tree(TokenList*);
void print_expression(ExpNode*, int);
char* tostring_datatype(const Datatype*);

#endif
