#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>

#include "lookuptable.h"
#include <stdio.h>

//-----------------------------------------------
// Globals
//-----------------------------------------------
#if defined(CPI_USE_SEGMENT)
# if defined(__linux__)
#  include <asm/prctl.h>
#  include <sys/prctl.h>
# elif defined(__FreeBSD__)
#  include <machine/sysarch.h>
# endif
#elif defined(CPI_ST_STATIC)
//tbl_entry *__llvm__cpi_dir[dir_entry_num] __attribute__((aligned(0x1000)));
#else
tbl_entry **__llvm__cpi_dir; // XXX: this needs to be static!
#endif

int __llvm__cpi_inited = 0;

// =============================================
// Initialization
// =============================================

/*** Interface function ***/
__attribute__((constructor(0)))
__CPI_EXPORT
void __llvm__cpi_init() {

  /* assert(dir_entry_size == 32); */

  if (__llvm__cpi_inited)
    return;

  __llvm__cpi_inited = 1;

#if defined(CPI_USE_SEGMENT)
  void *addr = mmap(0, dir_size, PROT_READ | PROT_WRITE,
                    CPI_MMAP_FLAGS, -1, 0);
# if defined(__gnu_linux__)
  int res = arch_prctl(ARCH_SET_GS, addr);
  if (res != 0) {
    perror("arch_prctl failed");
    abort();
  }
# elif defined(__FreeBSD__)
  int res = amd64_set_gsbase(addr);
  if (res != 0) {
    perror("arch_prctl failed");
    abort();
  }
# else
#  error "CPI_USE_SEGMENT is not supported on this platform"
# endif
#elif defined(CPI_ST_STATIC)
  void *addr = mmap(__llvm__cpi_dir, dir_size, PROT_READ | PROT_WRITE,
                    CPI_MMAP_FLAGS, -1, 0);
#else
  void *addr = mmap(0, dir_size, PROT_READ | PROT_WRITE,
                    CPI_MMAP_FLAGS, -1, 0);
  __llvm__cpi_dir = (tbl_entry**)
#endif

  if (addr == (void*) -1) {
    perror("Cannot map __llvm__cpi_dir");
    abort();
  }

#ifdef CPI_MMAP_MIN_ADDR
  if (CPI_MMAP_MIN_ADDR < tbl_size) {
    addr = mmap((void*) CPI_MMAP_MIN_ADDR, tbl_size - CPI_MMAP_MIN_ADDR,
                PROT_READ, CPI_NULLTBL_MMAP_FLAGS, -1, 0);
  }
  if (addr == (void*) -1) {
    perror("Cannot mmap nulltbl");
    abort();
  }
#endif

  DEBUG("[CPI] Initialization completed\n");

  return;
}

__attribute__((destructor(0)))
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

  const tbl_entry *entry = __llvm__cpi_get(ptr_address);

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
static inline
void __llvm__cpi_do_delete_range(unsigned char *src, size_t size) {
  DEBUG("[CPI] Do delete [%p, %p)\n", src, src + size);

  unsigned char *end = (unsigned char*)
      ((((size_t)src) + size + pointer_size-1) & ~(pointer_size-1));
  size_t src_dir_idx = dir_index(src);
  size_t src_idx = tbl_index(src);
  while (1) {
    tbl_entry *src_entry = __CPI_DIR_GET(src_dir_idx++);

    unsigned char *next_tbl_addr = (unsigned char*) (
          src_dir_idx << (tbl_mask_bits + alignment_bits));
    if (CPI_EXPECT(end < next_tbl_addr)) {
      // Less then one table left; this is the last iteration
      if (CPI_EXPECT(src_entry))
        memset(src_entry + src_idx, 0, (tbl_index(end)-src_idx)*tbl_entry_size);
      return;
    }

    if (CPI_EXPECT(src_entry))
      memset(src_entry + src_idx, 0, (tbl_entry_num-src_idx) * tbl_entry_size);

    if (end == next_tbl_addr)
      return;

    src_idx = 0;
  }
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

// __attribute__((always_inline)) static
tbl_entry *__llvm__cpi_tbl_maybe_alloc(size_t dir_idx) {
  tbl_entry *tbl = __CPI_DIR_GET(dir_idx);
  if (CPI_EXPECTNOT(!tbl)) {
    tbl = tbl_alloc();
    __CPI_DIR_SET(dir_idx, tbl);
  }
  return tbl;
}

// Copy from src_entry table entry to dst
static inline
void __llvm__cpi_copy_table_to(unsigned char *dst,
                               tbl_entry *src_entry, size_t entries_to_copy) {
  DEBUG("[CPI] CopyTable to [%p, %p)\n",
        dst, dst + (entries_to_copy<<alignment_bits));
  size_t dst_dir_idx = dir_index(dst);
  size_t dst_tbl_idx = tbl_index(dst);

  // Are we going out of the src table?
  if (CPI_EXPECT(dst_tbl_idx + entries_to_copy <= tbl_entry_num)) {
    // Fast path: copy in one round.
    tbl_entry *dst_entry = __llvm__cpi_tbl_maybe_alloc(dst_dir_idx);
    memcpy(dst_entry + dst_tbl_idx, src_entry,
           entries_to_copy * tbl_entry_size);
  } else {
    // Slow path: copy in two rounds.
    tbl_entry *dst_entry = __llvm__cpi_tbl_maybe_alloc(dst_dir_idx);
    size_t entries_first = tbl_entry_num - dst_tbl_idx;
    memcpy(dst_entry + dst_tbl_idx, src_entry,
           entries_first * tbl_entry_size);

    dst_entry = __llvm__cpi_tbl_maybe_alloc(dst_dir_idx+1);
    memcpy(dst_entry, src_entry + entries_first,
           (entries_to_copy - entries_first) * tbl_entry_size);
  }
}

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
      ((((size_t)src) + size + pointer_size-1) & ~(pointer_size-1));
  size_t src_dir_idx = dir_index(src);
  size_t src_idx = tbl_index(src);
  while (1) {
    tbl_entry *src_entry = __CPI_DIR_GET(src_dir_idx++);
    //tbl_entry *src_entry = __llvm__cpi_tbl_maybe_alloc(src_dir_idx++);

    unsigned char *src_next_tbl_addr = (unsigned char*) (
          src_dir_idx << (tbl_mask_bits + alignment_bits));
    if (CPI_EXPECT(src_end < src_next_tbl_addr)) {
      // Less then or exactly one table left; this is the last iteration
      size_t num_entries = tbl_index(src_end) - src_idx;
      if (CPI_EXPECT(src_entry))
        __llvm__cpi_copy_table_to(dst, src_entry + src_idx, num_entries);
      else
        __llvm__cpi_do_delete_range(dst, num_entries << alignment_bits);
      return;
    }

    size_t num_entries = tbl_entry_num - src_idx;
    if (CPI_EXPECT(src_entry))
      __llvm__cpi_copy_table_to(dst, src_entry + src_idx, num_entries);
    else
      __llvm__cpi_do_delete_range(dst, num_entries << alignment_bits);

    if (src_end == src_next_tbl_addr)
      return;

    dst += num_entries << alignment_bits;
    src_idx = 0;
  }
}

// ---------------------------------------------

static inline
void __llvm__cpi_move_table_to(unsigned char *dst,
                               tbl_entry *src_entry, size_t entries_to_copy,
                               int forward) {
  size_t dst_dir_idx = dir_index(dst);
  size_t dst_tbl_idx = tbl_index(dst);

  // Are we going out of the src table?
  if (CPI_EXPECT(dst_tbl_idx + entries_to_copy <= tbl_entry_num)) {
    // Fast path: copy in one round.
    tbl_entry *dst_entry = __llvm__cpi_tbl_maybe_alloc(dst_dir_idx);
    memmove(dst_entry + dst_tbl_idx, src_entry,
            entries_to_copy * tbl_entry_size);

  } else if (forward) {
    // Slow path: copy in two rounds, forward
    tbl_entry *dst_entry = __llvm__cpi_tbl_maybe_alloc(dst_dir_idx);
    size_t entries_first = tbl_entry_num - dst_tbl_idx;
    memmove(dst_entry + dst_tbl_idx, src_entry,
            entries_first * tbl_entry_size);

    dst_entry = __llvm__cpi_tbl_maybe_alloc(dst_dir_idx+1);
    memmove(dst_entry, src_entry + entries_first,
            (entries_to_copy - entries_first) * tbl_entry_size);
  } else {
    // Slow path: copy in two rounds, backwards
    tbl_entry *dst_entry = __llvm__cpi_tbl_maybe_alloc(dst_dir_idx+1);
    size_t entries_first = tbl_entry_num - dst_tbl_idx;
    memmove(dst_entry, src_entry + entries_first,
            (entries_to_copy - entries_first) * tbl_entry_size);

    dst_entry = __llvm__cpi_tbl_maybe_alloc(dst_dir_idx);
    memmove(dst_entry + dst_tbl_idx, src_entry,
            entries_first * tbl_entry_size);
  }
}

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_move_range(unsigned char *dst, unsigned char *src,
                            size_t size) {
  DEBUG("[CPI] memmove [%p, %p) -> [%p, %p)%s%s%s\n",
        src, src + size, dst, dst + size,
        (((size_t)src)&(pointer_size-1)) ? " src misaligned":"",
        (((size_t)dst)&(pointer_size-1)) ? " dst misaligned":"",
        (size&(pointer_size-1)) ? " size misaligned":"");

  // Fast path: no overlap (we assume no overflow)
  if (CPI_EXPECT(src + size < dst || dst + size < src)) {
    __llvm__cpi_copy_range(dst, src, size);
    return;
  }

  if (CPI_EXPECTNOT((dst-src) & (pointer_size-1))) {
    // Misaligned copy; we can't support it so let's just delete dst
    __llvm__cpi_do_delete_range(dst, size);
    return;
  }

  // FIXME: in case of misaligned copy, we should clobber first and last entry
  unsigned char *src_end = (unsigned char*)
      ((((size_t)src) + size + pointer_size-1) & ~(pointer_size-1));

  if (src < dst) {
    // same as copy, but use memmove internally
    size_t src_dir_idx = dir_index(src);
    size_t src_idx = tbl_index(src);
    while (1) {
      tbl_entry *src_entry = __CPI_DIR_GET(src_dir_idx++);
      //tbl_entry *src_entry = __llvm__cpi_tbl_maybe_alloc(src_dir_idx++);

      unsigned char *src_next_tbl_addr = (unsigned char*) (
            src_dir_idx << (tbl_mask_bits + alignment_bits));
      if (CPI_EXPECT(src_end < src_next_tbl_addr)) {
        // Less then or exactly one table left; this is the last iteration
        size_t num_entries = tbl_index(src_end) - src_idx;
        if (CPI_EXPECT(src_entry))
          __llvm__cpi_move_table_to(dst, src_entry + src_idx, num_entries, 1);
        else
          __llvm__cpi_do_delete_range(dst, num_entries << alignment_bits);
        return;
      }

      size_t num_entries = tbl_entry_num - src_idx;
      if (CPI_EXPECT(src_entry))
        __llvm__cpi_move_table_to(dst, src_entry + src_idx, num_entries, 1);
      else
        __llvm__cpi_do_delete_range(dst, num_entries << alignment_bits);

      if (src_end == src_next_tbl_addr)
        return;

      dst += num_entries << alignment_bits;
      src_idx = 0;
    }
  } else if (src > dst) {
    // same as copy, but backwards and use memmove internally
    size_t src_dir_idx = dir_index(src_end);
    size_t src_end_idx = tbl_index(src_end); // could be zero, which is OK
    while (1) {
      tbl_entry *src_entry = __CPI_DIR_GET(src_dir_idx);
      //tbl_entry *src_entry = __llvm__cpi_tbl_maybe_alloc(src_dir_idx);

      unsigned char *src_tbl_addr = (unsigned char*) (
            src_dir_idx << (tbl_mask_bits + alignment_bits));
      if (CPI_EXPECT(src >= src_tbl_addr)) {
        // Less then or exactly one table left; this is the last iteration
        size_t src_idx = tbl_index(src);
        size_t num_entries = src_end_idx - src_idx;
        if (CPI_EXPECT(src_entry))
          __llvm__cpi_move_table_to(dst, src_entry + src_idx, num_entries, 0);
        else
          __llvm__cpi_do_delete_range(dst, num_entries << alignment_bits);
        return;
      }

      if (CPI_EXPECT(src_entry))
        __llvm__cpi_move_table_to(dst, src_entry, src_end_idx, 0);
      else
        __llvm__cpi_do_delete_range(dst, src_end_idx << alignment_bits);

      dst += src_end_idx << alignment_bits;
      src_end_idx = tbl_entry_num;
      --src_dir_idx;
    }
  }
}

// ---------------------------------------------
