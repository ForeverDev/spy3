#include <math.h>
#include "capi_math.h"

static spy_integer
math_cos(SpyState* spy) {
	spy_push_float(spy, cos(spy_pop_float(spy)));
	return 1;	
}

static spy_integer
math_sin(SpyState* spy) {
	spy_push_float(spy, sin(spy_pop_float(spy)));
	return 1;	
}

static spy_integer
math_tan(SpyState* spy) {
	spy_push_float(spy, tan(spy_pop_float(spy)));
	return 1;	
}

static spy_integer
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
