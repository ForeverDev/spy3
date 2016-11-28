#ifndef Spy_H
#define Spy_H

#include <stdint.h>
#include "spy_types.h"

#define SIZE_MEMORY 0x100000
#define SIZE_STACK  0x010000
#define SIZE_CODE	0x010000 /* TODO make this dynamic */

#define FLAG_EQ		(0x1 << 0) /* NEQ = !EQ */
#define FLAG_GT		(0x1 << 1) /* GE = !LT */
#define FLAG_LT		(0x1 << 2) /* LE = !GT */
#define FLAG_Z		(0x1 << 3) /* NZ = !Z */
#define FLAG_S		(0x1 << 4) /* NS = !S */

/* NOTES
 * 
 * CODE LAYOUT:
 *   32BIT INT DATA_SIZE
 *   ... CODE ...
 */

typedef struct SpyState SpyState;
typedef struct SpyCFunc SpyCFunc;
typedef struct SpyCFuncList SpyCFuncList;
typedef struct SpyInstruction SpyInstruction;

struct SpyState {
	spy_byte* memory;	
	spy_byte* ip;
	spy_byte* sp;
	spy_byte* bp;
	spy_byte* code;
	SpyCFuncList* cfuncs;
	uint16_t flags;
};

struct SpyCFunc {
	char* name;
	spy_integer (*f)(SpyState*);
};

struct SpyCFuncList {
	SpyCFunc* cfunc;
	SpyCFuncList* next;
};	

struct SpyInstruction {
	const char* name;
	uint8_t opcode;
	enum InstructionOperand {
		OP_NONE = 0,
		OP_UINT8 = 1,
		OP_UINT32 = 2,
		OP_INT64 = 3,
		OP_FLOAT64 = 4
	} operands[4];
};

extern const SpyInstruction spy_instructions[255]; 

void spy_init();
void spy_execute(const char*);
void spy_dump();
const SpyInstruction* spy_get_instruction(const char*); /* for the assembler... */

#endif
