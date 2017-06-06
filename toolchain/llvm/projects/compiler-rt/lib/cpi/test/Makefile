CFLAGS=-O0 -I../ -g -Wall -Wextra -Winline

OBJS=cpi.o cpi_test.o

default: all

all: test

test: cpi_test.c ../cpi.h ../cpi.c
	$(CXX) ${CFLAGS} ../cpi.c cpi_test.c -o test


.PHONY=clean
clean:
	rm -f ${OBJS} test
