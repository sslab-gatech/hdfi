#!/bin/sh

set -x 

LLVM_SRC=/root/llvm
LLVM_OBJ=/root/build
LLVM_LIB=$LLVM_OBJ/Debug+Asserts/lib
CLANG_LIB=$LLVM_LIB/clang/3.3.1/lib/freebsd

CC=clang
INC=-I$LLVM_SRC/projects/compiler-rt/lib
CFLAGS="-Wall -Werror -O3 -fPIC -fPIE -fno-exceptions -funwind-tables -fomit-frame-pointer -fno-builtin -fno-rtti -fno-stack-protector -g -DNDEBUG"
# -m32/-m64 -fPIC -fPIE


echo Compile 32 bit:
$CC $INC $CFLAGS -m32 -c -o cpi32.o $LLVM_SRC/projects/compiler-rt/lib/cpi/cpi.cc
$CC $INC $CFLAGS -m32 -c -o interception32.o $LLVM_SRC/projects/compiler-rt/lib/interception/interception_linux.cc
$CC $INC $CFLAGS -m32 -c -o safestack32.o $LLVM_SRC/projects/compiler-rt/lib/safestack/safestack.cc

echo Compile 64 bit:
$CC $INC $CFLAGS -m64 -c -o cpi64.o $LLVM_SRC/projects/compiler-rt/lib/cpi/cpi.cc
$CC $INC $CFLAGS -m64 -c -o interception64.o $LLVM_SRC/projects/compiler-rt/lib/interception/interception_linux.cc
$CC $INC $CFLAGS -m64 -c -o safestack64.o $LLVM_SRC/projects/compiler-rt/lib/safestack/safestack.cc


echo Make output dir:
mkdir -p $CLANG_LIB


echo Archive 32 bit:
ar cru $CLANG_LIB/libclang_rt.cpi-i386.a cpi32.o
ar cru $CLANG_LIB/libclang_rt.safestack-i386.a interception32.o safestack32.o

echo Archive 64 bit:
ar cru $CLANG_LIB/libclang_rt.cpi-x86_64.a cpi64.o
ar cru $CLANG_LIB/libclang_rt.safestack-x86_64.a interception64.o safestack64.o


echo Ranlib 32 bit:
ranlib $CLANG_LIB/libclang_rt.cpi-i386.a
ranlib $CLANG_LIB/libclang_rt.safestack-i386.a

echo Ranlib 64 bit:
ranlib $CLANG_LIB/libclang_rt.cpi-x86_64.a
ranlib $CLANG_LIB/libclang_rt.safestack-x86_64.a
