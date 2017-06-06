#!/bin/bash
set -x 

CUR_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

LLVM_SRC=${CUR_DIR}/../..
LLVM_OBJ=${CUR_DIR}/../../../llvm-build
LLVM_LIB=${LLVM_OBJ}/lib
CLANG_LIB=${LLVM_LIB}/clang/3.3/lib/linux

HDFI_BIN_DIR=${CUR_DIR}/../../../../install/bin
SYSROOT=${CUR_DIR}/../../../../install/sysroot

CROSS_COMPILE=riscv64-unknown-elf-

GCC=${HDFI_BIN_DIR}/${CROSS_COMPILE}gcc
GXX=${HDFI_BIN_DIR}/${CROSS_COMPILE}g++
LD=${HDFI_BIN_DIR}/${CROSS_COMPILE}ld

CC=${GXX}

# INC="-I/usr/include"
INC="-I${SYSROOT}/usr/include"

CFLAGS="-Wall -Werror -O3 -fPIC -fPIE -fno-exceptions -funwind-tables -fomit-frame-pointer -fno-builtin -fno-rtti -fno-stack-protector -g -DNDEBUG -D__gnu_linux__ -D__riscv__"

# echo Compile 32 bit:
# $CC $INC $CFLAGS -m32 -c -o cpi32.o $LLVM_SRC/projects/compiler-rt/lib/cpi/cpi.cc
# $CC $INC $CFLAGS -m32 -c -o interception32.o $LLVM_SRC/projects/compiler-rt/lib/interception/interception_linux.cc
# $CC $INC $CFLAGS -m32 -c -o safestack32.o $LLVM_SRC/projects/compiler-rt/lib/safestack/safestack.cc

echo Compile 64 bit:
$CC $INC $CFLAGS -m64 -c -o cpi64.o $LLVM_SRC/projects/compiler-rt/lib/cpi/cpi.c
$CC $INC $CFLAGS -m64 -c -o interception64.o $LLVM_SRC/projects/compiler-rt/lib/interception/interception_linux.cc
$CC $INC $CFLAGS -m64 -c -o safestack64.o $LLVM_SRC/projects/compiler-rt/lib/safestack/safestack.cc

echo Make output dir:
mkdir -p $CLANG_LIB

# echo Archive 32 bit:
# ar cru $CLANG_LIB/libclang_rt.cpi-i386.a cpi32.o
# ar cru $CLANG_LIB/libclang_rt.safestack-i386.a interception32.o safestack32.o

echo Archive 64 bit:
ar cru $CLANG_LIB/libclang_rt.cpi-riscv_64.a cpi64.o
ar cru $CLANG_LIB/libclang_rt.safestack-riscv_64.a interception64.o safestack64.o


# echo Ranlib 32 bit:
# ranlib $CLANG_LIB/libclang_rt.cpi-i386.a
# ranlib $CLANG_LIB/libclang_rt.safestack-i386.a

echo Ranlib 64 bit:
ranlib $CLANG_LIB/libclang_rt.cpi-riscv_64.a
ranlib $CLANG_LIB/libclang_rt.safestack-riscv_64.a
