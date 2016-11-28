#ifndef SPYLIB_H
#define SPYLIB_H

#include "vm.h"

void spy_push_int(SpyState*, spy_integer);
spy_integer spy_pop_int(SpyState*);
const spy_string spy_gets(SpyState*, spy_integer);

#endif
