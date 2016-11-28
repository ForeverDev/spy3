#include <stdio.h>
#include <stdlib.h>
#include "capi_io.h"
#include "spylib.h"

static spy_integer 
io_print(SpyState* spy) {
	spy_string format = spy_gets(spy, spy_pop_int(spy));
	spy_integer length = 0; /* total characters printed */
	while (*format) {
		switch (*format) {
			/* NOTE: escape sequences not handeled here, the assembler already dealt with them */
			case '%':
				switch (*(++format)) {
					case 'd':
						length += printf("%lld", spy_pop_int(spy));
						break;
					case 'f':
						/* TODO implement float format */
						break;
					case 'c':
						fputc(spy_pop_byte(spy), stdout);
						length++;
						break;
					case 's':
						length += printf("%s", spy_gets(spy, spy_pop_int(spy)));
						break;
				}
				break;
			default:
				length++;
				fputc(*format, stdout);
				break;
		}
		format++;
	}
	spy_push_int(spy, length);
	return 1;
}

static spy_integer
io_outc(SpyState* spy) {
	spy_byte c = spy_pop_byte(spy);
	fputc(c, stdout);
	return 0;
}

SpyCFunc capi_io[] = {
	{"print", io_print},
	{"outc", io_outc},
	{NULL, NULL}
};
