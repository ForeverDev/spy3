#include <math.h>
#include <stdlib.h>
#include "vm.h"
#include "capi_math.h"
#include "spylib.h"

static spy_int
math_cos(SpyState* spy) {
	spy_push_float(spy, cos(spy_pop_float(spy)));
	return 1;	
}

static spy_int
math_sin(SpyState* spy) {
	spy_push_float(spy, sin(spy_pop_float(spy)));
	return 1;	
}

static spy_int
math_tan(SpyState* spy) {
	spy_push_float(spy, tan(spy_pop_float(spy)));
	return 1;	
}

static spy_int
math_acos(SpyState* spy) {
	spy_push_float(spy, acos(spy_pop_float(spy)));
	return 1;
}

SpyCFunc capi_math[] = {
	
	{"cos", math_cos},
	{"sin", math_sin},
	{"tan", math_tan},
	{NULL, NULL}
	
};
