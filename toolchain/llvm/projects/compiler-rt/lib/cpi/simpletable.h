#ifndef CPI_LOOKUP_TABLE_H
#define CPI_LOOKUP_TABLE_H

#include "cpi.h"

//-----------------------------------------------
// Type definitions
//-----------------------------------------------
#ifdef CPI_BOUNDS
typedef struct {
  void *ptr_value;
  void *reserved;
  __llvm__cpi_bounds bounds;
} tbl_entry;
#else
typedef struct {
  void *ptr_value;
} tbl_entry;
#endif

//-----------------------------------------------
// Constants
//-----------------------------------------------
#ifdef __i386__
#define CPI_TABLE_NUM_ENTRIES (1<<24)
#define alignment_bits 2
#define CPI_TABLE_ADDR (1ull<<24)

#define CPI_ADDR_MASK (0x0ull)
#warning NOT SUPPORTED
#endif // __i386__

#ifdef __riscv__
#  define CPI_ADDR_MASK (0x00fffffffff8ull) // this is ((1<<40)-1)&~7
#  define CPI_TABLE_NUM_ENTRIES (1ull<<(40 - 3))
#  define CPI_TABLE_ADDR (1ull<<45)
#endif

#ifdef __x86_64__

# if defined(__gnu_linux__)
// The following mask works for linux, it maps typical address space into
// a rage of non-overlapping addresses between 0 and 1<<40.
#  define CPI_ADDR_MASK (0x00fffffffff8ull) // this is ((1<<40)-1)&~7
#  define CPI_TABLE_NUM_ENTRIES (1ull<<(40 - 3))
#  define CPI_TABLE_ADDR (1ull<<45)
# elif defined(__FreeBSD__)
#  define CPI_ADDR_MASK (0x00fffffffff8ull) // this is ((1<<40)-1)&~7
#  define CPI_TABLE_NUM_ENTRIES (1ull<<(40 - 3))
#  define CPI_TABLE_ADDR (1ull<<45)
# elif defined(__APPLE__)
#  define CPI_ADDR_MASK (0x00fffffffff8ull) // this is ((1<<40)-1)&~7
#  define CPI_TABLE_NUM_ENTRIES (1ull<<(40 - 3))
#  define CPI_TABLE_ADDR (1ull<<45)
# else
#  error Not implemented yet
# endif

#define alignment_bits 3
#endif //  __x86_64__

#define pointer_size sizeof(void *)
#define pointer_mask (~(pointer_size-1))
#define tbl_entry_size_mult (sizeof(tbl_entry) / pointer_size)

// =============================================
// Global variable declarations
// =============================================
extern int __llvm__cpi_inited;

#if defined(__FreeBSD__) || defined(__APPLE__)
# define IMM_MODE "er"
#else
# define IMM_MODE "ir"
#endif

# define __CPI_TBL_GET(off)    \
  ({ size_t val;                \
      __asm__ volatile ("movq %%gs:(%1),%0"  \
               : "=r" (val)         \
               : "r" (off));        \
     val; })

# define __CPI_TBL_SET(off, val)                     \
  do { __asm__ volatile ("movq %0,%%gs:(%1)" :        \
                         : IMM_MODE (val),            \
                         "r" (off));                  \
  } while(0)

# define __CPI_TBL_GET2(off, b)      \
  ({ size_t val;                      \
      __asm__ volatile ("movq %%gs:%P2(%1),%0" \
               : "=r" (val)           \
               : "r" (off), "i" (b)); \
     val; })

# define __CPI_TBL_SET2(off, b, val)             \
  do { __asm__ volatile ("movq %0,%%gs:%P2(%1)" : \
                         : IMM_MODE (val),        \
                         "r" (off), "i" (b));     \
  } while(0)

//-----------------------------------------------
// Helper functions for indexing
//-----------------------------------------------
#define tbl_offset(ptr_address) \
  ((((size_t)(ptr_address)) & CPI_ADDR_MASK) * tbl_entry_size_mult)

#define tbl_address(ptr_address) \
  ((tbl_entry*) (((char*) __llvm__cpi_table) + tbl_offset(ptr_address)))

#endif // CPI_LOOKUP_TABLE_H
