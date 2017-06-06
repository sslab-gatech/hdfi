CUR_DIR = $(realpath $(LEVEL))

INSTALL_DIR := ${CUR_DIR}/../install
BIN_DIR := ${INSTALL_DIR}/bin

CROSS_COMPILE ?= riscv64-unknown-elf-

CLANG = ${BIN_DIR}/clang
CLANGXX = ${BIN_DIR}/clang++
GCC = ${BIN_DIR}/${CROSS_COMPILE}gcc
GXX = ${BIN_DIR}/${CROSS_COMPILE}g++
LD  = ${BIN_DIR}/${CROSS_COMPILE}ld

SPIKE = ${BIN_DIR}/spike

ifeq ($(CC),cc)
CC = $(GCC)
endif
ifeq ($(CXX),g++)
CXX = $(GXX)
endif

ifneq ($(findstring clang,$(CC)),)
CC = $(CLANG)
CXX = $(CLANGXX)
CFLAGS += -target riscv -mriscv=RV64IAMFD
CXXFLAGS += -target riscv -mriscv=RV64IAMFD
endif

ifeq ($(TARGET),)
TARGET = test
endif

.DEFAULT_GOAL := all

.SUFFIXES:
.SUFFIXES: .c .s .cpp

.IGNORE: test
.PHONY: all clean test

all: $(TARGET)

IMDS = $(SRCS:.c=.s)
ASMS += $(IMDS)

CXX_IMDS = $(CXX_SRCS:.cpp=.s)
ASMS += $(CXX_IMDS)

test: test.riscv
	$(SPIKE) pk $<

test.riscv : $(ASMS)
	$(GCC) $(LDFLAGS) -o $@ $^

%.s : %.c $(DEPS)
	$(CC) $(CFLAGS) -S -o $@ $<

%.s : %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -S -o $@ $<

clean::
	@rm -rf $(IMDS) $(CXX_IMDS) *.riscv
