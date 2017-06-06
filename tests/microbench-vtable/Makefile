BIN_DIR := ${CURDIR}/../../install/bin
CROSS_COMPILE := riscv64-unknown-elf-

CROSS_GCC = ${BIN_DIR}/${CROSS_COMPILE}gcc
CROSS_GXX = ${BIN_DIR}/${CROSS_COMPILE}g++
CROSS_CLANG = ${BIN_DIR}/clang++

#CFLAGS = -O0 -fpermissive -g
CFLAGS = -O0 -g

CLANG_CROSS_CFLAGS = -target riscv -mriscv=RV64IAMFD

all: bench.out bench.base.riscv bench.hdfi.riscv \
	bench.vtv.riscv bench.vtv.out bench.cps.riscv bench.cps.out

bench.out: bench.cpp
	g++ $(CFLAGS) -o $@ $^

bench.base.riscv: bench.cpp
	$(CROSS_GXX) $(CFLAGS) -S -o $@.s $<
	$(CROSS_GXX) $(LDFLAGS) -o $@ $@.s

bench.hdfi.riscv: bench.cpp
	$(CROSS_CLANG) $(CFLAGS) $(CLANG_CROSS_CFLAGS) -S -o $@.s $<
	$(CROSS_GXX) $(LDFLAGS) -o $@ $@.s

bench.cps.riscv: bench.cpp
	$(CROSS_GXX) -D__VTV__ $(CFLAGS) -o $@ $<

bench.cps.out: bench.cpp
	g++ -D__VTV__ $(CFLAGS) -o $@ $<

bench.vtv.riscv: bench.cpp
	$(CROSS_GXX) -D__VTV__ $(CFLAGS) -o $@ $<

bench.vtv.out: bench.cpp
	g++ -D__VTV__ $(CFLAGS) -o $@ $<

clean:
	rm -f *.out *.riscv *.s
