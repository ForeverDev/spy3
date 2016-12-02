#include <stdio.h>
#include "spylib.h"

/* PUSH FUNCTIONS */
void
spy_push_int(SpyState* spy, spy_int value) {
	spy->sp += 8;
	*(spy_int *)spy->sp = value;
}

void
spy_push_byte(SpyState* spy, spy_byte value) {
	spy->sp += 8;
	*(spy_int *)spy->sp = (spy_int)value;
}

void
spy_push_float(SpyState* spy, spy_float value) {
	spy->sp += 8;
	*(spy_float *)spy->sp = value;
}

/* POP FUNCTIONS */
spy_int
spy_pop_int(SpyState* spy) {
	spy_int ret = *(spy_int *)spy->sp;
	spy->sp -= 8;
	return ret;
}

spy_byte
spy_pop_byte(SpyState* spy) {
	spy_byte ret = *spy->sp;
	spy->sp -= 8;
	return ret;
}

spy_float
spy_pop_float(SpyState* spy) {
	spy_float ret = *(spy_float *)spy->sp;
	spy->sp -= 8;
	return ret;
}

/* MEMORY FUNCTIONS (LOAD) */
spy_int
spy_mem_int(SpyState* spy, spy_int addr) {
	return *(spy_int *)&spy->memory[addr];
}

spy_byte
spy_mem_byte(SpyState* spy, spy_int addr) {
	return spy->memory[addr];
}

spy_float
spy_mem_float(SpyState* spy, spy_int addr) {
	return *(spy_float *)&spy->memory[addr];
}

/* MEMORY FUNCTIONS (SAVE) */
void
spy_save_int(SpyState* spy, spy_int addr, spy_int value) {
	*(spy_int *)&spy->memory[addr] = value;
}

void
spy_save_byte(SpyState* spy, spy_int addr, spy_byte value) {
	spy->memory[addr] = value;
}

void
spy_save_float(SpyState* spy, spy_int addr, spy_float value) {
	*(spy_float *)&spy->memory[addr] = value;
}

/* MISC */
const spy_string
spy_gets(SpyState* spy, spy_int addr) {
	return (spy_string)&spy->memory[addr];
}

