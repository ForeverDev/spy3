#ifndef SPYLIB_H
#define SPYLIB_H

#include "vm.h"

void spy_push_int(SpyState*, spy_integer);
void spy_push_byte(SpyState*, spy_byte);

spy_integer spy_pop_int(SpyState*);
spy_byte spy_pop_byte(SpyState*);

spy_integer spy_mem_int(SpyState*, spy_integer);
spy_byte spy_mem_byte(SpyState*, spy_integer);

void spy_save_int(SpyState*, spy_integer, spy_integer);
void spy_save_byte(SpyState*, spy_integer, spy_byte);

const spy_string spy_gets(SpyState*, spy_integer);

#endif
