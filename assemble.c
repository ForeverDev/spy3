#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "asmlex.h"
#include "assemble.h"
#include "vm.h"

typedef struct Assembler Assembler;
typedef struct Label Label;
typedef struct LabelList LabelList;

struct Assembler {
	AsmTokenList* tokens;
	LabelList* labels; /* defined on first pass */
	Label* current_global; /* for accessing nested locals */
	FILE* handle; /* output handle */
	const char* inname;
};

struct Label {
	char* name;
	int64_t addr; /* relative to code start */
	LabelList* children; /* locals are nested in globals, for local this == NULL */
};

struct LabelList {
	Label* label;
	LabelList* next;
};

static enum AsmTokenType
peektype(Assembler* A) {
	if (!A->tokens->next) {
		return ASMTOK_NOTYPE;
	}
	return A->tokens->next->token->type;
}

static void
asm_die(Assembler* A, const char* msg, ...) {
	va_list args;
	va_start(args, msg);
	printf("\n\n*** SPYRE ASSEMBLER ERROR ***\n\tmessage: ");
	vprintf(msg, args);
	printf("\n\tfile: %s\n", A->inname);
	printf("\tline: %d\n\n\n", A->tokens->token ? A->tokens->token->line : 1);
	va_end(args); /* is this really necessary? */
	exit(1);
}

static void
register_label(Assembler* A, const char* name, int64_t addr) {
	int is_global = name[0] != '.';
	Label* new = malloc(sizeof(Label));
	new->name = malloc(strlen(name) + 1);
	strcpy(new->name, name);
	new->addr = addr;
	new->children = NULL;
	if (!is_global && !A->labels->label) {
		asm_die(A, "a local label must come after a global label");
	} else if (!A->labels->label) {
		A->labels->label = new;
		A->current_global = new;
	} else if (is_global) {
		LabelList* new_list = malloc(sizeof(LabelList));
		new_list->label = new;
		new_list->next = NULL;
		/* append normally */
		LabelList* scanner = A->labels;
		while (scanner->next) {
			scanner = scanner->next;
		}
		scanner->next = new_list;
		A->current_global = new;
	} else if (!is_global) {
		/* proper local label */
		LabelList* new_list = malloc(sizeof(LabelList));
		new_list->label = new;
		new_list->next = NULL;

		if (!A->current_global->children) {
			A->current_global->children = new_list;
		} else {
			LabelList* scanner = A->current_global->children;
			while (scanner->next) {
				scanner = scanner->next;
			}
			scanner->next = new_list;
		}
	}
}

static Label*
get_label(Assembler* A, const char* name) {
	/* check locals if starts with '.' */
	if (name[0] == '.') {
		if (!A->current_global) {
			goto die;
		}
		for (LabelList* i = A->current_global->children; i; i = i->next) {
			if (!strcmp(i->label->name, name)) {
				return i->label;
			}
		}
		goto die;
	} else {
		if (!A->labels->label) {
			goto die;
		}
		for (LabelList* i = A->labels; i; i = i->next) {
			if (!strcmp(i->label->name, name)) {
				return i->label;
			}
		}
		goto die;
	}
	die: /* nasty label but it makes things easier */
	asm_die(A, "unknown label '%s'", name);
	return NULL;
}

void generate_bytecode(const char* infile, const char* outfile) {

	/* useful token pointers */
	AsmTokenList* head = NULL;
	AsmTokenList* code_start = NULL;

	Assembler A;
	A.inname = infile;
	A.tokens = generate_tokens(infile);
	A.labels = malloc(sizeof(LabelList));
	A.labels->label = NULL;
	A.labels->next = NULL;
	A.current_global = NULL;
	A.handle = fopen(outfile, "wb");
	if (!A.handle) {
		asm_die(&A, "couldn't open '%s' for writing", outfile);
	} 
	/* empty input file? quit */
	if (!A.tokens->token) {
		fclose(A.handle);
		return;
	}

	const SpyInstruction* ins;
	int64_t cindex = 0; /* code index... how many bytes would be written so far */

	head = A.tokens;

	while (A.tokens) {
		if (A.tokens->token->type == ASMTOK_IDENTIFIER) {
			const char* on_word = A.tokens->token->sval;
			/* if on an instruction, increment index by 1 + operand_size */
			if ((ins = spy_get_instruction(on_word))) {
				cindex++; /* one byte for instruction */
				if (ins->operands[0] == OP_NONE) {
					A.tokens = A.tokens->next;
					continue;
				}
				if (A.tokens->next) {
					A.tokens = A.tokens->next; /* move to first operand */
				}
				for (const enum InstructionOperand* i = ins->operands; *i != OP_NONE; i++) {
					switch (*i) {
						case OP_INT64:
						case OP_FLOAT64:
							cindex += 8;
							break;
						case OP_UINT32:
							cindex += 4;
							break;
						case OP_UINT8:
							cindex++;
							break;
					}
					/* don't increment over the last one */
					if (*(i + 1) != OP_NONE) {
						A.tokens = A.tokens->next;
					}
				}
			} else if (!strcmp(on_word, "di") || !strcmp(on_word, "df")) {
				cindex += 8;
				/* skip the operand, not important yet */
				A.tokens = A.tokens->next;
			} else if (!strcmp(on_word, "db")) {
				/* could be a string or single byte.. */
				if (A.tokens->next->token->type == ASMTOK_INTEGER) {
					cindex++;
				} else if (A.tokens->next->token->type == ASMTOK_STRING) {
					const char* str = A.tokens->next->token->sval;
					size_t len = strlen(str);
					int start = cindex;
					for (size_t i = 0; i < len; i++) {
						switch (str[i]) {
							case '\\':
								i++;
							default:
								cindex++;
						}
					}
				} else {
					asm_die(&A, "'db' can only have an integer or string as an operand");
				}
				/* operand not important yet */
				A.tokens = A.tokens->next;
			} else if (peektype(&A) == ASMTOK_OPERATOR && A.tokens->next->token->oval == ':') {
				register_label(&A, on_word, cindex);
				A.tokens = A.tokens->next; /* skip colon */	
			}
		} else if (A.tokens->token->type == ASMTOK_STRING) {
			asm_die(&A, "unexpected string");
		}
		if (!A.tokens) {
			break;
		}
		A.tokens = A.tokens->next;
	}

	A.tokens = head; /* time for pass 2 */

	while (A.tokens) {
		if (A.tokens->token->type == ASMTOK_IDENTIFIER) {
			/* if it's a label definition, skip */
			if (peektype(&A) == ASMTOK_OPERATOR && A.tokens->next->token->oval == ':') {
				if (A.tokens->token->sval[0] != '.') {
					A.current_global = get_label(&A, A.tokens->token->sval);
				}
				A.tokens = A.tokens->next;
				continue;
			}
			if ((ins = spy_get_instruction(A.tokens->token->sval))) {
				fputc(ins->opcode, A.handle);
				for (int i = 0; i < 4; i++) {
					enum InstructionOperand op = ins->operands[i];
					if (op == OP_NONE) {
						break;
					}
					A.tokens = A.tokens->next;
					/* expect to be on a comma if it's not the first operand */
					if (i > 0) {
						if (!tok_istype(A.tokens->token, ASMTOK_OPERATOR) || A.tokens->token->oval != ',') {
							asm_die(&A, "expected comma");
						}
						A.tokens = A.tokens->next; /* eat comma */
					}
					switch (op) {
						case OP_INT64:
							switch (A.tokens->token->type) {
								case ASMTOK_INTEGER:
									fwrite(&A.tokens->token->ival, 1, sizeof(int64_t), A.handle);
									break;
								case ASMTOK_IDENTIFIER: {
									/* label needs a memory address */
									int64_t label = get_label(&A, A.tokens->token->sval)->addr;
									fwrite(&label, 1, sizeof(int64_t), A.handle);
									break;
								}
								default:
									asm_die(&A, "operand %d should be an integer or a label", i + 1);
							}
							break;
						case OP_FLOAT64:
							switch (A.tokens->token->type) {
								case ASMTOK_FLOAT:
									fwrite(&A.tokens->token->fval, 1, sizeof(double), A.handle);
									break;
								case ASMTOK_IDENTIFIER: {
									/* label needs a memory address */
									int64_t label = get_label(&A, A.tokens->token->sval)->addr;
									fwrite(&label, 1, sizeof(int64_t), A.handle);
									break;
								}
								default:
									asm_die(&A, "operand %d should be a float or a label", i + 1);
							}
							break;
						case OP_UINT32:
							break;
						case OP_UINT8:
							break;
					}
				}
			} else if (!strcmp(A.tokens->token->sval, "di")) {
				A.tokens = A.tokens->next;
				if (A.tokens->token->type != ASMTOK_INTEGER) {
					asm_die(&A, "expected integer");
				}
				fwrite(&A.tokens->token->ival, 1, sizeof(int64_t), A.handle);
			} else if (!strcmp(A.tokens->token->sval, "df")) {
				A.tokens = A.tokens->next;
				if (A.tokens->token->type != ASMTOK_FLOAT) {
					asm_die(&A, "expected float");
				}
				fwrite(&A.tokens->token->fval, 1, sizeof(double), A.handle);
			} else if (!strcmp(A.tokens->token->sval, "db")) {
				A.tokens = A.tokens->next;
				if (A.tokens->token->type == ASMTOK_INTEGER) {
					fputc((uint8_t)A.tokens->token->ival, A.handle);
				} else if (A.tokens->token->type == ASMTOK_STRING) {
					const char* str = A.tokens->token->sval;
					size_t len = strlen(str);
					for (size_t i = 0; i < len; i++) {
						switch (str[i]) {
							case '\\':
								switch (str[++i]) {
									case 'n':
										fputc('\n', A.handle);
										break;
									case 't':
										fputc('\t', A.handle);
										break;
									case '\\':
										fputc('\\', A.handle);
										break;
									case '0':
										fputc(0, A.handle);
										break;
									default:
										asm_die(&A, "invalid escape code '\\%c'", str[i]);
										break;
								}
								break;
							default:
								fputc((uint8_t)str[i], A.handle);
								break;
						}
					}
				} else {
					asm_die(&A, "expected byte(s)");
				}
			} else {
				asm_die(&A, "unexpected token '%s'", A.tokens->token->sval);
			}
		}
		A.tokens = A.tokens->next;
	}

	/* write NOP */
	fputc(0x00, A.handle);

	fclose(A.handle);

}
