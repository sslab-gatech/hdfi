//=====================================================
// Inlined functions for the lookup table
//=====================================================
#include <assert.h>
#include <sys/mman.h>

#include "simpletable.h"

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

  size_t off = tbl_offset(ptr_address);
  __CPI_TBL_SET(off, ptr_value);

#ifdef CPI_BOUNDS
  __CPI_TBL_SET2(off, pointer_size*2, bounds[0]);
  __CPI_TBL_SET2(off, pointer_size*3, bounds[1]);
#endif
}

// =============================================
// Load functions
// =============================================

__CPI_INLINE
const tbl_entry *__llvm__cpi_get(void **ptr_address) {
  return (tbl_entry*) tbl_offset(ptr_address);
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
__llvm__cpi_bounds __llvm__cpi_assert(void **ptr_address, void *ptr_value,
                                 char *loc) {

  DEBUG("[CPI] Check [%p] : %p (%s)\n", ptr_address, ptr_value, loc);

  size_t off = tbl_offset(ptr_address);
  void *tbl_value = (void*) __CPI_TBL_GET(off);

  // If the pointer value does not match -> fail!
  if (CPI_EXPECTNOT(tbl_value != ptr_value)) {
#ifdef CPI_VERBOSE_ERRORS
    __llvm__cpi_assert_fail(ptr_address, ptr_value, loc);
#else
    __llvm__cpi_assert_fail();
#endif
  }

// Return bounds if pointer values matched
#ifdef CPI_BOUNDS
  __llvm__cpi_bounds bounds;
  bounds[0] = __CPI_TBL_GET2(off, pointer_size*2);
  bounds[1] = __CPI_TBL_GET2(off, pointer_size*3);
  return bounds;
#else
  return __llvm__cpi_bounds_infty;
#endif
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
void *__llvm__cpi_get_metadata(void **ptr_address) {
  return (void*) tbl_offset(ptr_address);
}

__CPI_INLINE
void *__llvm__cpi_get_metadata_nocheck(void **ptr_address) {
  return (void*) tbl_offset(ptr_address);
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
void *__llvm__cpi_get_val(void *metadata) {
  return (void*) __CPI_TBL_GET((size_t) metadata);
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
__llvm__cpi_bounds __llvm__cpi_get_bounds(void *metadata) {
#ifdef CPI_BOUNDS
  __llvm__cpi_bounds bounds;
  bounds[0] = __CPI_TBL_GET2((size_t)metadata, pointer_size*2);
  bounds[1] = __CPI_TBL_GET2((size_t)metadata, pointer_size*3);
  return bounds;
#else
  return __llvm__cpi_bounds_infty;
#endif
}

// =============================================
