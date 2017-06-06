#ifdef __APPLE__
#include <malloc/malloc.h>
#elif __gnu_linux__
#include <malloc.h>
#elif __FreeBSD__
#include <malloc_np.h>
#endif

#include "common.h"

// =============================================
// Store compatibility interface
// =============================================

/*** Interface function ***/
#ifdef CPI_BOUNDS
__CPI_INLINE 
void __llvm__cpi_set(void **fptr, void *val) {
  __llvm__cpi_set_bounds(fptr, val, __llvm__cpi_bounds_infty);
}
#else
__CPI_INLINE 
void __llvm__cpi_set_bounds(void **fptr, void *val, __llvm__cpi_bounds bounds) {
  __llvm__cpi_set(fptr, val);
}
#endif

// =============================================
// Load compatibility interface
// =============================================

/*** Interface function ***/
#ifdef CPI_BOUNDS
__CPI_INLINE 
void __llvm__cpi_assert_bounds(void *val, size_t size, __llvm__cpi_bounds bounds,
                               char *loc) {
  if (CPI_EXPECTNOT((uintptr_t)(val) < bounds[0] ||
                     (uintptr_t)(val) + size - 1 > bounds[1])) {
#ifdef CPI_VERBOSE_ERRORS
    __llvm__cpi_assert_bounds_fail(val, size, bounds, loc);
#else
    __llvm__cpi_assert_bounds_fail();
#endif
  }
}
#else
__CPI_INLINE 
void __llvm__cpi_assert_bounds(void *val, size_t size, __llvm__cpi_bounds bounds,
                               char *loc) {}
#endif

// =============================================
// Memory management reletad
// =============================================

/*** Interface function ***/
__CPI_INLINE 
unsigned long __llvm__cpi_malloc_size(unsigned char *fptr) {
#ifdef __APPLE__
    return malloc_size(fptr);
#else //__gnu_linux__ or __FreeBSD__
    return malloc_usable_size(fptr);
#endif
}

// =============================================

/*** Interface function ***/
__CPI_INLINE 
void __llvm__cpi_alloc(unsigned char *fptr) {
#ifdef CPI_DELETE_ON_ALLOC
    if (CPI_EXPECT((long)fptr))
        __llvm__cpi_delete_range(fptr, __llvm__cpi_malloc_size(fptr));
#endif
}

// =============================================

/*** Interface function ***/
__CPI_INLINE
void __llvm__cpi_free(unsigned char *fptr) {
#ifdef CPI_DELETE_ON_FREE
    if (CPI_EXPECT((long)fptr))
        __llvm__cpi_delete_range(fptr, __llvm__cpi_malloc_size(fptr));
#endif
}

// =============================================
// Arg bounds
// =============================================

/*** Interface function ***/
__CPI_INLINE 
void __llvm__cpi_set_arg_bounds(uint32_t arg_no, __llvm__cpi_bounds bounds) {
#ifdef CPI_BOUNDS
# ifdef __CPI_USE_TCB_ARGB
  __CPI_ARG_BOUNDS_SET(arg_no, bounds);
# else 
  __llvm__cpi_arg_bounds[arg_no] = bounds;
# endif //__CPI_USE_TCB_ARGB
#endif
}

// =============================================

/*** Interface function ***/
__CPI_INLINE 
__llvm__cpi_bounds __llvm__cpi_get_arg_bounds(uint32_t arg_no) {
#ifdef CPI_BOUNDS
# ifdef __CPI_USE_TCB_ARGB
  return __CPI_ARG_BOUNDS_GET(arg_no);
# else 
  return __llvm__cpi_arg_bounds[arg_no];
# endif //__CPI_USE_TCB_ARGB
#else
  return __llvm__cpi_bounds_infty;
#endif
}

// =============================================
