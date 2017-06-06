#ifndef CPI_COMMON_H
#define CPI_COMMON_H
// =============================================

#include "cpi.h"

// =============================================
// Global variable declarations
// =============================================
#ifdef CPI_NOFAIL
extern uint64_t __llvm__cpi_num_fails;
#endif

#ifdef CPI_BOUNDS
# ifdef __FreeBSD__
#  define __CPI_USE_TCB_ARGB
# else
extern __llvm__cpi_bounds __llvm__cpi_arg_bounds[64];
# endif
#endif

// =============================================
// arg_bounds TCB hack for FreeBSD
// =============================================
#ifdef __CPI_USE_TCB_ARGB
// NOTE: Using movaps, the aligned version of movups, would be the optimal, but
// somehow the TCB structure is not aligned to 16 byte.
#define __CPI_BOUNDS_OFFSET 0x20
#define __CPI_ARG_BOUNDS_SET(arg_no, bounds)                                  \
  ({                                                                           \
    __asm__ volatile(                                                          \
        "movups %0, %%fs:%1"                                                   \
        :                                                                      \
        : "x"(bounds),                                                         \
          "m"(*(volatile void *)(__CPI_BOUNDS_OFFSET +                        \
                                 (arg_no * sizeof(__llvm__cpi_bounds)))));          \
  })
#define __CPI_ARG_BOUNDS_GET(arg_no)                                          \
  ({                                                                           \
    __llvm__cpi_bounds __bounds;                                                    \
    __asm__ volatile(                                                          \
        "movups %%fs:%1, %0"                                                   \
        : "=x"(__bounds)                                                       \
        : "m"(*(volatile void *)(__CPI_BOUNDS_OFFSET +                        \
                                 (arg_no * sizeof(__llvm__cpi_bounds)))));          \
    __bounds;                                                                  \
  })
#endif // __CPI_USE_TCB_ARGB

// =============================================
// Local function declarations
// =============================================
#ifdef CPI_VERBOSE_ERRORS
__CPI_EXPORT __CPI_NOINLINE
    void __llvm__cpi_assert_fail(void **fptr, void *val, char *loc);
__CPI_EXPORT __CPI_NOINLINE
  void __llvm__cpi_assert_bounds_fail(void *val, size_t size,
                                      __llvm__cpi_bounds bounds, char *loc);
#else
__CPI_EXPORT __CPI_NOINLINE void __llvm__cpi_assert_fail();
__CPI_EXPORT __CPI_NOINLINE void __llvm__cpi_assert_bounds_fail();
#endif // CPI_VERBOSE_ERRORS

// =============================================
// Declarations for profile stat
// =============================================
#ifdef CPI_PROFILE_STATS
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#define CPI_PROFILE_LOG "/tmp/cpi_profile_stat"
typedef struct {
  size_t num;
  char *name;
} CPIProfileStat;

typedef struct {
  CPIProfileStat *table;
  size_t size;
} CPIProfileTable;

extern CPIProfileTable __llvm__cpi_stats_dir[256];
extern size_t __llvm__cpi_stats_dir_size;
extern size_t __llvm__cpi_stats_total_size;
#endif // CPI_PROFILE_STATS

// =============================================
#endif // CPI_COMMON_H
