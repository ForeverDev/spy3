#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "vm.h"
#include "spylib.h"
#include "capi_load.h"

static SpyState* spy = NULL;

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

static spy_integer
spy_code_int() {
	spy_integer ret = *(spy_integer *)spy->ip;
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
		printf("\t[SP + 0x%04x]: %lld\n", i - &spy->memory[SIZE_CODE], *(spy_integer *)i);
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
	spy_integer flen;

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
			spy_integer addr = spy_code_int(); \
			if ((cond)) { \
				spy->ip = &code[addr]; \
			} \
		}

	#define CJMPCOND(cond) \
		{ \
			spy_integer addr = spy_pop_int(spy); \
			if ((cond)) { \
				spy->ip = &code[addr]; \
			} \
		}

	#define INTARITH(op) \
		{ \
			spy_integer b = spy_pop_int(spy); \
			spy_integer a = spy_pop_int(spy); \
			spy_push_int(spy, a op b); \
		}

	/* go */
	do {
		/* check for stack overflow before executing the next instruction...
		 * note: 24 is the maximum number of stack space that an instruction requires */
		if (&spy->memory[SIZE_STACK + SIZE_CODE] - spy->sp <= 24) {
			spy_die("stack overflow");
		}

		opcode = spy_code_int8(spy);

		switch (opcode) {
			/* NOP */
			case 0x00: 
				break;

			/* ICONST */
			case 0x01: 
				spy_push_int(spy, spy_code_int());
				break;

			/* ICMP */
			case 0x02: {
				spy_integer b = spy_pop_int(spy);
				spy_integer a = spy_pop_int(spy);
				/* set flags accordingly... */
				if (a == b) {
					spy->flags |= FLAG_EQ;
					spy->flags &= ~(FLAG_LT | FLAG_GT);
				} else if (a > b) {
					spy->flags |= FLAG_GT;
					spy->flags &= ~(FLAG_LT | FLAG_EQ);
				} else {
					/* a < b */
					spy->flags |= FLAG_LT;
					spy->flags &= ~(FLAG_GT | FLAG_EQ);
				}
				/* sign flag set based on (a - b) cuz why not */
				if (a - b > 0) {
					spy->flags &= ~FLAG_S;
				} else {
					spy->flags |= FLAG_S;
				}
				/* cant do anything with Z or NZ flags... */
				break;
			}

			/* ITEST */
			case 0x03: {
				spy_integer a = spy_pop_int(spy);
				if (a == 0) {
					spy->flags |= FLAG_Z;
				} else {
					spy->flags &= ~FLAG_Z;
				}
				if (a < 0) {
					spy->flags |= FLAG_S;
				} else {
					spy->flags &= ~FLAG_S;
				}
				break;
			}
			
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
				JMPCOND(!(spy->flags & FLAG_GT));
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
				spy_integer addr = spy_code_int();
				spy_integer nargs = spy_code_int();
				/* save things on stack */
				spy_push_int(spy, (intptr_t)spy->ip);	/* save ip */
				spy_push_int(spy, (intptr_t)spy->bp);	/* save bp */
				spy_push_int(spy, (spy_integer)nargs);  /* save nargs */
				spy->bp = spy->sp;
				spy->ip = &code[addr];
				break;
			}
			
			/* CCALL (computed call, NOT C-func call) */
			case 0x24: {
				spy_integer addr = spy_pop_int(spy);
				spy_integer nargs = spy_code_int();
				/* save things on stack */
				spy_push_int(spy, (intptr_t)spy->ip);	/* save ip */
				spy_push_int(spy, (intptr_t)spy->bp);	/* save bp */
				spy_push_int(spy, (spy_integer)nargs);  /* save nargs */
				spy->bp = spy->sp;
				spy->ip = &code[addr];
				break;	
			}

			/* CFCALL (c-func call) */
			case 0x25: {
				char* cf_name = (char *)&code[spy_code_int()];
				spy_integer nargs = spy_code_int();
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
				spy_integer retval, nargs;
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
				return;

			/* IDER */
			case 0x28:
				spy_push_int(spy, spy_mem_int(spy, spy_pop_int(spy)));
				break;

			/* BDER */
			case 0x29:
				spy_push_byte(spy, spy_mem_byte(spy, spy_pop_int(spy)));
				break;

			/* ISAVE */
			case 0x2A: {
				spy_integer value = spy_pop_int(spy);
				spy_integer addr = spy_pop_int(spy);
				spy_save_int(spy, addr, value);	
				break;
			}
				
			/* BSAVE */
			case 0x2B: {
				spy_byte value = spy_pop_byte(spy);
				spy_integer addr = spy_pop_int(spy);
				spy_save_byte(spy, addr, value);
				break;
			}

			/* RES */
			case 0x2C:
				spy->sp += spy_code_int(spy)*8;
				break;

			/* IINC */
			case 0x2D:
				spy_push_int(spy, spy_pop_int(spy) + spy_code_int());
				break;

			/* POP */
			case 0x2E:
				spy_pop_int(spy); /* type of pop is irrelevant */
				break;

			/* IARG */
			case 0x2F:
				spy_push_int(spy, *(spy_integer *)&spy->bp[-3*8 - spy_code_int()*8]);
				break;
			
			/* BARG */
			case 0x30:
				spy_push_byte(spy, spy->bp[-3*8 - spy_code_int()*8]);
				break;

			/* LEA */
			case 0x31:
				spy_push_int(spy, (spy_integer)(&spy->bp[8 + spy_code_int()*8] - spy->memory));
				break;

			/* AISAVE (integer absolute save) */
			case 0x32: {
				spy_integer value = spy_pop_int(spy);
				spy_integer addr = spy_code_int(spy);
				spy_save_int(spy, addr, value);	
				break;
			}
				
			/* ABSAVE */
			case 0x33: {	
				spy_byte value = spy_pop_byte(spy);
				spy_integer addr = spy_code_int(spy);
				spy_save_byte(spy, addr, value);
				break;
			}

			/* AIDER */
			case 0x34: 
				spy_push_int(spy, spy_mem_int(spy, spy_code_int()));
				break;

			/* ABDER */
			case 0x35:
				spy_push_byte(spy, spy_mem_int(spy, spy_code_int()));
				break;
			
			/* MALLOC */
			case 0x36: {
				spy_integer requested_bytes = spy_pop_int(spy);
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
					spy_integer pending_addr = i->block->addr + i->block->bytes;
					spy_integer delta = i->next->block->addr - pending_addr;
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
				spy_integer nargs;
				spy->sp = spy->bp;
				nargs = spy_pop_int(spy);
				spy->bp = (uint8_t *)spy_pop_int(spy);
				spy->ip = (uint8_t *)spy_pop_int(spy);
				spy->sp -= nargs * 8;
				break;
			}

			/* ILOCALL */
			case 0x39:
				spy_push_int(spy, *(spy_integer *)&spy->bp[8 + spy_code_int()]);
				break;
			
			/* BLOCALL */
			case 0x3A: 
				spy_push_byte(spy, spy->bp[8 + spy_code_int()]);
				break;
			
			/* ILOCALS */
			case 0x3B:
				*(spy_integer *)&spy->bp[8 + spy_code_int()] = spy_pop_int(spy);
				break;

			/* BLOCALS */
			case 0x3C:
				spy->bp[8 + spy_code_int()] = spy_pop_byte(spy);
				break;

			
		}

	} while (opcode != 0x00);
}
