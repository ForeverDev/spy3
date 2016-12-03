#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "lex.h"
#include "assemble.h"

int main(int argc, char** argv) {
	
	if (argc <= 1) {
		printf("expected input file\n");
		return 1;
	}

	char* fname = argv[1];
	char* fasm;
	char* fbin;
	size_t slen = strlen(fname);
	
	fasm = malloc(slen + 6);
	strcpy(fasm, fname);
	strcat(fasm, ".spys");

	fbin = malloc(slen + 6);
	strcpy(fbin, fname);
	strcat(fbin, ".spyb");

	/*	
	generate_bytecode(fasm, fbin);
	spy_init();
	spy_execute(fbin);
	*/

	generate_tokens_from_source(argv[1]);

	return 0;

}	
