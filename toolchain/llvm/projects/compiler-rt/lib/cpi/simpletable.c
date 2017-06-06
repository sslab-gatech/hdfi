#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>

#include "simpletable.h"
#include <stdio.h>

//-----------------------------------------------
// Globals
//-----------------------------------------------
#if defined(__gnu_linux__)
#if !defined(__riscv__)
# include <asm/prctl.h>
#endif
# include <sys/prctl.h>
#elif defined(__FreeBSD__)
# include <machine/sysarch.h>
#endif

int __llvm__cpi_inited = 0;
void* __llvm__cpi_table = 0;

// =============================================
// Initialization
// =============================================

/*** Interface function ***/
#if defined(__riscv__) // gcc seems not allowing zero# constructor
__attribute__((constructor(101)))
#else
__attribute__((constructor(0)))
#endif
__CPI_EXPORT
void __llvm__cpi_init() {
  if (__llvm__cpi_inited)
    return;

  __llvm__cpi_inited = 1;

  __llvm__cpi_table = mmap((void*) CPI_TABLE_ADDR,
                      CPI_TABLE_NUM_ENTRIES*sizeof(tbl_entry),
                      PROT_READ | PROT_WRITE,
                      CPI_MMAP_FLAGS, -1, 0);
  if (__llvm__cpi_table == (void*) -1) {
    perror("Cannot map __llvm__cpi_dir");
    abort();
  }

/* # if defined(__gnu_linux__) */
/*   int res = arch_prctl(ARCH_SET_GS, __llvm__cpi_table); */
/*   if (res != 0) { */
/*     perror("arch_prctl failed"); */
/*     abort(); */
/*   } */
/* # elif defined(__FreeBSD__) */
/*   int res = amd64_set_gsbase(__llvm__cpi_table); */
/*   if (res != 0) { */
/*     perror("arch_prctl failed"); */
/*     abort(); */
/*   } */
/* # endif */

  DEBUG("[CPI] Initialization completed\n");

  return;
}

__attribute__((destructor(101)))
__CPI_EXPORT
void __llvm__cpi_destroy(void) {
#ifdef CPI_PROFILE_STATS
    __llvm__cpi_profile_statistic();
#endif
  DEBUG("[CPI] Finalizatoin completed\n");
}

// =============================================
// Debug functions
// =============================================

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_dump(void **ptr_address) {

  tbl_entry *entry = tbl_address(ptr_address);

  fprintf(stderr, "Pointer  address: %p\n", ptr_address);
  if (ptr_address)
    fprintf(stderr, "Pointer  value  : %p\n", *ptr_address);

  if (!entry) {
    fprintf(stderr, "No entry for address: %p\n", ptr_address);
  } else {
    fprintf(stderr, "Metadata address: %p\n", entry);
    fprintf(stderr, "Metadata value  : %p\n", entry->ptr_value);
#ifdef CPI_BOUNDS
    fprintf(stderr, "Lower bound:    : 0x%lx\n", entry->bounds[0]);
    fprintf(stderr, "Upper bound:    : 0x%lx\n", entry->bounds[1]);
#endif
  }
}

// =============================================
// Deletion functions
// =============================================
// static __attribute__((always_inline))
void __llvm__cpi_do_delete_range(unsigned char *src, size_t size) {
  DEBUG("[CPI] Do delete [%p, %p)\n", src, src + size);

  unsigned char *end = (unsigned char*)
      ((((size_t)src) + size + pointer_size-1) & pointer_mask);

  src = (unsigned char*) (((size_t) src) & pointer_mask);
  memset(tbl_address(src), 0, (end - src) * tbl_entry_size_mult);
}

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_delete_range(unsigned char *src, size_t size) {
  DEBUG("[CPI] Delete [%p, %p)%s%s\n", src, src + size,
        (((size_t)src)&(pointer_size-1)) ? " src misaligned":"",
        (size&(pointer_size-1)) ? " size misaligned":"");
#ifdef CPI_DO_DELETE
  __llvm__cpi_do_delete_range(src, size);
#endif // CPI_DO_DELETE
}

// =============================================
// Data movement functions
// =============================================

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_copy_range(unsigned char *dst, unsigned char *src,
                            size_t size) {
  DEBUG("[CPI] memcpy [%p, %p) -> [%p, %p)%s%s%s\n",
        src, src + size, dst, dst + size,
        (((size_t)src)&(pointer_size-1)) ? " src misaligned":"",
        (((size_t)dst)&(pointer_size-1)) ? " dst misaligned":"",
        (size&(pointer_size-1)) ? " size misaligned":"");

  if (CPI_EXPECTNOT((dst-src) & (pointer_size-1))) {
    // Misaligned copy; we can't support it so let's just delete dst
    __llvm__cpi_do_delete_range(dst, size);
    return;
  }

  // FIXME: in case of misaligned copy, we should clobber first and last entry
  unsigned char *src_end = (unsigned char*)
      ((((size_t)src) + size + pointer_size-1) & pointer_mask);

  src = (unsigned char*) (((size_t) src) & pointer_mask);
  memcpy(tbl_address(dst), tbl_address(src),
         (src_end - src) * tbl_entry_size_mult);
}

// ---------------------------------------------

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_move_range(unsigned char *dst, unsigned char *src,
                            size_t size) {
  DEBUG("[CPI] memmove [%p, %p) -> [%p, %p)%s%s%s\n",
        src, src + size, dst, dst + size,
        (((size_t)src)&(pointer_size-1)) ? " src misaligned":"",
        (((size_t)dst)&(pointer_size-1)) ? " dst misaligned":"",
        (size&(pointer_size-1)) ? " size misaligned":"");

  if (CPI_EXPECTNOT((dst-src) & (pointer_size-1))) {
    // Misaligned copy; we can't support it so let's just delete dst
    __llvm__cpi_do_delete_range(dst, size);
    return;
  }

  // FIXME: in case of misaligned copy, we should clobber first and last entry
  unsigned char *src_end = (unsigned char*)
      ((((size_t)src) + size + pointer_size-1) & pointer_mask);

  src = (unsigned char*) (((size_t) src) & pointer_mask);
  memmove(tbl_address(dst), tbl_address(src),
          (src_end - src) * tbl_entry_size_mult);
}

// ---------------------------------------------
