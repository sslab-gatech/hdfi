#ifndef CPI_HASH_TABLE_H
#define CPI_HASH_TABLE_H

#include "cpi.h"

// =============================================
// Hash table related configurations
// =============================================
#define ASSUME_8B_ALIGNED
//#define CPI_HASH_STAT
//#define CPI_ST_USE_CAS
#define CPI_MAX_LOOP 1024

// =============================================
// Type definitions for hash table entries
// =============================================
#ifdef CPI_BOUNDS
typedef struct {
  void *ptr;
  void *value;
  __llvm__cpi_bounds bounds;
} __llvm__cpi_ptrval;
#else
typedef struct {
  void *ptr;
  void *value;
} __llvm__cpi_ptrval;
#endif

// =============================================
// Helper definitions for hash table implementation
// =============================================
#ifndef ST_NRBITS
#ifdef CPI_BOUNDS
#define ST_NRBITS 28
#else
#define ST_NRBITS 28
#endif
#endif // defined(ST_NRBITS)
#define ST_SIZE ((0x1 << ST_NRBITS) * sizeof(__llvm__cpi_ptrval))
#define ST_MAXENTRIES (0x1 << ST_NRBITS)

#ifndef ST_SHIFT_BITS
#define ST_SHIFT_BITS 2
#endif // !defined(ST_SHIFT_BITS)

// ASSUMPTION: pointers are sizeof(void*) aligned
#define ST_MAPPING_PATTERN ((ST_SIZE - 1) ^ (sizeof(__llvm__cpi_ptrval) - 1))
#define ST_GET_OFFSET(addr)                                                    \
  ((((uintptr_t)(addr)) << ST_SHIFT_BITS) & ST_MAPPING_PATTERN)
#define ST_GET_ADDR_FROM_OFFSET(offset)                                        \
  ((__llvm__cpi_ptrval *)(((unsigned char *)__llvm__cpi_shadowmemory) + offset))
#define ST_GET_ADDR(addr) ST_GET_ADDR_FROM_OFFSET(ST_GET_OFFSET(addr));

#ifndef ASSUME_8B_ALIGNED
#define ALIGNED(ptr) (((unsigned long)ptr) & (~(sizeof(void *) - 1)))
#define ALIGNED_OFFSET(offset, ptr) ST_GET_OFFSET(ptr)
#else
#define ALIGNED(ptr) ptr
#define ALIGNED_OFFSET(offset, ptr) offset
#endif // !defined(ASSUME_8B_ALIGNED)

// =============================================
// Declarations for hash stat
// =============================================
#ifdef CPI_HASH_STAT
#define CPI_FPTR_LOG "/tmp/cpi_hash_stat"
extern long long __llvm__cpi_nrhit;
extern long long __llvm__cpi_nrmiss1;
extern long long __llvm__cpi_nrmiss2;
extern long long __llvm__cpi_nrmiss3;
extern long long __llvm__cpi_nrmiss4;
extern long long __llvm__cpi_nrmiss5;
extern long long __llvm__cpi_nrmissx;
extern long long __llvm__cpi_nrset0;
extern long long __llvm__cpi_nrset1;
extern long long __llvm__cpi_nrset2;
extern long long __llvm__cpi_nrset3;
extern long long __llvm__cpi_nrset4;
extern long long __llvm__cpi_nrset5;
extern long long __llvm__cpi_nrsetx;
void __llvm__cpi_statistic();
#endif // CPI_HASH_STAT

// =============================================
// Global variable declarations
// =============================================
#ifdef CPI_ST_STATIC
//extern __llvm__cpi_ptrval __llvm__cpi_shadowmemory[1 << ST_NRBITS]
//    __attribute__((aligned(1 << ST_NRBITS)));
#define __llvm__cpi_shadowmemory ((__llvm__cpi_ptrval*) 0x100000000000)
#else
extern __llvm__cpi_ptrval *__llvm__cpi_shadowmemory;
#endif

extern __llvm__cpi_ptrval __llvm__cpi_null_entry;

extern int __llvm__cpi_inited;

// =============================================
// Local function declarations
// =============================================

__CPI_EXPORT void __llvm__cpi_destroy(void);

#ifdef CPI_BOUNDS
__CPI_HIDDEN void __llvm__cpi_set_slow(void **fptr, unsigned long offset,
                                        void *val, __llvm__cpi_bounds bounds);
#else
__CPI_HIDDEN void __llvm__cpi_set_slow(void **fptr, unsigned long offset,
                                        void *val);
#endif // CPI_BOUNDS

__CPI_INLINE __llvm__cpi_ptrval *__llvm__cpi_get_boundary_addr_in_st(unsigned char *fptr);

__CPI_INLINE __llvm__cpi_ptrval *__llvm__cpi_get(void **fptr);
__CPI_INLINE __llvm__cpi_ptrval *__llvm__cpi_get_or_null(void **fptr);

__CPI_HIDDEN __llvm__cpi_ptrval *__llvm__cpi_get_slow(void **fptr,
                                                  unsigned long offset);

__CPI_INLINE void __llvm__cpi_delete(void **fptr);
__CPI_HIDDEN void __llvm__cpi_delete_slow(void **fptr, unsigned long offset);

// =============================================
// Stat profile functions
// =============================================

              void __llvm__cpi_statistic();

__CPI_EXPORT void __llvm__cpi_register_profile_table(void * table, size_t size);
              int __llvm__cpi_profile_item_compar(const void * p1, const void * p2);
              void __llvm__cpi_profile_statistic();

#endif // CPI_HASH_TABLE_H
