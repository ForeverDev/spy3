#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "lex.h"
#include "parse.h"
#include "assemble.h"
#include "generate.h"

int main(int argc, char** argv) {
	
	if (argc <= 1) {
		printf("expected input file\n");
		return 1;
	}

	char* fname = argv[1];
	char* fasm;
	char* fbin;
	char* fspy;
	size_t slen = strlen(fname);
	
	fasm = malloc(slen + 6);
	strcpy(fasm, fname);
	strcat(fasm, ".spys");

	fspy = malloc(slen + 5);
	strcpy(fspy, fname);
	strcat(fspy, ".spy");

	fbin = malloc(slen + 6);
	strcpy(fbin, fname);
	strcat(fbin, ".spyb");
	
	TokenList* tokens = generate_tokens_from_source(fspy);
	ParseState* state = generate_syntax_tree(tokens);
	generate_instructions(state, fasm);

	return 0;

}	
