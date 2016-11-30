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
					case 'x':
						length += printf("%llx", spy_pop_int(spy));
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

static spy_integer
io_fopen(SpyState* spy) {
	spy_string fname = spy_gets(spy, spy_pop_int(spy));
	spy_string mode = spy_gets(spy, spy_pop_int(spy));
	FILE* fptr = fopen(fname, mode);
	spy_push_int(spy, (spy_integer)fptr);
	return 1;
}

static spy_integer
io_fseek(SpyState* spy) {
	int mode;
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_integer mode_raw = spy_pop_int(spy);
	spy_integer offset = spy_pop_int(spy);
	mode = (
		mode_raw == 1 ? SEEK_SET :
		mode_raw == 2 ? SEEK_END : SEEK_CUR
	);
	spy_push_int(spy, (spy_integer)fseek(fptr, mode, mode)); 
	return 1;
}

static spy_integer
io_ftell(SpyState* spy) {
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_push_int(spy, (spy_integer)ftell(fptr));
	return 1;
}

static spy_integer
io_fgetc(SpyState* spy) {
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_push_byte(spy, (spy_byte)fgetc(fptr));
	return 1;
}

/* NOTE args are (fptr, target, bytes) */
static spy_integer
io_fread(SpyState* spy) {
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_integer target = spy_pop_int(spy);
	spy_integer bytes = spy_pop_int(spy);
	spy_push_int(spy, fread(&spy->memory[target], 1, bytes, fptr));
	return 1; 
}

SpyCFunc capi_io[] = {
	{"print", io_print},
	{"outc", io_outc},
	{"fopen", io_fopen},
	{"fseek", io_fseek},
	{"ftell", io_ftell},
	{"fgetc", io_fgetc},
	{"fread", io_fread},
	{NULL, NULL}
};
