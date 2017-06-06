//=====================================================
// Inlined functions for the lookup table
//=====================================================
#include <assert.h>
#include <sys/mman.h>

#include "lookuptable.h"

// =============================================
// Table allocation
// =============================================

// Must be noinline in order to reduce the icache pressure on the hot paths
__attribute__((noinline)) __attribute__((weak))
tbl_entry *tbl_alloc() {
  tbl_entry *tbl = (tbl_entry*)
      mmap(0, tbl_size, PROT_READ | PROT_WRITE, CPI_TBL_MMAP_FLAGS, -1, 0);

  assert(tbl != (void *)-1);

  return tbl;
}

// =============================================
// Store functions
// =============================================

/*** Interface function ***/
__CPI_INLINE
#ifdef CPI_BOUNDS
void __llvm__cpi_set_bounds(void **ptr_address, void *ptr_value,
                            __llvm__cpi_bounds bounds) {
#else
void __llvm__cpi_set(void **ptr_address, void *ptr_value) {
#endif

  DEBUG("[CPI] Store [%p] : %p\n", ptr_address, ptr_value);

  size_t dir_idx = dir_index(ptr_address);
  tbl_entry *tbl = __CPI_DIR_GET(dir_idx);

  // Is there a directory entry?
  if (CPI_EXPECTNOT(!tbl)) {
    __CPI_DIR_SET(dir_idx, tbl = tbl_alloc());
  }

  size_t tbl_idx = tbl_index(ptr_address);
  tbl_entry *entry = &tbl[tbl_idx];

  entry->ptr_value = ptr_value;
#ifdef CPI_BOUNDS
  entry->bounds = bounds;
#endif
}

// =============================================
// Load functions
// =============================================

#ifdef CPI_BOUNDS
static const tbl_entry __llvm__cpi_null_entry = { 0, 0, __llvm__cpi_bounds_null };
#else
static const tbl_entry __llvm__cpi_null_entry = { 0 };
#endif

__CPI_INLINE
const tbl_entry *__llvm__cpi_get(void **ptr_address) {
  size_t dir_idx = dir_index(ptr_address);
  tbl_entry *tbl = __CPI_DIR_GET(dir_idx);

  // Is there a directory entry?
  if (CPI_EXPECTNOT(!tbl)) {
    return &__llvm__cpi_null_entry;
    //__CPI_DIR_SET(dir_idx, tbl = tbl_alloc());
  }

  return tbl + tbl_index(ptr_address);
}

__CPI_INLINE
const tbl_entry *__llvm__cpi_get_nocheck(void **ptr_address) {
  size_t dir_idx = dir_index(ptr_address);
  tbl_entry *tbl = __CPI_DIR_GET(dir_idx);
  // Let it crash if tbl is NULL as soon as the result is dereferenced.
  // NOTE: we also map the NULL tbl to read-only all-zero area.

  return tbl + tbl_index(ptr_address);
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
__llvm__cpi_bounds __llvm__cpi_assert(void **ptr_address, void *ptr_value,
                                 char *loc) {

  DEBUG("[CPI] Check [%p] : %p (%s)\n", ptr_address, ptr_value, loc);

  const tbl_entry *entry = __llvm__cpi_get(ptr_address);

#if 0
  // XXX: this is accounted for in __llvm__cpi_get
  // If no entry -> it's okay to be null..
  if (CPI_EXPECTNOT(!entry)) {
    DEBUG("[CPI] ^^^^^ unkown address!\n");
    return __llvm__cpi_bounds_null;
  }
#endif

  // If the pointer value does not match -> fail!
  if (CPI_EXPECTNOT(entry->ptr_value != ptr_value)) {
#ifdef CPI_VERBOSE_ERRORS
    __llvm__cpi_assert_fail(ptr_address, ptr_value, loc);
#else
    __llvm__cpi_assert_fail();
#endif
  }

// Return bounds if pointer values matched
#ifdef CPI_BOUNDS
  return entry->bounds;
#else
  return __llvm__cpi_bounds_infty;
#endif
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
void *__llvm__cpi_get_metadata(void **ptr_address) {
  return (void *)__llvm__cpi_get(ptr_address);
}

__CPI_INLINE
void *__llvm__cpi_get_metadata_nocheck(void **ptr_address) {
  return (void *)__llvm__cpi_get_nocheck(ptr_address);
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
void *__llvm__cpi_get_val(void *metadata) {
  return ((tbl_entry *)metadata)->ptr_value;
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
__llvm__cpi_bounds __llvm__cpi_get_bounds(void *metadata) {
#ifdef CPI_BOUNDS
  return ((tbl_entry *)metadata)->bounds;
#else
  return __llvm__cpi_bounds_infty;
#endif
}

// =============================================
