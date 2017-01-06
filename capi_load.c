#include <stdlib.h>
#include "capi_io.h"
#include "capi_load.h"
#include "capi_std.h"
#include "capi_math.h"

static void
append_cfunc(SpyState* spy, SpyCFunc* cfunc) {
	if (!spy->cfuncs->cfunc) {
		spy->cfuncs->cfunc = cfunc;
	} else {
		SpyCFuncList* new_list = malloc(sizeof(SpyCFuncList));
		new_list->cfunc = cfunc;
		new_list->next = NULL;
		SpyCFuncList* scanner = spy->cfuncs;
		while (scanner->next) {
			scanner = scanner->next;
		}
		scanner->next = new_list;
	}
}

static void
register_array(SpyState* spy, SpyCFunc cfuncs[]) {
	for (SpyCFunc* i = cfuncs; i->name != NULL; i++) {
		append_cfunc(spy, i);
	}
}

void spy_init_capi(SpyState* spy) {
	register_array(spy, capi_io); 
	register_array(spy, capi_std);
	register_array(spy, capi_math);
}
