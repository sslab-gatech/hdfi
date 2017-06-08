#!/bin/bash
. build.common

CC= CXX= 
#build_project riscv-pk --prefix=$RISCV/riscv64-unknown-elf --host=riscv64-unknown-elf
build_tests
