LEVEL = ..
CXX_SRCS = simple.cpp
CC=clang
CXXFLAGS = -O0
TARGET = simple
include $(LEVEL)/Makefile.inc

simple: simple.riscv
	$(SPIKE) pk $<

simple.riscv: $(ASMS)
	$(GXX) $(LDFLAGS) -o $@ $^
