#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "capi_io.h"
#include "spylib.h"

static spy_int 
io_print(SpyState* spy) {
	spy_int format_addr = spy_pop_int(spy);
	spy_string format = spy_gets(spy, format_addr);
	spy_int length = 0; /* total characters printed */
	while (*format) {
		switch (*format) {
			/* NOTE: escape sequences not handeled here, the assembler already dealt with them */
			case '%':
				switch (*(++format)) {
					case 'd':
						length += printf("%lld", spy_pop_int(spy));
						break;
					case 'x': 
						switch (*(++format)) {
							case 'i':
								length += printf("%llx", spy_pop_int(spy));
								break;
							case 'b':
								length += printf("%x", spy_pop_byte(spy));
								break;			
						}
						break;
					case 'f':
						length += printf("%f", spy_pop_float(spy));
						break;
					case 'c':
						fputc(spy_pop_byte(spy), stdout);
						length++;
						break;
					case 's':
						length += printf("%s", spy_gets(spy, spy_pop_int(spy)));
						break;
					case 'b': {
						uint64_t bin;
						int bits;
						switch (*(++format)) {
							case 'i': {
								bin = spy_pop_int(spy); 
								bits = 64;
								break;
							}
							case 'f': {
								spy_float pop = spy_pop_float(spy);
								bin = *(uint64_t *)&pop; 
								bits = 64;
								break;
							}
							case 'b': {
								bin = 0x0 | spy_pop_byte(spy); 
								bits = 8;
								break;
							}
							/* an attempt to catch undefined behavior from a faulty format... */
							default:
								bits = 8;
								bin = 0;
								break;
						}
						for (int i = 0; i < bits; i++) {
							fputc('0' + ((bin >> (bits - i - 1)) & 0x1), stdout);
						}
						length += 64;
						break;
					}
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

static spy_int
io_fputc(SpyState* spy) {
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_byte b = spy_pop_byte(spy);
	fputc(b, fptr);
	return 0;
}

static spy_int
io_fputs(SpyState* spy) {
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_string s = spy_gets(spy, spy_pop_int(spy));
	fputs(s, fptr);
	return 0;
}

static spy_int
io_outc(SpyState* spy) {
	spy_byte c = spy_pop_byte(spy);
	fputc(c, stdout);
	return 0;
}

static spy_int
io_fopen(SpyState* spy) {
	char path[1024] = {0};
	getcwd(path, 1024);
	spy_string fname = spy_gets(spy, spy_pop_int(spy));
	spy_string mode = spy_gets(spy, spy_pop_int(spy));
	strcat(path, "/");
	strcat(path, fname);
	FILE* fptr = fopen(path, mode);
	spy_push_int(spy, (spy_int)fptr);
	return 1;
}

static spy_int
io_fclose(SpyState* spy) {
	fclose((FILE *)spy_pop_int(spy));
	return 0;
}

static spy_int
io_fseek(SpyState* spy) {
	int mode;
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_int mode_raw = spy_pop_int(spy);
	spy_int offset = spy_pop_int(spy);
	mode = (
		mode_raw == 1 ? SEEK_SET :
		mode_raw == 2 ? SEEK_END : SEEK_CUR
	);
	spy_push_int(spy, (spy_int)fseek(fptr, mode, mode)); 
	return 1;
}

static spy_int
io_ftell(SpyState* spy) {
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_push_int(spy, (spy_int)ftell(fptr));
	return 1;
}

static spy_int
io_fgetc(SpyState* spy) {
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_push_byte(spy, (spy_byte)fgetc(fptr));
	return 1;
}

/* NOTE args are (fptr, target, bytes) */
static spy_int
io_fread(SpyState* spy) {
	FILE* fptr = (FILE *)spy_pop_int(spy);
	spy_int target = spy_pop_int(spy);
	spy_int bytes = spy_pop_int(spy);
	spy_push_int(spy, fread(&spy->memory[target], 1, bytes, fptr));
	return 1; 
}

static spy_int
io_flush(SpyState* spy) {
	char c;
	while ((c = getchar()) != '\n' && c != EOF);
	return 0;
}

static spy_int
io_read_int(SpyState* spy) {
	/* TODO make work properly */
	//spy_string format = spy_gets(spy, spy_pop_int(spy));
	spy_int ptr = spy_pop_int(spy);
	int check = scanf("%lld", (spy_int *)&spy->memory[ptr]);
	if (check != 1) {
		*(spy_int *)&spy->memory[ptr] = 0;
		spy_push_int(spy, 0);
	} else {
		spy_push_int(spy, 1);
	}
	io_flush(spy);
	return 1;
}

static spy_int
io_read_float(SpyState* spy) {
	spy_int ptr = spy_pop_int(spy);
	int check = scanf("%lf", (spy_float *)&spy->memory[ptr]);
	if (check != 1) {
		*(spy_float *)&spy->memory[ptr] = 0.0;
		spy_push_int(spy, 0);
	} else {
		spy_push_int(spy, 1);	
	}
	io_flush(spy);
	return 1;
}

static spy_int
io_read_string(SpyState* spy) {
	spy_int ptr = spy_pop_int(spy);
	spy_int size = spy_pop_int(spy);
	char* str = (char *)&spy->memory[ptr];
	if (fgets(str, size, stdin)) {
		/* strip newline at end if needed */
		size_t slen = strlen(str) - 1;
		if (*str && str[slen] == '\n') {
			str[slen] = 0;
		}
		spy_push_int(spy, ptr);
	} else {
		spy_push_int(spy, 0);
	}
	return 1;
}

SpyCFunc capi_io[] = {
	{"print", io_print},
	{"outc", io_outc},
	{"fopen", io_fopen},
	{"fclose", io_fclose},
	{"fseek", io_fseek},
	{"ftell", io_ftell},
	{"fgetc", io_fgetc},
	{"fputc", io_fputc},
	{"fputs", io_fputs},
	{"fread", io_fread},
	{"read_int", io_read_int},
	{"read_float", io_read_float},
	{"read_string", io_read_string},
	{"flush", io_flush},
	{NULL, NULL}
};
