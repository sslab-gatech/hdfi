//=====================================================
// Inlined functions for the hashtable
//=====================================================

#include "hashtable.h"

// =============================================
// Helper functions
// =============================================

__CPI_INLINE 
__llvm__cpi_ptrval *__llvm__cpi_get_boundary_addr_in_st(unsigned char *fptr) {
    unsigned long offset = ST_GET_OFFSET(fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    // we have to check if the actual addr is shifted due to collisions
    // let's loop if the end addr is not (yet) the correct addr.

    // unfortunately also earlier addresses can be set after our address is set.
    // if this happens then these addresses are in the table after the last
    // address so
    // addresses must not be sequential.
    while (ptr->ptr != NULL) {
        offset = (offset + sizeof(__llvm__cpi_ptrval)) & (ST_SIZE - 1);
        ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    }
    return ptr;
}

// =============================================
// Load functions
// =============================================

__CPI_INLINE 
__llvm__cpi_ptrval *__llvm__cpi_get(void **fptr) {
    unsigned long offset = ST_GET_OFFSET(fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    if (CPI_EXPECT(ptr->ptr == fptr)) {
#ifdef CPI_HASH_STAT
        ++__llvm__cpi_nrhit;
#endif
        DEBUG("Get %p (val: %p) ptr: %p\n", fptr, ptr->value, ptr);
        return ptr;
    }
    ptr = __llvm__cpi_get_slow(fptr, offset);
    DEBUG("Get %p val: %p ptr: %p ptr->ptr: %p\n", fptr, ptr->value, ptr,
          ptr->ptr);
    return ptr;
}

// =============================================

// same as __llvm__cpi_get but can return an entry with ptr->ptr == NULL
__CPI_INLINE 
__llvm__cpi_ptrval *__llvm__cpi_get_or_null(void **fptr) {
    unsigned long offset = ST_GET_OFFSET(fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    if (CPI_EXPECT(ptr->ptr == NULL || ptr->ptr == fptr)) {
#ifdef CPI_HASH_STAT
        ++__llvm__cpi_nrhit;
#endif
        DEBUG("Get %p (val: %p) ptr: %p\n", fptr, ptr->value, ptr);
        return ptr;
    }
    ptr = __llvm__cpi_get_slow(fptr, offset);
    DEBUG("Get %p val: %p ptr: %p ptr->ptr: %p\n", fptr, ptr->value, ptr,
          ptr->ptr);
    return ptr;
}


// =============================================

/*** Interface function ***/
__CPI_INLINE 
__llvm__cpi_bounds __llvm__cpi_assert(void **fptr, void *val, char *loc) {
  __llvm__cpi_ptrval *ptr = __llvm__cpi_get(fptr);
  DEBUG("Check %p val: %p ptr: %p ptr->val: %p ptr->ptr: %p\n", fptr, val, ptr,
        ptr->value, ptr->ptr);
  if (CPI_EXPECTNOT(val != ptr->value)) {
#ifdef CPI_VERBOSE_ERRORS
    __llvm__cpi_assert_fail(fptr, val, loc);
#else
    __llvm__cpi_assert_fail();
#endif
  }
#ifdef CPI_BOUNDS
  return ptr->bounds;
#else
  return __llvm__cpi_bounds_infty;
#endif
}

// =============================================

/*** Interface function ***/
__CPI_INLINE 
void *__llvm__cpi_get_metadata(void **fptr) {
  return (void *)__llvm__cpi_get(fptr);
}

__CPI_INLINE
void *__llvm__cpi_get_metadata_nocheck(void **fptr) {
  return (void *)__llvm__cpi_get(fptr);
}

// =============================================

/*** Interface function ***/
__CPI_INLINE void *__llvm__cpi_get_val(void *metadata) {
  return ((__llvm__cpi_ptrval *)metadata)->value;
}

// =============================================

/*** Interface function ***/
__CPI_INLINE 
__llvm__cpi_bounds __llvm__cpi_get_bounds(void *metadata) {
#ifdef CPI_BOUNDS
  return ((__llvm__cpi_ptrval *)metadata)->bounds;
#else
  return __llvm__cpi_bounds_infty;
#endif
}

// =============================================
// Store functions
// =============================================

/*** Interface function ***/
__CPI_INLINE
#ifdef CPI_BOUNDS
void __llvm__cpi_set_bounds(void **fptr, void *val, __llvm__cpi_bounds bounds) {
#else
void __llvm__cpi_set(void **fptr, void *val) {
#endif
    unsigned long offset = ST_GET_OFFSET(fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    DEBUG("Set %p (val: %p) ptr: %p (old val: %p, ptr: %p)\n", fptr, val, ptr,
          ptr->value, ptr->ptr);
    if (CPI_EXPECT(ptr->ptr == NULL || ptr->ptr == fptr)) {
#ifdef CPI_HASH_STAT
        ++__llvm__cpi_nrset0;
        if (ptr->ptr == NULL)
          ++__llvm__cpi_nrsetx;
#endif
        // FIXME: Support CPI_ST_USE_CAS here
        ptr->ptr = fptr;
        ptr->value = val;
#ifdef CPI_BOUNDS
        ptr->bounds = bounds;
#endif
        return;
    }
#ifdef CPI_BOUNDS
    __llvm__cpi_set_slow(fptr, offset, val, bounds);
#else
    __llvm__cpi_set_slow(fptr, offset, val);
#endif
}

// =============================================
// Deletion functions
// =============================================

__CPI_INLINE 
void __llvm__cpi_delete(void **fptr) {
    unsigned long offset = ST_GET_OFFSET(fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    if (CPI_EXPECT(ptr->ptr == fptr)) {
        ptr->value = NULL;
#ifdef CPI_BOUNDS
        ptr->bounds = __llvm__cpi_bounds_empty;
#endif // defined(CPI_BOUNDS)
        return;
    }
    if (CPI_EXPECT(ptr->ptr == NULL))
        return;
    __llvm__cpi_delete_slow(fptr, offset);
}

// =============================================
