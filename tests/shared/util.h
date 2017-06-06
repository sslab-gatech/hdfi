#ifndef _HDFI_UTIL_H_
#define _HDFI_UTIL_H_
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define QWORD (64 / 8)
#define NOT_INLINE  __attribute__ ((noinline))
static void NOT_INLINE memcpy0(void* dst, void* src, size_t size) {
  assert(size % QWORD == 0);
  __asm(
	"add	sp,sp,-64\n"
	"sd	s0,56(sp)\n"
	"add	s0,sp,64\n"
	"sd	a0,-40(s0)\n"
	"sd	a1,-48(s0)\n"
	"sd	a2,-56(s0)\n"
	"sw	zero,-20(s0)\n"
	"j	2f\n"
"1:\n"
	"lw	a5,-20(s0)\n"
	"sll	a5,a5,3\n"
	"ld	a4,-40(s0)\n"
	"add	a5,a4,a5\n"
	"lw	a4,-20(s0)\n"
	"sll	a4,a4,3\n"
	"ld	a3,-48(s0)\n"
	"add	a4,a3,a4\n"
	"ld	a4,0(a4)\n"
	"sdset0	a4,0(a5)\n"
	"lw	a5,-20(s0)\n"
	"addw	a5,a5,1\n"
	"sw	a5,-20(s0)\n"
"2:\n"
	"lw	a4,-20(s0)\n"
	"ld	a5,-56(s0)\n"
	"srl	a5,a5,3\n"
	"bltu	a4,a5,1b\n"
	"nop\n"
	"ld	s0,56(sp)\n"
	"add	sp,sp,64\n");
}

static void NOT_INLINE memcpy1(void* dst, void* src, size_t size) {
  assert(size % QWORD == 0);
  __asm(
	"add	sp,sp,-64\n"
	"sd	s0,56(sp)\n"
	"add	s0,sp,64\n"
	"sd	a0,-40(s0)\n"
	"sd	a1,-48(s0)\n"
	"sd	a2,-56(s0)\n"
	"sw	zero,-20(s0)\n"
	"j	2f\n"
"1:\n"
	"lw	a5,-20(s0)\n"
	"sll	a5,a5,3\n"
	"ld	a4,-40(s0)\n"
	"add	a5,a4,a5\n"
	"lw	a4,-20(s0)\n"
	"sll	a4,a4,3\n"
	"ld	a3,-48(s0)\n"
	"add	a4,a3,a4\n"
	"ld	a4,0(a4)\n"
	"sdset1	a4,0(a5)\n" // only difference
	"lw	a5,-20(s0)\n"
	"addw	a5,a5,1\n"
	"sw	a5,-20(s0)\n"
"2:\n"
	"lw	a4,-20(s0)\n"
	"ld	a5,-56(s0)\n"
	"srl	a5,a5,3\n"
	"bltu	a4,a5,1b\n"
	"nop\n"
	"ld	s0,56(sp)\n"
	"add	sp,sp,64\n");
}

static void NOT_INLINE check0(void* buf, size_t size) {
  // XXX: size % 8 != 0 -> some bytes may be missing
  // size = size - (size % 8);
__asm(
  "add	sp,sp,-48\n"
	"sd	s0,40(sp)\n"
	"add	s0,sp,48\n"
	"sd	a0,-40(s0)\n"
	"sd	a1,-48(s0)\n"
	"sw	zero,-20(s0)\n"
	"j	2f\n"
"1:\n"
	"lw	a5,-20(s0)\n"
	"sll	a5,a5,3\n"
	"ld	a4,-40(s0)\n"
	"add	a5,a4,a5\n"
	"ldchk0	a5,0(a5)\n"
	"sd	a5,-32(s0)\n"
	"lw	a5,-20(s0)\n"
	"addw	a5,a5,1\n"
	"sw	a5,-20(s0)\n"
"2:\n"
	"lw	a4,-20(s0)\n"
	"ld	a5,-48(s0)\n"
	"srl	a5,a5,3\n"
	"bltu	a4,a5,1b\n"
	"nop\n"
	"ld	s0,40(sp)\n"
	"add	sp,sp,48\n");
}

static void NOT_INLINE check1(void* buf, size_t size) {
  size = size - (size % 8);
__asm(
  "add	sp,sp,-48\n"
	"sd	s0,40(sp)\n"
	"add	s0,sp,48\n"
	"sd	a0,-40(s0)\n"
	"sd	a1,-48(s0)\n"
	"sw	zero,-20(s0)\n"
	"j	2f\n"
"1:\n"
	"lw	a5,-20(s0)\n"
	"sll	a5,a5,3\n"
	"ld	a4,-40(s0)\n"
	"add	a5,a4,a5\n"
	"ldchk1	a5,0(a5)\n" // only diff
	"sd	a5,-32(s0)\n"
	"lw	a5,-20(s0)\n"
	"addw	a5,a5,1\n"
	"sw	a5,-20(s0)\n"
"2:\n"
	"lw	a4,-20(s0)\n"
	"ld	a5,-48(s0)\n"
	"srl	a5,a5,3\n"
	"bltu	a4,a5,1b\n"
	"nop\n"
	"ld	s0,40(sp)\n"
	"add	sp,sp,48\n");
}
#endif // _HDFI_UTIL_H_
