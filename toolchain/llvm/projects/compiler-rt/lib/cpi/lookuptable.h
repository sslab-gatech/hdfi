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
/* MPX model:
 * 
 *  31                12 11       2  0
 * +--------------------+----------+--+
 * |      Directory     |   Table  |  |
 * +--------------------+----------+--+
 */
static const int alignment_bits = 2;
static const int tbl_mask_bits = 10;
static const int dir_mask_bits = 20;
#endif // __i386__

#ifdef __x86_64__
/* MPX model:
 * 
 *  63               47                        20 19              3   0
 * +----------------+----------------------------+-----------------+---+
 * |      ---       |        Directory           |      Table      |   |
 * +----------------+----------------------------+-----------------+---+
 */
static const int alignment_bits = 3;
static const int tbl_mask_bits = 17;
static const int dir_mask_bits = 28;

/* Softbound model: */
/* static const int alignment_bits = 3; */
/* static const int tbl_mask_bits = 22; */
/* static const int dir_mask_bits = 23; */
#endif //  __x86_64__

static const size_t tbl_entry_num = ((size_t)1) << tbl_mask_bits;
static const size_t dir_entry_num = ((size_t)1) << dir_mask_bits;

static const size_t tbl_entry_size = sizeof(tbl_entry);
static const size_t dir_entry_size = sizeof(tbl_entry *);

static const size_t tbl_size = tbl_entry_num * tbl_entry_size;
static const size_t dir_size = dir_entry_num * dir_entry_size;

static const size_t pointer_size = sizeof(void *);

// =============================================
// Global variable declarations
// =============================================
extern int __llvm__cpi_inited;

#if defined(CPI_USE_SEGMENT)

# ifdef __pic__
#  define IMM_MODE "nr"
# else
#  define IMM_MODE "ir"
# endif

# define __CPI_DIR_GET(idx)                  \
  ({ tbl_entry *tbl;                          \
      __asm__ volatile ("movq %%gs:(,%1,%P2),%0"           \
           : "=r" (tbl)                       \
           : "r" (idx), "i" (pointer_size));  \
      tbl; })

# define __CPI_DIR_SET(idx, value)                       \
  do { __asm__ volatile ("movq %0,%%gs:(,%1,%P2)" :           \
                     : IMM_MODE ((unsigned long) (value)),\
                     "r" (idx), "i" (pointer_size));      \
  } while(0)

#elif defined CPI_ST_STATIC
//tbl_entry *__llvm__cpi_dir[dir_entry_num]
//  __attribute__((aligned(0x1000)));
# define __llvm__cpi_dir ((tbl_entry**) 0x100000000000)
# define __CPI_DIR_GET(idx) __llvm__cpi_dir[idx]
# define __CPI_DIR_SET(idx, value) do { __llvm__cpi_dir[idx] = value; } while(0)
#else
extern tbl_entry **__llvm__cpi_dir;
# define __CPI_DIR_GET(idx) __llvm__cpi_dir[idx]
# define __CPI_DIR_SET(idx, value) do { __llvm__cpi_dir[idx] = value; } while(0)
#endif

//-----------------------------------------------
// Helper functions for indexing
//-----------------------------------------------
// __attribute__((always_inline)) static
size_t dir_index(void *ptr_address) {
  size_t index = ((size_t)ptr_address) >> (tbl_mask_bits + alignment_bits);
  // Masking is not necessary in user-space (lower half)
  /* size_t mask = (1 << dir_mask_bits) - 1; */
  /* return index & mask; */
  return index;
}

// __attribute__((always_inline)) static
size_t tbl_index(void *ptr_address) {
  static const size_t mask = (((size_t)1) << tbl_mask_bits) - 1;
  size_t index = ((size_t)ptr_address) >> (alignment_bits);
  return index & mask;
}

#endif // CPI_LOOKUP_TABLE_H
