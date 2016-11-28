#include "spylib.h"

void
spy_push_int(SpyState* spy, spy_integer value) {
	spy->sp += 8;
	*(spy_integer *)spy->sp = value;
}

void
spy_push_byte(SpyState* spy, spy_byte value) {
	spy->sp += 8;
	*spy->sp = value;
}

spy_integer
spy_pop_int(SpyState* spy) {
	spy_integer ret = *(spy_integer *)spy->sp;
	spy->sp -= 8;
	return ret;
}

spy_byte
spy_pop_byte(SpyState* spy) {
	spy_byte ret = *spy->sp;
	spy->sp -= 8;
	return ret;
}

const spy_string
spy_gets(SpyState* spy, spy_integer addr) {
	return &spy->memory[addr];
}

spy_integer
spy_mem_int(SpyState* spy, spy_integer addr) {
	return *(spy_integer *)&spy->memory[addr];
}

spy_byte
spy_mem_byte(SpyState* spy, spy_integer addr) {
	return spy->memory[addr];
}

void
spy_save_int(SpyState* spy, spy_integer addr, spy_integer value) {
	*(spy_integer *)&spy->memory[addr] = value;
}

void
spy_save_byte(SpyState* spy, spy_integer addr, spy_byte value) {
	spy->memory[addr] = value;
}
