/**
 * @file cpi_test.c
 * This module runs a bunch of tests for the cpi hash table.
 *
 * Copyright (c) 2013 UC Berkeley
 * @author Mathias Payer <mathias.payer@nebelwelt.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

//#define CPI_BOUNDS 1

#include "cpi.h"

extern "C" {

typedef struct {
    void *value;
} ptrval_t;

ptrval_t *fptr_get(void **addr) {
    return (ptrval_t*) __llvm__cpi_get_metadata(addr);
}

void test1() {
    assert(fptr_get((void**)0x0)->value == 0x0);  
    assert(fptr_get((void**)0x12)->value == 0x0);  
    assert(fptr_get((void**)0xffffffff)->value == 0x0);  
}

void test2() {
    assert(fptr_get((void**)0x42)->value == 0x0);
    __llvm__cpi_set((void**)0x42, (void*)22);
    __llvm__cpi_set((void**)0x42, (void*)23);
    __llvm__cpi_set((void**)0x20, (void*)1);
    __llvm__cpi_set((void**)(0x20+sizeof(void*)), (void*)2);
    __llvm__cpi_set((void**)(0x20+2*sizeof(void*)), (void*)3);
}

void test3() {
    assert(fptr_get((void**)0x42)->value == (void*)23);
    assert(fptr_get((void**)0x20)->value == (void*)1);
    assert(fptr_get((void**)(0x20+sizeof(void*)))->value == (void*)2);
    assert(fptr_get((void**)(0x20+2*sizeof(void*)))->value == (void*)3);
}

void test4() {
    __llvm__cpi_set((void**)0x42, (void*)24);
    __llvm__cpi_set((void**)0x20, (void*)4);
    __llvm__cpi_set((void**)(0x20+sizeof(void*)), (void*)5);
    __llvm__cpi_set((void**)(0x20+2*sizeof(void*)), (void*)6);
}

void test5() {
    assert(fptr_get((void**)0x42)->value == (void*)24);
    assert(fptr_get((void**)0x20)->value == (void*)4);
    assert(fptr_get((void**)(0x20+sizeof(void*)))->value == (void*)5);
    assert(fptr_get((void**)(0x20+2*sizeof(void*)))->value == (void*)6);
}

void test6() {
    __llvm__cpi_delete_range((unsigned char*)0x20, 0x8);
    __llvm__cpi_delete_range((unsigned char*)(0x20+sizeof(void*)), 0x8);
    __llvm__cpi_delete_range((unsigned char*)0x20, 0x8);
    __llvm__cpi_delete_range((unsigned char*)(0x20+2*sizeof(void*)), 0x8);
    assert(fptr_get((void**)0x42)->value == (void*)24);
    assert(fptr_get((void**)0x20) == 0x0 || fptr_get((void**)0x20)->value == 0x0);
    assert(fptr_get((void**)(0x20+sizeof(void*))) == 0x0 || fptr_get((void**)(0x20+sizeof(void*)))->value == 0x0);
    assert(fptr_get((void**)(0x20+2*sizeof(void*))) == 0x0 || fptr_get((void**)(0x20+2*sizeof(void*)))->value == 0x0);
    __llvm__cpi_set((void**)(0x20+2*sizeof(void*)), (void*)6);
    assert(fptr_get((void**)(0x20+2*sizeof(void*)))->value == (void*)6);
}

void test7() {
#if defined(CPI_BOUNDS)
    __llvm__cpi_set_bounds((void**)0x42, (void*)24, (void*)24, (void*)24);
    __llvm__cpi_set_bounds((void**)(0x20+2*sizeof(void*)), (void*)6, (void*)6, (void*)6);
#endif
    /*
    assert(__llvm__cpi_check((void**)0x20, 0x0));
    assert(__llvm__cpi_check((void**)(0x20+sizeof(void*)), 0x0));
    assert(__llvm__cpi_check((void**)0x42, (void*)24) == 1 );
    assert(__llvm__cpi_check((void**)(0x20+2*sizeof(void*)), (void*)6));
    assert(__llvm__cpi_check((void**)0x42, (void*)25) == 0);
    assert(__llvm__cpi_check((void**)(0x20+2*sizeof(void*)), (void*)7) == 0);
    */
}

void test8() {
    __llvm__cpi_set((void**) 0x79, (void*) 0xaa);
    __llvm__cpi_set((void**) 0x80, (void*) 0xbb);
    __llvm__cpi_copy_range((unsigned char*)0x120, (unsigned char*)0x20, 0x60);
    assert(fptr_get((void**)0x140)->value == (void*)24);
    assert(fptr_get((void**)0x142)->value == (void*)24);
    assert(fptr_get((void**)0x120) == 0x0 || fptr_get((void**)0x120)->value == 0x0);
    assert(fptr_get((void**)(0x120+sizeof(void*)))->value == 0x0);
    assert(fptr_get((void**)(0x120+2*sizeof(void*)))->value == (void*)6);

    assert(fptr_get((void**)0x179)->value == (void*)0xaa);
    assert(fptr_get((void**)0x180)->value == 0x0);
}

void test9() {
    __llvm__cpi_set((void**) 0x180, (void*) 0xbb);
    __llvm__cpi_delete_range((unsigned char*)0x120, 0x60);
    assert(fptr_get((void**)0x142) == 0x0 || fptr_get((void**)0x142)->value == 0x0);
    assert(fptr_get((void**)0x120) == 0x0 || fptr_get((void**)0x120)->value == 0x0);
    assert(fptr_get((void**)(0x120+sizeof(void*))) == 0x0 || fptr_get((void**)(0x120+sizeof(void*)))->value == 0x0);
    assert(fptr_get((void**)(0x120+2*sizeof(void*))) == 0x0 || fptr_get((void**)(0x120+2*sizeof(void*)))->value == 0x0);

    assert(fptr_get((void**)0x179) == 0x0 || fptr_get((void**)0x179)->value == 0x0 );
    assert(fptr_get((void**)0x180)->value == (void*)0xbb);
}

int main() {
    printf("Running test1 (get)\n");
    test1();
    printf("Running test2 (set)\n");
    test2();
    printf("Running test3 (get)\n");
    test3();
    printf("Running test4 (update)\n");
    test4();
    printf("Running test5 (get)\n");
    test5();
    printf("Running test6 (delete)\n");
    test6();
    printf("Running test7 (check)\n");
    test7();
    printf("Running test8 (copy_range)\n");
    test8();
    printf("Running test9 (delete_range)\n");
    test9();
    return 0;
}

}
