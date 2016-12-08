#ifndef SPYLIB_H
#define SPYLIB_H

#include "vm.h"

void spy_push_int(SpyState*, spy_int);
void spy_push_byte(SpyState*, spy_byte);
void spy_push_float(SpyState*, spy_float);

spy_int spy_pop_int(SpyState*);
spy_byte spy_pop_byte(SpyState*);
spy_float spy_pop_float(SpyState*);
spy_int spy_top_int(SpyState*);

spy_int spy_mem_int(SpyState*, spy_int);
spy_byte spy_mem_byte(SpyState*, spy_int);
spy_float spy_mem_float(SpyState*, spy_int);

void spy_save_int(SpyState*, spy_int, spy_int);
void spy_save_byte(SpyState*, spy_int, spy_byte);
void spy_save_float(SpyState*, spy_int, spy_float);

const spy_string spy_gets(SpyState*, spy_int);

#endif
