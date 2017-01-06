#include <stdlib.h>
#include "capi_std.h"
#include "vm.h"
#include "spylib.h"

static spy_int
std_quit(SpyState* spy) {
	spy->bail = 1;
	return 0;
}

SpyCFunc capi_std[] = {
	{"quit", std_quit},
	{NULL, NULL}
};
