#include <stdio.h>
#include <stdlib.h>
#include "capi_io.h"
#include "spylib.h"

static spy_integer spy_capi_print(SpyState* spy) {
	const spy_string format = spy_gets(spy, spy_pop_int(spy));
	printf("FORMAT: %s\n", format);
	return 0;
}

SpyCFunc capi_io_functions[] = {
	{"print", spy_capi_print},
	{NULL, NULL}
};
