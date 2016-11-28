#include <stdio.h>
#include <stdint.h>
#include "vm.h"
#include "assemble.h"

int main(int argc, char** argv) {
	
	generate_bytecode("demo/compile.spys", "demo/compile.spyb");
	spy_init();
	spy_execute("demo/compile.spyb");
	spy_dump();

	return 0;

}	
