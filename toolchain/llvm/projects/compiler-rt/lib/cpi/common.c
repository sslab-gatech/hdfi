#include <stdio.h>
#include <stdlib.h>

#include "common.h"

// =============================================
// Common global variable definitions
// =============================================
#if defined(CPI_NOFAIL) || defined(CPI_BOUNDS_NOFAIL)
uint64_t __llvm__cpi_num_fails = 0;
#endif

#ifdef CPI_BOUNDS
# ifdef __FreeBSD__
__thread
# endif
__llvm__cpi_bounds __llvm__cpi_arg_bounds[64];
#endif

// =============================================
// Memory management related functions
// =============================================

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_realloc(unsigned char *fptr_new, unsigned long size_new,
                         unsigned char *fptr_old, unsigned long size_old) {
    if (CPI_EXPECTNOT(fptr_old == NULL || fptr_new == NULL)) {
        return;
    } else if (fptr_old == fptr_new) {
#ifdef CPI_DELETE_ON_FREE
        /*
        if (size_new < size_old) // shrink
            __llvm__cpi_delete_range(fptr_old + size_new, size_old - size_new);
        */
#endif
#ifdef CPI_DELETE_ON_ALLOC
        /*
        if (size_new > size_old) // enlarge
            __llvm__cpi_delete_range(fptr_old + size_new, size_old - size_new);
        */
#endif
    } else { // data was moved
        __llvm__cpi_move_range(fptr_new, fptr_old,
                               size_old < size_new ? size_old : size_new);
#ifdef CPI_DELETE_ON_FREE
        // FIXME: need to check for overlap
        //__llvm__cpi_delete_range(fptr_old, size_old);
#endif
    }
}

// =============================================
// Failure reporting functions
// =============================================

__CPI_EXPORT __CPI_NOINLINE
#ifndef CPI_NOFAIL
    __attribute__((noreturn))
#endif
#ifdef CPI_VERBOSE_ERRORS
    void __llvm__cpi_assert_fail(void **fptr, void *val, char *loc) {
#else
    void __llvm__cpi_assert_fail() {
#endif
#ifdef CPI_NOFAIL
  ++__llvm__cpi_num_fails;
#else
# ifdef CPI_VERBOSE_ERRORS
  fprintf(stderr, "CPI check fail at %s:\n", loc);
  __llvm__cpi_dump(fptr);
# else
  fprintf(stderr, "CPI check fail\n");
# endif
  abort();
#endif
}

// =============================================

__CPI_EXPORT __CPI_NOINLINE
#ifndef CPI_NOFAIL
    __attribute__((noreturn))
#endif
#ifdef CPI_VERBOSE_ERRORS
    void __llvm__cpi_assert_bounds_fail(void *val, size_t size,
                                        __llvm__cpi_bounds bounds, char *loc) {
#else
    void __llvm__cpi_assert_bounds_fail() {
#endif
#if defined(CPI_NOFAIL) || defined(CPI_BOUNDS_NOFAIL)
  ++__llvm__cpi_num_fails;
#else
# ifdef CPI_VERBOSE_ERRORS
  fprintf(stderr, "CPI bounds check fail at %s:\n"
                  "val=%p, size=0x%lx, bounds=[0x%lx, 0x%lx]\n",
          loc, val, size, bounds[0], bounds[1]);
# else
  fprintf(stderr, "CPI bounds check fail\n");
# endif
  abort();
#endif
}

// =============================================
// Profile stat
// =============================================
#ifdef CPI_PROFILE_STATS
// --------------------------------------------

CPIProfileTable __llvm__cpi_stats_dir[256];
size_t __llvm__cpi_stats_dir_size = 0;
size_t __llvm__cpi_stats_total_size = 0;

extern char* program_invocation_short_name;

__CPI_EXPORT
void __llvm__cpi_register_profile_table(void *table, size_t size) {
    if (__llvm__cpi_stats_dir_size >= 256) {
        fprintf(stderr, "CPI: profile table overflow (too many modules)\n");
        abort();
    }
    __llvm__cpi_stats_dir[__llvm__cpi_stats_dir_size].table = (CPIProfileStat *)table;
    __llvm__cpi_stats_dir[__llvm__cpi_stats_dir_size].size = size;
    __llvm__cpi_stats_total_size += size;
    __llvm__cpi_stats_dir_size += 1;
}

// --------------------------------------------

int __llvm__cpi_profile_item_compar(const void *p1, const void *p2) {
    return ((CPIProfileStat *)p2)->num - ((CPIProfileStat *)p1)->num;
}

// --------------------------------------------

void __llvm__cpi_profile_statistic() {
    if (__llvm__cpi_stats_total_size == 0)
        return;

    CPIProfileStat *table = (CPIProfileStat *)malloc(__llvm__cpi_stats_total_size *
                                                       sizeof(CPIProfileStat));
    CPIProfileStat *table_ptr = table;

    size_t i;
    for (i = 0; i < __llvm__cpi_stats_dir_size; ++i) {
        memcpy(table_ptr, __llvm__cpi_stats_dir[i].table,
               __llvm__cpi_stats_dir[i].size * sizeof(CPIProfileStat));
        table_ptr += __llvm__cpi_stats_dir[i].size;
    }

    qsort(table, __llvm__cpi_stats_total_size, sizeof(CPIProfileStat),
          __llvm__cpi_profile_item_compar);

    char fname[512];

    char tbuf[64];
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    strftime(tbuf, sizeof(tbuf), "%FT%T", tm_now);

#ifdef __gnu_linux__
    snprintf(fname, sizeof(fname), CPI_PROFILE_LOG "-%s-%s-%d",
             program_invocation_short_name, tbuf, getpid());
#else
    snprintf(fname, sizeof(fname), CPI_PROFILE_LOG "-%s-%s-%d",
             getprogname(), tbuf, getpid());
#endif
    FILE *fd = fopen(fname, "w");
    for (i = 0; i < __llvm__cpi_stats_total_size; ++i) {
        fprintf(fd, "%zu\t%s\n", table[i].num, table[i].name);
    }
    fclose(fd);
}

// --------------------------------------------
#endif // defined(CPI_PROFILE_STATS)

// =============================================
