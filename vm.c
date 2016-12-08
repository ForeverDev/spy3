#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "vm.h"
#include "spylib.h"
#include "capi_load.h"

static SpyState* spy = NULL;

/* TODO all float instructions */
const SpyInstruction spy_instructions[255] = {
	{"NOP", 0x00, {OP_NONE}},				/* [] -> [] */
	{"iconst", 0x01, {OP_INT64}},			/* [] -> [int val] */
	{"icmp", 0x02, {OP_NONE}},				/* [int a, int b] -> [] */
	{"itest", 0x03, {OP_NONE}},				/* [int a] -> [] */
	{"je", 0x04, {OP_INT64}},				/* [] -> [] */
	{"jne", 0x05, {OP_INT64}},				/* [] -> [] */
	{"jgt", 0x06, {OP_INT64}},				/* [] -> [] */
	{"jge", 0x07, {OP_INT64}},				/* [] -> [] */
	{"jlt", 0x08, {OP_INT64}},				/* [] -> [] */
	{"jle", 0x09, {OP_INT64}},				/* [] -> [] */
	{"jz", 0x0A, {OP_INT64}},				/* [] -> [] */
	{"jnz", 0x0B, {OP_INT64}},				/* [] -> [] */
	{"js", 0x0C, {OP_INT64}},				/* [] -> [] */
	{"jns", 0x0D, {OP_INT64}},				/* [] -> [] */
	{"jmp", 0x0E, {OP_INT64}},				/* [] -> [] */
	{"cjeq", 0x0F, {OP_NONE}},				/* [int addr] -> [] */
	{"cjneq", 0x10, {OP_NONE}},				/* [int addr] -> [] */
	{"cjgt", 0x11, {OP_NONE}},				/* [int addr] -> [] */
	{"cjge", 0x12, {OP_NONE}},				/* [int addr] -> [] */
	{"cjlt", 0x13, {OP_NONE}},				/* [int addr] -> [] */
	{"cjle", 0x14, {OP_NONE}},				/* [int addr] -> [] */
	{"cjz", 0x15, {OP_NONE}},				/* [int addr] -> [] */
	{"cjnz", 0x16, {OP_NONE}},				/* [int addr] -> [] */
	{"cjs", 0x17, {OP_NONE}},				/* [int addr] -> [] */
	{"cjns", 0x18, {OP_NONE}},				/* [int addr] -> [] */
	{"cjmp", 0x19, {OP_NONE}},				/* [int addr] -> [] */
	{"iadd", 0x1A, {OP_NONE}},				/* [int a, int b] -> [int result] */
	{"isub", 0x1B, {OP_NONE}},				/* [int a, int b] -> [int result] */
	{"imul", 0x1C, {OP_NONE}},				/* [int a, int b] -> [int result] */
	{"idiv", 0x1D, {OP_NONE}},				/* [int a, int b] -> [int result] */
	{"shl", 0x1E, {OP_NONE}},				/* [int a, int bits] -> [int result] */
	{"shr", 0x1F, {OP_NONE}},				/* [int a, int bits] -> [int result] */
	{"and", 0x20, {OP_NONE}},				/* [int a, int b] -> [int result] */
	{"or", 0x21, {OP_NONE}},				/* [int a, int b] -> [int result] */
	{"xor", 0x22, {OP_NONE}},				/* [int a, int b] -> [int result] */
	{"call", 0x23, {OP_INT64, OP_INT64}},	/* [] -> [int ip_save, int bp_save, int nargs */
	{"ccall", 0x24, {OP_INT64}},			/* [int addr] -> [int ip_save, int bp_save, int nargs */
	{"cfcall", 0x25, {OP_INT64, OP_INT64}},	/* [] -> [int ip_save, int bp_save, int nargs] */
	{"iret", 0x26, {OP_NONE}},				/* [int ip_save, int bp_save, int nargs, int ret_val] -> [int ret_val] */
	{"exit", 0x27, {OP_NONE}},				/* [] -> [] */
	{"ider", 0x28, {OP_NONE}},				/* [int addr] -> [int value] */
	{"bder", 0x29, {OP_NONE}},				/* [int addr] -> [int value (casted uint8_t)] */
	{"isave", 0x2A, {OP_NONE}},				/* [int addr, int value] -> [] */
	{"bsave", 0x2B, {OP_NONE}},				/* [int addr, int value (casted uint8_t)] -> [] */
	{"res", 0x2C, {OP_INT64}},				/* [] -> [int[bytes]] */
	{"iinc", 0x2D, {OP_INT64}},				/* [int val = x] -> [int val = x + amount] */
	{"pop", 0x2E, {OP_NONE}},				/* [64bit thing] -> [] */
	{"iarg", 0x2F, {OP_INT64}},				/* [] -> [int value] */
	{"barg", 0x30, {OP_INT64}},				/* [] -> [int value (casted uint8_t)] */
	{"lea", 0x31, {OP_INT64}},				/* [] -> [int addr] */
	{"aisave", 0x32, {OP_INT64}},			/* [int value] -> [] */
	{"absave", 0x33, {OP_INT64}},			/* [int value (casted uint8_t)] -> [] */
	{"aider", 0x34, {OP_INT64}},			/* [] -> [int value] */
	{"abder", 0x35, {OP_INT64}},			/* [] -> [int value (casted uint8_t)] */
	{"malloc", 0x36, {OP_NONE}},			/* [int num_bytes] -> [int ptr] */
	{"free", 0x37, {OP_NONE}},				/* [int ptr] -> [] */
	{"vret", 0x38, {OP_NONE}},				/* [int ip_save, int bp_save, int nargs] -> [int ret_val] */
	{"ilocall", 0x39, {OP_INT64}},			/* [] -> [int value] */
	{"blocall", 0x3A, {OP_INT64}},			/* [] -> [int value (casted uint8_t)] */
	{"ilocals", 0x3B, {OP_INT64}},			/* [int value] -> [] */
	{"blocals", 0x3C, {OP_INT64}},			/* [int value (casted uint8_t)] -> [] */
	{"pe", 0x3D, {OP_NONE}},				/* [] -> [int flag] */
	{"pne", 0x3E, {OP_NONE}},				/* [] -> [int flag] */
	{"pgt", 0x3F, {OP_NONE}},				/* [] -> [int flag] */
	{"pge", 0x40, {OP_NONE}},				/* [] -> [int flag] */
	{"plt", 0x41, {OP_NONE}},				/* [] -> [int flag] */
	{"ple", 0x42, {OP_NONE}},				/* [] -> [int flag] */
	{"pz", 0x43, {OP_NONE}},				/* [] -> [int flag] */
	{"pnz", 0x44, {OP_NONE}},				/* [] -> [int flag] */
	{"ps", 0x45, {OP_NONE}},				/* [] -> [int flag] */
	{"pns", 0x46, {OP_NONE}},				/* [] -> [int flag] */
	{"fconst", 0x47, {OP_FLOAT64}},			/* [] -> [float value] */
	{"fcmp", 0x48, {OP_NONE}},				/* [float a, float b] -> [] */
	{"ftest", 0x49, {OP_NONE}},				/* [float value] -> [] */
	{"fadd", 0x4A, {OP_NONE}},				/* [float a, float b] -> [float resu;t] */
	{"fsub", 0x4B, {OP_NONE}},				/* [float a, float b] -> [float resu;t] */
	{"fmul", 0x4C, {OP_NONE}},				/* [float a, float b] -> [float resu;t] */
	{"fdiv", 0x4D, {OP_NONE}},				/* [float a, float b] -> [float resu;t] */
	{"finc", 0x4E, {OP_FLOAT64}},			/* [float value] -> [float value] */
	{"fret", 0x4F, {OP_NONE}},				/* [float value] -> [float value] */	
	{"flocall", 0x50, {OP_INT64}},			/* [] -> [float value] */	
	{"flocals", 0x51, {OP_INT64}},			/* [float value] -> [] */
	{"afsave", 0x52, {OP_INT64}},			/* [float value] -> [] */
	{"afder", 0x53, {OP_INT64}},			/* [] -> [float value] */
	{"fder", 0x54, {OP_NONE}},				/* [int addr] -> [float value] */
	{"farg", 0x55, {OP_INT64}},				/* [] -> [float value] */
	{"itof", 0x56, {OP_NONE}},				/* [int value] -> [float value] */
	{"ftoi", 0x57, {OP_NONE}},				/* [float value] -> [int value] */
	{"dup", 0x58, {OP_NONE}},				/* [int value] -> [int value, int value] */

	/* debuggers */
	{"ilog", 0xFD, {OP_NONE}},				
	{"blog", 0xFE, {OP_NONE}},
	{"flog", 0xFF, {OP_NONE}},


	{NULL, 0x00, {OP_NONE}}		

};

void
spy_init() {
	
	spy = malloc(sizeof(SpyState));
	spy->memory = malloc(SIZE_MEMORY);	

	/* zero registers for now, initialized in spy_execute */
	spy->ip = NULL;
	spy->sp = NULL;
	spy->bp = NULL;
	spy->code = NULL;

	/* zero flags */
	spy->flags = 0;

	/* initialize c-api */
	spy->cfuncs = malloc(sizeof(SpyCFuncList));
	spy->cfuncs->cfunc = NULL;
	spy->cfuncs->next = NULL;
	spy_init_capi(spy);

	/* initialize memory map */
	spy->memory_map = malloc(sizeof(MemoryBlockList));
	/* first block marks the start of the heap */
	spy->memory_map->block = malloc(sizeof(MemoryBlock));;
	spy->memory_map->block->bytes = 0;
	spy->memory_map->block->addr = SIZE_CODE + SIZE_STACK;
	spy->memory_map->next = NULL;
	spy->memory_map->prev = NULL;

}

static uint8_t
spy_code_int8() {
	return *spy->ip++;
}

static uint32_t
spy_code_uint32() {
	int32_t ret = *(int32_t *)spy->ip;
	spy->ip += 4;
	return ret;
}

static spy_int
spy_code_int64() {
	spy_int ret = *(spy_int *)spy->ip;
	spy->ip += 8;
	return ret;
}

static spy_float
spy_code_float() {
	spy_float ret = *(spy_float *)spy->ip;
	spy->ip += 8;
	return ret;
}

static void
spy_die(const char* msg, ...) {
	va_list args;
	va_start(args, msg);
	printf("\n\n*** SPYRE RUNTIME ERROR ***\n\tmessage: ");
	vprintf(msg, args);
	printf("\n\n\n");
	va_end(args); /* is this really necessary? */
	exit(1);
}

/* NOTE: exposed to assembler */
const SpyInstruction*
spy_get_instruction(const char* name) {
	for (const SpyInstruction* i = spy_instructions; i->name; i++) {
		if (!strcmp(i->name, name)) {
			return i;
		}	
	}
	return NULL;
}

static const SpyInstruction*
spy_get_instruction_op(uint8_t opcode) {
	for (const SpyInstruction* i = spy_instructions; i->name; i++) {
		if (i->opcode == opcode) {
			return i;
		}	
	}
	return NULL;
}

void
spy_dump() {
	printf("STACK: \n");
	for (uint8_t* i = spy->sp; i >= &spy->memory[SIZE_CODE] + 8; i -= 8) {
		printf("\t[SP + 0x%04lx]: %lld\n", i - &spy->memory[SIZE_CODE], *(spy_int *)i);
	}
	printf("FLAGS: \n");
	printf("\tEQ:   %d\n", (spy->flags & FLAG_EQ) != 0);
	printf("\tNEQ:  %d\n", !(spy->flags & FLAG_EQ));
	printf("\tGT:   %d\n", (spy->flags & FLAG_GT) != 0);
	printf("\tGE:   %d\n", !(spy->flags & FLAG_LT));
	printf("\tLT:   %d\n", (spy->flags & FLAG_LT) != 0);
	printf("\tLE:   %d\n", !(spy->flags & FLAG_GT));
	printf("\tZ:    %d\n", (spy->flags & FLAG_Z) != 0);
	printf("\tNZ:   %d\n", !(spy->flags & FLAG_Z));
	printf("\tS:    %d\n", (spy->flags & FLAG_S) != 0);
	printf("\tNS:   %d\n", !(spy->flags & FLAG_S));
}

/*
 *
 * note: there is no differentiation between code and data... the
 * first instructions that is executed is whatever is at code[0]...
 * therefore, the first instruction should (almost) always be
 * some sort of jump instruction to an entry point
 */
void
spy_execute(const char* filename) {
	
	/* read file contents into code */
	uint8_t* code;
	FILE* handle;
	spy_int flen;

	handle = fopen(filename, "rb");
	if (!handle) {
		spy_die("couldn't read bytecode from '%s'", filename);
	}
	fseek(handle, 0, SEEK_END);
	flen = ftell(handle);
	rewind(handle);
	code = malloc(flen);
	fread(code, 1, flen, handle);
	fclose(handle);

	/* copy code into memory */
	memcpy(&spy->memory[0], code, flen);

	/* initialize registers */
	spy->code = code;
	spy->ip = &spy->memory[0];
	spy->sp = &spy->memory[SIZE_CODE]; /* stack grows up */
	spy->bp = &spy->memory[SIZE_CODE];
	
	/* other stuff */
	uint8_t opcode;

	/* NOTES
	 *
	 * 1. for now, jump instructions are relative to the
	 * start of the code section (INCLUDING the data section)
	 * e.g. ip becomes &code[addr];
	 * 
	 *
	 */

	/* some useful macros for instructions */

	#define JMPCOND(cond) \
		{ \
			spy_int addr = spy_code_int64(); \
			if ((cond)) { \
				spy->ip = &code[addr]; \
			} \
		}

	#define CJMPCOND(cond) \
		{ \
			spy_int addr = spy_pop_int(spy); \
			if ((cond)) { \
				spy->ip = &code[addr]; \
			} \
		}

	#define INTARITH(op) \
		{ \
			spy_int b = spy_pop_int(spy); \
			spy_int a = spy_pop_int(spy); \
			spy_push_int(spy, a op b); \
		}
	
	#define FLOATARITH(op) \
		{ \
			spy_float b = spy_pop_float(spy); \
			spy_float a = spy_pop_float(spy); \
			spy_push_float(spy, a op b); \
		}

	#define CMPTYPE(type) \
		{ \
			spy_ ## type b = spy_pop_ ## type(spy); \
			spy_ ## type a = spy_pop_ ## type(spy); \
			if (a == b) { \
				spy->flags |= FLAG_EQ; \
				spy->flags &= ~(FLAG_LT | FLAG_GT); \
			} else if (a > b) { \
				spy->flags |= FLAG_GT; \
				spy->flags &= ~(FLAG_LT | FLAG_EQ); \
			} else { \
				spy->flags |= FLAG_LT; \
				spy->flags &= ~(FLAG_GT | FLAG_EQ); \
			} \
			if (a - b > 0) { \
				spy->flags &= ~FLAG_S; \
			} else { \
				spy->flags |= FLAG_S; \
			} \
		} 

	#define TESTTYPE(type) \
		{ \
			spy_ ## type a = spy_pop_ ## type(spy); \
			if (a == 0) { \
				spy->flags |= FLAG_Z; \
			} else { \
				spy->flags &= ~FLAG_Z; \
			} \
			if (a < 0) { \
				spy->flags |= FLAG_S; \
			} else { \
				spy->flags &= ~FLAG_S; \
			} \
		}

	#define PUSHFLAG(flag) spy_push_int(spy, (spy->flags & (flag)) != 0)
	
	#define PUSHNOTFLAG(flag) spy_push_int(spy, !(spy->flags & (flag)))
	
	uint64_t instructions = 0;			

	/* go */
	do {
		/* check for stack overflow before executing the next instruction...
		 * note: 24 is the maximum number of stack space that an instruction requires */
		if (&spy->memory[SIZE_STACK + SIZE_CODE] - spy->sp <= 24) {
			spy_die("stack overflow");
		}

		opcode = spy_code_int8();
		instructions++;

		switch (opcode) {
			/* NOP */
			case 0x00: 
				break;

			/* ICONST */
			case 0x01: 
				spy_push_int(spy, spy_code_int64());
				break;

			/* ICMP */
			case 0x02: 
				CMPTYPE(int);
				break;
			

			/* ITEST */
			case 0x03: 
				TESTTYPE(int);
				break;
			
			/* JE */	
			case 0x04: 
				JMPCOND(spy->flags & FLAG_EQ);
				break;
			
			/* JNE */
			case 0x05:
				JMPCOND(!(spy->flags & FLAG_EQ));
				break;

			/* JGT */
			case 0x06:
				JMPCOND(spy->flags & FLAG_GT);
				break;
	
			/* JGE */
			case 0x07:
				JMPCOND(!(spy->flags & FLAG_LT));
				break;

			/* JLT */
			case 0x08:
				JMPCOND(spy->flags & FLAG_LT);
				break;
			
			/* JLE */
			case 0x09:
				JMPCOND(!(spy->flags & FLAG_GT));
				break;

			/* JZ */
			case 0x0A:
				JMPCOND(spy->flags & FLAG_Z);
				break;
			
			/* JNZ */
			case 0x0B:
				JMPCOND(!(spy->flags & FLAG_Z));
				break;

			/* JS */
			case 0x0C:
				JMPCOND(spy->flags & FLAG_S);
				break;

			/* JNS */
			case 0x0D:
				JMPCOND(!(spy->flags & FLAG_S));
				break;
	
			/* JMP */
			case 0x0E:
				JMPCOND(1); /* will be optimized */
				break;

			/* CJEQ */	
			case 0x0F: 
				CJMPCOND(spy->flags & FLAG_EQ);
				break;
			
			/* CJNEQ */
			case 0x10:
				CJMPCOND(!(spy->flags & FLAG_EQ));
				break;

			/* CJGT */
			case 0x11:
				CJMPCOND(spy->flags & FLAG_GT);
				break;
	
			/* CJGE */
			case 0x12:
				CJMPCOND(!(spy->flags & FLAG_GT));
				break;

			/* CJLT */
			case 0x13:
				CJMPCOND(spy->flags & FLAG_LT);
				break;
			
			/* CJLE */
			case 0x14:
				CJMPCOND(!(spy->flags & FLAG_GT));
				break;

			/* CJZ */
			case 0x15:
				CJMPCOND(spy->flags & FLAG_Z);
				break;
			
			/* CJNZ */
			case 0x16:
				CJMPCOND(!(spy->flags & FLAG_Z));
				break;

			/* CJS */
			case 0x17:
				CJMPCOND(spy->flags & FLAG_S);
				break;

			/* CJNS */
			case 0x18:
				CJMPCOND(!(spy->flags & FLAG_S));
				break;

			/* CJMP */
			case 0x19:
				CJMPCOND(1); /* will be optimized */
				break;

			/* IADD */
			case 0x1A:
				INTARITH(+);
				break;

			/* ISUB */
			case 0x1B:
				INTARITH(-);
				break;

			/* IMUL */
			case 0x1C:
				INTARITH(*);
				break;

			/* IDIV */
			case 0x1D:
				INTARITH(/);
				break;

			/* SHL */
			case 0x1E:
				INTARITH(<<);
				break;

			/* SHR */
			case 0x1F:
				INTARITH(>>);
				break;

			/* AND */
			case 0x20:
				INTARITH(&);
				break;

			/* OR */
			case 0x21:
				INTARITH(|);
				break;

			/* XOR */
			case 0x22:
				INTARITH(^);
				break;

			/* CALL */
			case 0x23: {
				spy_int addr = spy_code_int64();
				spy_int nargs = spy_code_int64();
				/* save things on stack */
				spy_push_int(spy, (intptr_t)spy->ip);	/* save ip */
				spy_push_int(spy, (intptr_t)spy->bp);	/* save bp */
				spy_push_int(spy, (spy_int)nargs);  /* save nargs */
				spy->bp = spy->sp;
				spy->ip = &code[addr];
				break;
			}
			
			/* CCALL (computed call, NOT C-func call) */
			case 0x24: {
				spy_int addr = spy_pop_int(spy);
				spy_int nargs = spy_code_int64();
				/* save things on stack */
				spy_push_int(spy, (intptr_t)spy->ip);	/* save ip */
				spy_push_int(spy, (intptr_t)spy->bp);	/* save bp */
				spy_push_int(spy, (spy_int)nargs);  /* save nargs */
				spy->bp = spy->sp;
				spy->ip = &code[addr];
				break;	
			}

			/* CFCALL (c-func call) */
			case 0x25: {
				char* cf_name = (char *)&code[spy_code_int64()];
				spy_int nargs = spy_code_int64();
				SpyCFunc* cfunc = NULL;
				for (SpyCFuncList* i = spy->cfuncs; i; i = i->next) {
					if (!i->cfunc) {
						break;
					}
					if (!strcmp(i->cfunc->name, cf_name)) {
						cfunc = i->cfunc;
						break;
					}	
				}
				if (!cfunc) {
					spy_die("unknown c-function '%s'", cf_name);
				}
				cfunc->f(spy);
				break;
			}

			/* IRET */
			case 0x26: {
				spy_int retval, nargs;
				retval = spy_pop_int(spy);
				spy->sp = spy->bp;
				nargs = spy_pop_int(spy);
				spy->bp = (uint8_t *)spy_pop_int(spy);
				spy->ip = (uint8_t *)spy_pop_int(spy);
				spy->sp -= nargs * 8;
				spy_push_int(spy, retval);
				break;
			}

			/* EXIT */
			case 0x27:
				printf("INSTRUCTIONS EXECUTED: %llu\n", instructions);
				return;

			/* IDER */
			case 0x28:
				spy_push_int(spy, *(spy_int *)&spy->memory[spy_pop_int(spy)]);
				break;

			/* BDER */
			case 0x29:
				spy_push_byte(spy, spy->memory[spy_pop_int(spy)]);
				break;

			/* ISAVE */
			case 0x2A: {
				spy_int value = spy_pop_int(spy);
				spy_int addr = spy_pop_int(spy);
				spy_save_int(spy, addr, value);	
				break;
			}
				
			/* BSAVE */
			case 0x2B: {
				spy_byte value = spy_pop_byte(spy);
				spy_int addr = spy_pop_int(spy);
				spy_save_byte(spy, addr, value);
				break;
			}

			/* RES */
			case 0x2C:
				spy->sp += spy_code_int64()*8;
				break;

			/* IINC */
			case 0x2D:
				spy_push_int(spy, spy_pop_int(spy) + spy_code_int64());
				break;

			/* POP */
			case 0x2E:
				spy_pop_int(spy); /* type of pop is irrelevant */
				break;

			/* IARG */
			case 0x2F:
				spy_push_int(spy, *(spy_int *)&spy->bp[-3*8 - spy_code_int64()*8]);
				break;
			
			/* BARG */
			case 0x30:
				spy_push_byte(spy, spy->bp[-3*8 - spy_code_int64()*8]);
				break;

			/* LEA */
			case 0x31:
				spy_push_int(spy, (spy_int)(&spy->bp[8 + spy_code_int64()*8] - spy->memory));
				break;

			/* AISAVE (absolute integer save) */
			case 0x32: {
				spy_int value = spy_pop_int(spy);
				spy_int addr = spy_code_int64();
				spy_save_int(spy, addr, value);	
				break;
			}
				
			/* ABSAVE */
			case 0x33: {	
				spy_byte value = spy_pop_byte(spy);
				spy_int addr = spy_code_int64();
				spy_save_byte(spy, addr, value);
				break;
			}

			/* AIDER */
			case 0x34: 
				spy_push_int(spy, spy_mem_int(spy, spy_code_int64()));
				break;

			/* ABDER */
			case 0x35:
				spy_push_byte(spy, spy_mem_int(spy, spy_code_int64()));
				break;
			
			/* MALLOC */
			case 0x36: {
				spy_int requested_bytes = spy_pop_int(spy);
				MemoryBlockList* new_list = malloc(sizeof(MemoryBlockList));
				new_list->next = NULL;
				new_list->prev = NULL;
				new_list->block = malloc(sizeof(MemoryBlock));
				new_list->block->bytes = requested_bytes + (MALLOC_CHUNK % requested_bytes);
				MemoryBlockList* head = spy->memory_map;
				MemoryBlockList* next = head->next;
				MemoryBlockList* tail = NULL;
				int found_slot = 0;
				for (MemoryBlockList* i = spy->memory_map; i->next; i = i->next) {
					spy_int pending_addr = i->block->addr + i->block->bytes;
					spy_int delta = i->next->block->addr - pending_addr;
					/* is there enough space to fit the block? */
					if (new_list->block->bytes <= delta) {
						/* found enough space: */
						new_list->next = i->next;
						new_list->prev = i;
						i->next->prev = new_list;
						i->next = new_list->next;
						new_list->block->addr = pending_addr;
						found_slot = 1;
						break;
					}
					if (!i->next->next) {
						tail = i->next;
					}
				}
				/* space wasn't found... append to tail block */
				if (!found_slot) {
					if (tail) {
						tail->next = new_list;
						new_list->prev = tail;
						new_list->block->addr = tail->block->addr + tail->block->bytes;
					} else {
						new_list->block->addr = head->block->addr;
						new_list->prev = head;
						head->next = new_list;
					}
				}
				/* !!!! out of memory !!!! */
				/* TODO defragment when OOM and retry */
				if (new_list->block->addr + new_list->block->bytes >= SIZE_MEMORY) {
					new_list->prev->next = NULL;
					free(new_list->block);
					free(new_list);
					spy_push_int(spy, 0);
				} else {
					spy_push_int(spy, new_list->block->addr);
				}
				break;
			}

			/* FREE */
			case 0x37:
				break;

			/* VRET */
			case 0x38: {
				spy_int nargs;
				spy->sp = spy->bp;
				nargs = spy_pop_int(spy);
				spy->bp = (uint8_t *)spy_pop_int(spy);
				spy->ip = (uint8_t *)spy_pop_int(spy);
				spy->sp -= nargs * 8;
				break;
			}

			/* ILOCALL */
			case 0x39:
				spy_push_int(spy, *(spy_int *)&spy->bp[8 + spy_code_int64()*8]);
				break;
			
			/* BLOCALL */
			case 0x3A: 
				spy_push_byte(spy, spy->bp[8 + spy_code_int64()*8]);
				break;
			
			/* ILOCALS */
			case 0x3B:
				*(spy_int *)&spy->bp[8 + spy_code_int64()*8] = spy_pop_int(spy);
				break;

			/* BLOCALS */
			case 0x3C:
				spy->bp[8 + spy_code_int64()*8] = spy_pop_byte(spy);
				break;

			/* PE */
			case 0x3D:
				PUSHFLAG(FLAG_EQ);
				break;
				
			/* PNE */
			case 0x3E:	
				PUSHNOTFLAG(FLAG_EQ);
				break;

			/* PGT */
			case 0x3F:
				PUSHFLAG(FLAG_GT);
				break;
				
			/* PGE */
			case 0x40:	
				PUSHNOTFLAG(FLAG_LT);
				break;

			/* PLT */
			case 0x41:
				PUSHFLAG(FLAG_LT);
				break;
				
			/* PLE */
			case 0x42:	
				PUSHNOTFLAG(FLAG_GT);
				break;

			/* PZ */
			case 0x43:
				PUSHFLAG(FLAG_Z);
				break;
				
			/* PNZ */
			case 0x44:	
				PUSHNOTFLAG(FLAG_Z);
				break;

			/* PS */
			case 0x45:
				PUSHFLAG(FLAG_S);
				break;

			/* PNS */
			case 0x46:
				PUSHNOTFLAG(FLAG_S);
				break;

			/* FCONST */
			case 0x47:
				spy_push_float(spy, spy_code_float());
				break;
			
			/* FCMP */
			case 0x48:
				CMPTYPE(float);
				break;

			/* FTEST */
			case 0x49:
				TESTTYPE(float);
				break;

			/* FADD */
			case 0x4A:
				FLOATARITH(+);
				break;

			/* FSUB */
			case 0x4B:
				FLOATARITH(-);
				break;
			
			/* FMUL */
			case 0x4C:
				FLOATARITH(*);
				break;

			/* FDIV */
			case 0x4D:
				FLOATARITH(/);
				break;

			/* FINC */
			case 0x4E:
				spy_push_float(spy, spy_pop_float(spy) + spy_code_float());
				break;

			/* FRET */
			case 0x4F: {
				spy_int nargs;
				spy_float retval;
				retval = spy_pop_float(spy);
				spy->sp = spy->bp;
				nargs = spy_pop_int(spy);
				spy->bp = (uint8_t *)spy_pop_int(spy);
				spy->ip = (uint8_t *)spy_pop_int(spy);
				spy->sp -= nargs * 8;
				spy_push_float(spy, retval);
				break;
			}

			/* FLOCALL */
			case 0x50:
				spy_push_float(spy, *(spy_float *)&spy->bp[8 + spy_code_int64()*8]);
				break;

			/* FLOCALS */
			case 0x51:
				*(spy_float *)&spy->bp[8 + spy_code_int64()*8] = spy_pop_float(spy);
				break;

			/* AFSAVE */
			case 0x52: {
				spy_float value = spy_pop_int(spy);
				spy_int addr = spy_code_int64();
				spy_save_float(spy, addr, value);	
				break;
			}

			/* AFDER */
			case 0x53: 
				spy_push_float(spy, spy_mem_float(spy, spy_code_int64()));
				break;

			/* FDER */
			case 0x54:
				spy_push_float(spy, *(spy_float *)&spy->memory[spy_pop_int(spy)]);
				break;

			/* FARG */
			case 0x55:
				spy_push_float(spy, *(spy_float *)&spy->bp[-3*8 - spy_code_int64()*8]);
				break;

			/* ITOF */
			case 0x56:
				spy_push_float(spy, (spy_float)spy_pop_int(spy));
				break;

			/* FTOI */
			case 0x57:
				spy_push_int(spy, (spy_int)spy_pop_float(spy));
				break;
			
			/* DUP */
			case 0x58:
				spy_push_int(spy, spy_top_int(spy));
				break;

			/* ILOG */
			case 0xFD:
				printf("%lld\n", spy_pop_int(spy));
				break;

			/* BLOG */
			case 0xFE:
				printf("%d\n", spy_pop_byte(spy));
				break;

			/* FLOG */
			case 0xFF:
				printf("%f\n", spy_pop_float(spy));
				break;
		}

	} while (opcode != 0x00);
}
