// See LICENSE for license details.

//**************************************************************************
// Towers of Hanoi benchmark
//--------------------------------------------------------------------------
//
// Towers of Hanoi is a classic puzzle problem. The game consists of
// three pegs and a set of discs. Each disc is a different size, and
// initially all of the discs are on the left most peg with the smallest
// disc on top and the largest disc on the bottom. The goal is to move all
// of the discs onto the right most peg. The catch is that you are only
// allowed to move one disc at a time and you can never place a larger
// disc on top of a smaller disc.
//
// This implementation starts with NUM_DISC discs and uses a recursive
// algorithm to sovel the puzzle.  The smips-gcc toolchain does not support
// system calls so printf's can only be used on a host system, not on the
// smips processor simulator itself. You should not change anything except
// the HOST_DEBUG and PREALLOCATE macros for your timing run.

#include "util.h"

//--------------------------------------------------------------------------
// Main
extern char* malloc();

void store_set_tag(unsigned long* addr, unsigned long data) {

  __asm__ __volatile__("sdset1\t%1, 0(%0)" : : "r" (addr), "r" (data): "memory" );
  //printf("store_set_tag:\t%p\t<-\t%lx\n",addr,data); 
}


unsigned long load_check_tag(unsigned long* addr) {

  unsigned long ret;

  __asm__ __volatile__("ldchk1\t%0, 0(%1)" : "=r" (ret) : "r" (addr) );
  //printf("load_check_tag:\t%p:\t%lx\n",addr,ret); 

  return ret;
}

unsigned long addr_to_tag_entry(unsigned long base, unsigned long addr) {

  unsigned long offset = (addr >> 11) << 6;
  return base + offset;
}

void* adfim_memcpy(void* addr1, const void* addr2, unsigned n)
{
  void* dst = addr1;
  void* src = addr2;

  __asm__ __volatile("memcpy\t%1, 0(%0)" : : "r" (src), "r" (dst): "memory");
  __asm__ __volatile("memcpy\t%1, 8(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 16(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 24(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 32(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 40(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 48(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 56(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 64(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 72(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 80(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 88(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 96(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 104(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 112(%0)" : : "r" (src), "r" (dst): "memory");
  //__asm__ __volatile("memcpy\t%1, 120(%0)" : : "r" (src), "r" (dst): "memory");

  return addr1;
}

// void* cc_memcpy(void* aa, const void* bb, unsigned n)
// {
//   #define BODY(a, b, t) { \
//     t tt = *b; \
//     a++, b++; \
//     *(a-1) = tt; \
//   }

//   char* a = (char*)aa;
//   const char* b = (const char*)bb;
//   char* end = a+n;
//   uintptr_t msk = sizeof(long)-1;
//   if (__builtin_expect(((uintptr_t)a & msk) != ((uintptr_t)b & msk) || n < sizeof(long), 0))
//   {
// small:
//     if (__builtin_expect(a < end, 1))
//       while (a < end)
//         BODY(a, b, char);
//     return aa;
//   }

//   if (__builtin_expect(((uintptr_t)a & msk) != 0, 0))
//     while ((uintptr_t)a & msk)
//       BODY(a, b, char);

//   long* la = (long*)a;
//   const long* lb = (const long*)b;
//   long* lend = (long*)((uintptr_t)end & ~msk);

//   if (__builtin_expect(la < lend-8, 0))
//   {
//     while (la < lend-8)
//     {
//       long b0 = *lb++;
//       long b1 = *lb++;
//       long b2 = *lb++;
//       long b3 = *lb++;
//       long b4 = *lb++;
//       long b5 = *lb++;
//       long b6 = *lb++;
//       long b7 = *lb++;
//       long b8 = *lb++;
//       *la++ = b0;
//       *la++ = b1;
//       *la++ = b2;
//       *la++ = b3;
//       *la++ = b4;
//       *la++ = b5;
//       *la++ = b6;
//       *la++ = b7;
//       *la++ = b8;
//     }
//   }

//   while (la < lend)
//     BODY(la, lb, long);

//   a = (char*)la;
//   b = (const char*)lb;
//   if (__builtin_expect(a < end, 0))
//     goto small;
//   return aa;
// }

// void* ac_memcpy(void* addr1, const void* addr2, unsigned n)
// {
//   void* dst = addr1;
//   void* src = addr2;

//   unsigned long data;

//   __asm__ __volatile__("ld\t%0, 0(%1)" : "=r" (data) : "r" (src) );
//   __asm__ __volatile__("sd\t%1, 0(%0)" : : "r" (dst), "r" (data): "memory" );
//   __asm__ __volatile__("ld\t%0, 8(%1)" : "=r" (data) : "r" (src) );
//   __asm__ __volatile__("sd\t%1, 8(%0)" : : "r" (dst), "r" (data): "memory" );

//   return addr1;
// }

// void* adfi_memcpy(void* addr1, const void* addr2, unsigned n)
// {
//   void* dst = addr1;
//   void* src = addr2; 

//   unsigned long data;

//   __asm__ __volatile__("ldchk1\t%0, 0(%1)" : "=r" (data) : "r" (src) );
//   __asm__ __volatile__("sdset1\t%1, 0(%0)" : : "r" (dst), "r" (data): "memory" );
//   __asm__ __volatile__("ldchk1\t%0, 8(%1)" : "=r" (data) : "r" (src) );
//   __asm__ __volatile__("sdset1\t%1, 8(%0)" : : "r" (dst), "r" (data): "memory" );

//   return addr1;
// }

int main( int argc, char* argv[] )
{
/*
  plain tag table:
  
  byteAddrBits = 3
  entryAddrBits = 9
  tableAddrBits = 20
  512bit -> 512words -> 512*8=4KB
  512 * 2^20 bits->2^20B
  2^29 bits = 2^26 bytes
  64MB
 */ 

  unsigned long arr[2048];
  unsigned long *critical;

  unsigned long* scr_base = 0x40008000;
  volatile unsigned long* tagger_base = scr_base + 8;
  volatile unsigned long tagger_control_val = *tagger_base;

  printf("nCores\t@%p:\t%d\n",scr_base,*scr_base);
  //printf("MMIOSize(MB)\t@%p:\t%d\n",scr_base+1,*(scr_base+1));
  printf("tagger_control\t@%p:\t%lx\n",scr_base+8,*tagger_base);
  
  //unsigned long* tagtable_base = (unsigned long*)malloc(4*1024*1024*1024);
  unsigned long* tagtable_base = 0x800000;
  unsigned long tagtable_mask = 0xffffffffl;
  *tagger_base = (tagger_control_val & 0xffffffff00000000l) | ((unsigned long)tagtable_base & tagtable_mask);
  printf("tagger_control should be:%lx\n",(unsigned long) & tagtable_mask);
  printf("tagger_control(updated)\t@%p:\t%lx\n",scr_base,*tagger_base);
  *tagger_base = *tagger_base | (0x200000000l); // WB enable
  *tagger_base = *tagger_base | (0x100000000l); // enable

  long j = 1;
  long i = 0;
  unsigned long val = 0xCC11ul;
  unsigned long* entry; 
  printf("execute sdset1s\n");
  for(j = 0; j < 32 ; j = j + 32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    store_set_tag(critical, val);
    // printf("stored?\t%lx=%lx\n",val,load_check_tag(critical));
  }
  printf("let's make them written back\n");

  long k = 0;
  for(k = 0; k < 2; k++){
    for(j = 128 ; j < 2048 ; j++) {
      for(i = 0 ; i < 32; i++){
	    arr[j] = arr[j-64+i*32];
      }
    }
  }

  printf("wait for a moment..\n");
  for(j = 0; j < 10240000; j++);
  
  printf("checkt the tag entries\n");
  for(j = 0; j < 32 ; j = j + 32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("block for critical:\t%lx\n",((unsigned long)critical) >> 7);
    entry = addr_to_tag_entry((unsigned long)tagtable_base, (unsigned long)critical);
    //printf("block for entry:\t%lx\n",((unsigned long)entry) >> 7);
    printf("tag @ %p:\t",entry);
    for(i = 7; i >=0 ; i -= 1) {
      printf("%016lx",*(entry + i));
    }
    printf("\n");
  }
  
  printf("2nd stage lchk test after artificail cache flush\n");
  for(j = 0; j < 32 ; j = j +  32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    printf("stored?\t%lx=%lx\n",val,load_check_tag(critical));
  }
  
  printf("the end of main\n");

  unsigned long arr2[2048];

  setStats(1);
  adfim_memcpy(arr2, arr, 10);
  setStats(0);
  return 0;
}



