CC = gcc
CF = -std=c11 -g -Wno-int-to-pointer-cast
OBJ = build/main.o build/vm.o build/asmlex.o build/assemble.o build/spylib.o build/capi_io.o build/capi_load.o

all: spy.exe

clean:
	rm -Rf build/*.o

spy.exe: build $(OBJ)
	$(CC) $(CF) $(OBJ) -o spy.exe
	mv spy.exe C:\MinGW\bin\spy.exe

build:
	mkdir build

build/vm.o:
	$(CC) $(CF) -c vm.c -o build/vm.o

build/asmlex.o:
	$(CC) $(CF) -c asmlex.c -o build/asmlex.o

build/assemble.o:
	$(CC) $(CF) -c assemble.c -o build/assemble.o

build/spylib.o:
	$(CC) $(CF) -c spylib.c -o build/spylib.o

build/capi_io.o:
	$(CC) $(CF) -c capi_io.c -o build/capi_io.o

build/capi_load.o:
	$(CC) $(CF) -c capi_load.c -o build/capi_load.o

build/main.o:
	$(CC) $(CF) -c main.c -o build/main.o
