#include "spylib.h"

void
spy_push_int(SpyState* spy, spy_integer value) {
	spy->sp += 8;
	*(spy_integer *)spy->sp = value;
}

spy_integer
spy_pop_int(SpyState* spy) {
	spy_integer ret = *(spy_integer *)spy->sp;
	spy->sp -= 8;
	return ret;
}

const spy_string
spy_gets(SpyState* spy, spy_integer addr) {
	return &spy->memory[addr];
}
