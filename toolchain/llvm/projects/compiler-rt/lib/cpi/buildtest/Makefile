CC=clang
CFLAGS=-O3 -I../

all: inlines runtime test clean

inlines:
	$(CC) $(CFLAGS) -D CPI_INLINES_ONLY -S -emit-llvm ../cpi_inlines.c
	$(CC) $(CFLAGS) -D CPI_INLINES_ONLY -c -emit-llvm ../cpi_inlines.c
	! cat cpi_inlines.ll | grep dir_index
	cat cpi_inlines.ll | grep __cpi_fptr_set

runtime:
	$(CC) $(CFLAGS) -c ../cpi.c
	objdump -d cpi.o > cpi.S
	! cat cpi.S | grep dir_index
	! cat cpi.S | grep __cpi_fptr_set

test:
	$(CC) $(CFLAGS) test.c -o test
	objdump -d test > test.S
	! cat test.S | grep dir_index
	! cat test.S | grep __cpi_fptr_set
	./test

clean:
	- rm *.bc *.ll *.S cpi.o test
