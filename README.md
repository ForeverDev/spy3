Welcome to the Spyre programming language.  Throughout highschool I oddly became deeply
interested in everything low level - programming languages, virtual machines, compilers,
assemblers, you name it.  So, I ended up trying to write my own little language in C.
Here's what I came up with.

## DISCLAIMER
This language is far from perfect.  It's really just meant to be a fun
little thought expirement gone wild.  As I wrote this code, I was still
learning tons about languages, compilers, and even C itself (I was 16 at
the time, cut me a little slack!)

## This repo includes:
1. A Spyre virtual machine.  The entire VM is stack based (I took a lot of
   inspiration from the JVM instruction set). See vm.c for the instruction
   set (looks like I didn't document it very well at all... whoops)
2. A Spyre assembler.  This allows the user to write assembly code for the
   Spyre VM and assemble it into Spyre bytecode.  Assembly code should
   have the .spys extension, and will produce bytecode with the .spyb
   extension.
3. A full blown Spyre compiler.  Unfortunately, I didn't do a great job of
   actually writing a Spyre SPECIFICATION, so it's not very clear how to
   actually WRITE Spyre code.  However, if you take a look at demo/bigtest.spy
   or demo/mandelbrot.spy, you'll see some example programs and syntax.
4. A C-API.  Users can write their own C functions and use the Spyre-C 
   library to call these functions with Spyre syntax.

## Some interesting points:
- When you compile a .spy file, it will produce a Spyre binary output (.spyb).
  However, it will also spit out an assembly representation of the binary file.
  So, compiling 'test.spy' will output 'test.spys' and 'test.spyb'.  The
  'test.spys' file has some comments made by the compiler to help you understand
  what is actually going on.
- The compiler is pretty cool!  It's got full blown typechecking, the ability
  to allocate and free memory, recursive functions, for loops, while loops,
  pointers, arrays, structs, etc.

Map of what actually happens:
SPYRE CODE (.spy) -> SPYRE COMPILER -> SPYRE ASSEMBLY CODE (.spys)
SPYRE ASSEMBLY CODE (.spys) -> SPYRE ASSEMBLER -> SPYRE BYTECODE (.spyb)
SPYRE BYTECODE (.spyb) -> SPYRE VIRTUAL MACHINE -> your program is run!

## WANT TO TRY IT OUT?
```
git clone https://github.com/ForeverDev/spy3.git
cd spy3
make
spy demo/bigtest
spy demo/mandelbrot
```

David Wilson
