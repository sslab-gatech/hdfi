/**
 * @file hashtable.cc
 * Implementation of the CPI hash table
 *
 * Copyright (c) 2013 UC Berkeley, EPFL
 * @author Mathias Payer <mathias.payer@nebelwelt.net>
 * @author Volodymyr Kuznetsov <ks.vladimir@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifdef __APPLE__
#include <malloc/malloc.h>
#elif __gnu_linux__
#define _GNU_SOURCE 1
#include <errno.h>
#include <malloc.h>
#elif __FreeBSD__
#include <malloc_np.h>
#endif

#include <sys/mman.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "hashtable.h"

// =============================================
// Definitions for hash stat
// =============================================
// FIXME: enforce initialization order
#ifdef CPI_HASH_STAT
long long __llvm__cpi_nrhit = 0;
long long __llvm__cpi_nrmiss1 = 0;
long long __llvm__cpi_nrmiss2 = 0;
long long __llvm__cpi_nrmiss3 = 0;
long long __llvm__cpi_nrmiss4 = 0;
long long __llvm__cpi_nrmiss5 = 0;
long long __llvm__cpi_nrmissx = 0;
long long __llvm__cpi_nrset0 = 0;
long long __llvm__cpi_nrset1 = 0;
long long __llvm__cpi_nrset2 = 0;
long long __llvm__cpi_nrset3 = 0;
long long __llvm__cpi_nrset4 = 0;
long long __llvm__cpi_nrset5 = 0;
long long __llvm__cpi_nrsetx = 0;
#endif // CPI_HASH_STAT

// =============================================
// Global variables definitions
// =============================================
#ifdef CPI_ST_STATIC
//__llvm__cpi_ptrval __llvm__cpi_shadowmemory[1 << ST_NRBITS]
//    __attribute__((aligned(1 << ST_NRBITS)));
#else
__llvm__cpi_ptrval *__llvm__cpi_shadowmemory;
#endif

#ifdef CPI_BOUNDS
__llvm__cpi_ptrval __llvm__cpi_null_entry = { NULL, NULL, __llvm__cpi_bounds_null };
#else
__llvm__cpi_ptrval __llvm__cpi_null_entry = { NULL, NULL };
#endif

int __llvm__cpi_inited = 0;

// =============================================
// Constructor / Destructor
// =============================================

/*** Interface function ***/
__attribute__((constructor(0)))
__CPI_EXPORT
void __llvm__cpi_init(void) {
    if (__llvm__cpi_inited)
        return;

    __llvm__cpi_inited = 1;

#ifdef CPI_ST_STATIC
  unsigned long long size = ST_SIZE;
  void *p = mmap(__llvm__cpi_shadowmemory, size, PROT_READ | PROT_WRITE,
                    CPI_MMAP_FLAGS, -1, 0);
    if (p == (void *)-1) {
        perror("Failed to mmap CPI shadow memory");
        exit(1);
    } else if (p != __llvm__cpi_shadowmemory) {
        fprintf(stderr, "Failed to mmap CPI shadow memory");
        exit(1);
    }
#else
    // we looked into 3 options to allocate the shadow table:
    // 1) static array, but llvm analysis tries to optimize accesses, runs out
    // of memory for most programs
    // 2) posix_memalign, registers with malloc memory mgmt but not necessarily
    // 0 initialized (valgrind memcheck complains)
    // 3) mmap, raw and ugly.

    unsigned long long size = ST_SIZE;
    // Randomization and 32 bit support :)
    // void *p = mmap((char*) (1ull<<46) - (1ull<<39), size,
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   CPI_MMAP_FLAGS,
                   -1, 0);
    if (p == (void *)-1) {
        perror("Failed to mmap CPI shadow memory");
        exit(1);
    }
    __llvm__cpi_shadowmemory = (__llvm__cpi_ptrval *)p;
#endif
    __llvm__cpi_null_entry.ptr = NULL;
    __llvm__cpi_null_entry.value = NULL;
#if defined(CPI_BOUNDS)
    __llvm__cpi_null_entry.bounds = __llvm__cpi_bounds_empty;
#endif
}

// =============================================

__attribute__((destructor(0)))
__CPI_EXPORT
void __llvm__cpi_destroy(void) {
#ifdef CPI_HASH_STAT
    __llvm__cpi_statistic();
#endif
#ifdef CPI_PROFILE_STATS
    __llvm__cpi_profile_statistic();
#endif
}

// =============================================
// Store functions
// =============================================

__CPI_HIDDEN __CPI_NOINLINE
#ifdef CPI_BOUNDS
void __llvm__cpi_set_slow(void **fptr, unsigned long offset, void *val,
                          __llvm__cpi_bounds bounds) {
#else
void __llvm__cpi_set_slow(void **fptr, unsigned long offset, void *val) {
#endif
    // fptr = (void **)ALIGNED(fptr);
    offset = ALIGNED_OFFSET(offset, fptr);

    // We know the fast path missed, hence we can skip the first location
    offset = (offset + sizeof(__llvm__cpi_ptrval)) & (ST_SIZE - 1);

    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
#if defined(CPI_HASH_STAT) || defined(CPI_MAX_LOOP)
    long count = 1;
    while (ptr->ptr != NULL) {
#else
    while (ptr->ptr != NULL) {
#endif // defined(CPI_HASH_STAT or CPI_MAX_LOOP)
        if (CPI_EXPECT(ptr->ptr == fptr)) {
            ptr->value = val;
#ifdef CPI_BOUNDS
            ptr->bounds = bounds;
#endif
#ifdef CPI_HASH_STAT
            switch (count) {
                case 1:
                    ++__llvm__cpi_nrset1;
                    break;
                case 2:
                    ++__llvm__cpi_nrset2;
                    break;
                case 3:
                    ++__llvm__cpi_nrset3;
                    break;
                case 4:
                    ++__llvm__cpi_nrset4;
                    break;
                case 5:
                default:
                    ++__llvm__cpi_nrset5;
                    break;
            }
#endif // defined(CPI_HASH_STAT)
            return;
        }
#if defined(CPI_MAX_LOOP)
        if (count == CPI_MAX_LOOP) {
            fprintf(stderr, "CPI mapping table full (set):\n"
                            "fptr=%p, val=%p at %p\n",
                    fptr, val, ptr);
            abort();
        }
#endif // defined(CPI_MAX_LOOP)
        // seek plus range check
        offset = (offset + sizeof(__llvm__cpi_ptrval)) & (ST_SIZE - 1);
        ptr = ST_GET_ADDR_FROM_OFFSET(offset);

#if defined(CPI_HASH_STAT) || defined(CPI_MAX_LOOP)
        ++count;
#endif
    }

#ifdef CPI_HASH_STAT
    ++__llvm__cpi_nrsetx;
    switch (count) {
        case 1:
            ++__llvm__cpi_nrset1;
            break;
        case 2:
            ++__llvm__cpi_nrset2;
            break;
        case 3:
            ++__llvm__cpi_nrset3;
            break;
        case 4:
            ++__llvm__cpi_nrset4;
            break;
        case 5:
        default:
            ++__llvm__cpi_nrset5;
            break;
    }
#endif // defined(CPI_HASH_STAT)
#ifdef CPI_ST_USE_CAS
    // Either we use test and set (NULL -> ptr)
    if (!__sync_bool_compare_and_swap(&(ptr->ptr), NULL, fptr))
#ifdef CPI_BOUNDS
        return __llvm__cpi_set_bounds(fptr, val, bounds);
#else
        return __llvm__cpi_set(fptr, val);
#endif
#else
    // Or we hope and pray; here we assume that no concurrent thread writes to
    // ptr
    // in the meantime
    ptr->ptr = fptr;
#endif // defined(CPI_ST_USE_CAS)
    ptr->value = val;
#ifdef CPI_BOUNDS
    ptr->bounds = bounds;
#endif
}

// =============================================
// Load functions
// =============================================

__CPI_HIDDEN __CPI_NOINLINE
__llvm__cpi_ptrval *__llvm__cpi_get_slow(void **fptr, unsigned long offset) {
    // fptr = (void **)ALIGNED(fptr);
    offset = ALIGNED_OFFSET(offset, fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
#if defined(CPI_HASH_STAT) || defined(CPI_MAX_LOOP)
    long count = 0;
    while (ptr->ptr != NULL) {
#else
    while (ptr->ptr != NULL) {
#endif // defined(CPI_HASH_STAT or CPI_MAX_LOOP)
        if (CPI_EXPECT(ptr->ptr == fptr)) {
#ifdef CPI_HASH_STAT
            switch (count) {
                case 0:
                    ++__llvm__cpi_nrmissx;
                    break;
                case 1:
                    ++__llvm__cpi_nrmiss1;
                    break;
                case 2:
                    ++__llvm__cpi_nrmiss2;
                    break;
                case 3:
                    ++__llvm__cpi_nrmiss3;
                    break;
                case 4:
                    ++__llvm__cpi_nrmiss4;
                    break;
                case 5:
                default:
                    ++__llvm__cpi_nrmiss5;
                    break;
            }
#endif // defined(CPI_HASH_STAT)
            return ptr;
        }
#if defined(CPI_MAX_LOOP)
        if (count == CPI_MAX_LOOP) {
            fprintf(stderr, "CPI mapping table full (get):\n"
                            "fptr=%p, at %p\n",
                    fptr, ptr);
            abort();
        }
#endif // defined(CPI_MAX_LOOP)
        // seek plus range check
        offset = (offset + sizeof(__llvm__cpi_ptrval)) & (ST_SIZE - 1);
        ptr = ST_GET_ADDR_FROM_OFFSET(offset);

#if defined(CPI_HASH_STAT) || defined(CPI_MAX_LOOP)
        ++count;
#endif
    }

#ifdef CPI_HASH_STAT
    switch (count) {
        case 0:
            ++__llvm__cpi_nrmissx;
            break;
        case 1:
            ++__llvm__cpi_nrmiss1;
            break;
        case 2:
            ++__llvm__cpi_nrmiss2;
            break;
        case 3:
            ++__llvm__cpi_nrmiss3;
            break;
        case 4:
            ++__llvm__cpi_nrmiss4;
            break;
        case 5:
        default:
            ++__llvm__cpi_nrmiss5;
            break;
    }
#endif // defined(CPI_HASH_STAT)

    return &__llvm__cpi_null_entry;
#if 0
#if defined(CPI_ST_USE_CAS)
    // Either we use test and set (NULL -> ptr)
    if (!__sync_bool_compare_and_swap(&(ptr->ptr), NULL, fptr))
        return __llvm__cpi_get(fptr);
#else
    // Or we hope and pray; here we assume that no concurrent thread writes to ptr
    // in the meantime
    ptr->ptr = fptr;
#endif // defined(CPI_ST_USE_CAS)
    return ptr;
#endif
}

// =============================================
// Deletion functions
// =============================================

__CPI_HIDDEN __CPI_NOINLINE
void __llvm__cpi_delete_slow(void **fptr, unsigned long offset) {
    fptr = (void **)ALIGNED(fptr);
    offset = ALIGNED_OFFSET(offset, fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    while (ptr->ptr != NULL) {
        if (CPI_EXPECT(ptr->ptr == fptr)) {
            ptr->value = NULL;
#ifdef CPI_BOUNDS
            ptr->bounds = __llvm__cpi_bounds_empty;
#endif // defined(CPI_BOUNDS)
            return;
         }
        // seek plus range check
        offset = (offset + sizeof(__llvm__cpi_ptrval)) & (ST_SIZE - 1);
        ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    }
}

// =============================================

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_delete_range(unsigned char *fptr, size_t size) {
    if (size <= 0)
        return;

    fptr = (unsigned char *)ALIGNED(fptr);
    unsigned char *fptr_end = fptr + size;
    unsigned long offset = ST_GET_OFFSET(fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    __llvm__cpi_ptrval *ptr_last = __llvm__cpi_get_boundary_addr_in_st(fptr_end);

    DEBUG("Delete range: %p to %p, (%ld)\n", fptr, fptr_end, size);
    // only one entry
    if (CPI_EXPECT(size <= sizeof(void *))) {
        while (ptr->ptr != NULL) {
            if (CPI_EXPECT(ptr->ptr == fptr)) {
                ptr->value = NULL;
#ifdef CPI_BOUNDS
                ptr->bounds = __llvm__cpi_bounds_empty;
#endif
                break;
            }
            ++ptr;
        }
        return;
    }

    // loop through range and delete pointers
    while (ptr != ptr_last) {
      if (CPI_EXPECT((unsigned char *)(ptr->ptr) >= fptr &&
                      (unsigned char *)(ptr->ptr) < fptr_end)) {
            ptr->value = NULL;
#ifdef CPI_BOUNDS
            ptr->bounds = __llvm__cpi_bounds_empty;
#endif
        }
        // advance src ptr
        offset = (offset + sizeof(__llvm__cpi_ptrval)) & (ST_SIZE - 1);
        ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    }

#if 0
    // no wrap around
    if (CPI_EXPECT(ptr < ptr_last)) {
        for (; ptr < ptr_last; ++ptr) {
            if (CPI_EXPECT(ptr->ptr >= fptr && ptr->ptr < fptr_end)) {
                ptr->value = NULL;
#ifdef CPI_BOUNDS
                ptr->upper = NULL;
                ptr->lower = NULL;
#endif
             }
        }
        return;
    }
    // wrap around in hash table
    __llvm__cpi_ptrval *hash_start = __llvm__cpi_shadowmemory;
    __llvm__cpi_ptrval *hash_end = __llvm__cpi_shadowmemory + ST_MAXENTRIES;
    for (; ptr < hash_end; ++ptr) {
        if (CPI_EXPECT(ptr->ptr >= fptr && ptr->ptr < fptr_end)) {
            ptr->value = NULL;
#ifdef CPI_BOUNDS
            ptr->upper = NULL;
            ptr->lower = NULL;
#endif
        }
    }
    for (ptr = hash_start; ptr < ptr_last; ++ptr) {
        if (CPI_EXPECT(ptr->ptr >= fptr && ptr->ptr < fptr_end)) {
            ptr->value = NULL;
#ifdef CPI_BOUNDS
            ptr->upper = NULL;
            ptr->lower = NULL;
#endif
        }
    }
#endif
}

// =============================================
// Data movement functions
// =============================================

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_move_range(unsigned char *fptr_dst, unsigned char *fptr,
                            size_t size) {
    // TODO: should we print an error message? (or just silently ignore it?)
    if (CPI_EXPECTNOT(size <= 0 || fptr_dst == fptr))
        return;

    if (!(fptr_dst < (fptr + size) && (fptr_dst + size) > fptr)) {
        // clean destination and copy valid pointers
        __llvm__cpi_copy_range(fptr_dst, fptr, size);
        return;
    }

    if (fptr_dst < fptr) {
        void **src = (void **)(ALIGNED(fptr));
        void **dst = (void **)(ALIGNED(fptr_dst));
        void **end = (void **)(ALIGNED((fptr + size)));
        while (src < end) {
            __llvm__cpi_ptrval *ptr_src = __llvm__cpi_get_or_null(src);
            if (CPI_EXPECT(ptr_src->ptr == NULL || ptr_src->value == NULL)) {
                __llvm__cpi_delete(dst);
            } else {
#ifndef CPI_BOUNDS
                __llvm__cpi_set(dst, ptr_src->value);
#else
                __llvm__cpi_set_bounds(dst, ptr_src->value, ptr_src->bounds);
#endif
            }
            ++src;
            ++dst;
        }
    } else {
        void **src = (void **)(ALIGNED(fptr + size));
        void **dst = (void **)(ALIGNED(fptr_dst + size));
        void **end = (void **)(ALIGNED((fptr)));
        while (src > end) {
            __llvm__cpi_ptrval *ptr_src = __llvm__cpi_get_or_null(src);
            if (CPI_EXPECT(ptr_src->ptr == NULL || ptr_src->value == NULL)) {
                __llvm__cpi_delete(dst);
            } else {
#ifndef CPI_BOUNDS
                __llvm__cpi_set(dst, ptr_src->value);
#else
                __llvm__cpi_set_bounds(dst, ptr_src->value, ptr_src->bounds);
#endif
            }
            --src;
            --dst;
        }
    }

#if 0
    // clean destination and copy valid pointers
    __llvm__cpi_copy_range(fptr_dst, fptr, size);

    // clean source
    // take care of overlapping regions, dst can start before src or inside src
    if ((fptr_dst < (fptr+size) && (fptr_dst+size) > fptr)) {
        if (fptr < fptr_dst) {
            // dst starts before src ends
            long diff_after = (long)fptr_dst - (long)fptr + size;
            __llvm__cpi_delete_range(fptr, size - diff_after);
        } else {
            // dst overlaps into src
            long diff_before = (long)fptr - (long)fptr_dst + size;
            __llvm__cpi_delete_range(fptr+diff_before, size-diff_before);
        }
    } else {
        // no overlap
        __llvm__cpi_delete_range(fptr, size);
    }
#endif
}

// =============================================

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_copy_range(unsigned char *fptr_dst, unsigned char *fptr,
                            size_t size) {
    DEBUG("Copy range %p, %p -> %p %p, %ld\n", fptr, fptr + size, fptr_dst,
          fptr_dst + size, size);
    // TODO: should we print an error message? (or just silently ignore it?)
    if (CPI_EXPECTNOT(size <= 0 || fptr_dst == fptr))
        return;

    fptr = (unsigned char *)ALIGNED(fptr);
    fptr_dst = (unsigned char *)ALIGNED(fptr_dst);

    unsigned char *fptr_end = (unsigned char *)ALIGNED(fptr + size);
    unsigned long offset = ST_GET_OFFSET(fptr);
    __llvm__cpi_ptrval *ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    __llvm__cpi_ptrval *ptr_last = __llvm__cpi_get_boundary_addr_in_st(fptr_end);

    // clean dst area first
    __llvm__cpi_delete_range(fptr_dst, size);

    // copy valid pointers, adapting target ptr.
    // three cases: single entry/wrap/nowrap

    // 1) only one entry
    if (CPI_EXPECT(size <= sizeof(void *))) {
        while (ptr->ptr != NULL) {
            if (CPI_EXPECTNOT(ptr->ptr == fptr)) {
#ifndef CPI_BOUNDS
                __llvm__cpi_set((void **)fptr_dst, ptr->value);
#else
                __llvm__cpi_set_bounds((void **)fptr_dst, ptr->value,
                                       ptr->bounds);
#endif
                break;
            }
            ++ptr;
        }
        return;
    }

    // 2) copy range
    //    wrap for src handled explicitly through the offset adjusment
    //    wrap for dst handled implicitly through set function
    while (ptr != ptr_last) {
        void **tmp_dst =
            (void **)((unsigned long)fptr_dst + (unsigned long)(ptr->ptr) -
                      (unsigned long)fptr);
        if (CPI_EXPECTNOT(ptr->ptr >= (void *)fptr &&
                           ptr->ptr < (void *)fptr_end)) {
#ifndef CPI_BOUNDS
            __llvm__cpi_set(tmp_dst, ptr->value);
#else
            __llvm__cpi_set_bounds(tmp_dst, ptr->value, ptr->bounds);
#endif
        }
        // advance src ptr
        offset = (offset + sizeof(__llvm__cpi_ptrval)) & (ST_SIZE - 1);
        ptr = ST_GET_ADDR_FROM_OFFSET(offset);
    }
#if 0
    // 2) no wrap in source
    if (CPI_EXPECT(ptr < ptr_last)) {
        for (; ptr < ptr_last; ++ptr) {
            if (CPI_EXPECT(ptr->ptr >= fptr && ptr->ptr < fptr_end)) {
                void **tmp_dst = (void**)((unsigned long)fptr_dst + (unsigned long)(ptr->ptr)-(unsigned long)fptr);
#ifndef CPI_BOUNDS
                __llvm__cpi_set(tmp_dst, ptr->value);
#else
                __llvm__cpi_set_bounds(tmp_dst, ptr->value, ptr->upper, ptr->lower);
#endif
            }
        }
        return;

    }
    // 3) wrap around in hash table
    __llvm__cpi_ptrval *hash_start = __llvm__cpi_shadowmemory;
    __llvm__cpi_ptrval *hash_end = __llvm__cpi_shadowmemory + ST_MAXENTRIES;
    for (; ptr < hash_end; ++ptr) {
        if (CPI_EXPECT(ptr->ptr >= fptr && ptr->ptr < fptr_end)) {
            void **tmp_dst = (void**)((unsigned long)fptr_dst + (unsigned long)(ptr->ptr)-(unsigned long)fptr);
#ifndef CPI_BOUNDS
            __llvm__cpi_set(tmp_dst, ptr->value);
#else
            __llvm__cpi_set_bounds((void**)tmp_dst, ptr->value, ptr->upper, ptr->lower);
#endif
        }
    }
    for (ptr = hash_start; ptr < ptr_last; ++ptr) {
        if (CPI_EXPECT(ptr->ptr >= fptr && ptr->ptr < fptr_end)) {
            void **tmp_dst = (void**)((unsigned long)fptr_dst + (unsigned long)(ptr->ptr)-(unsigned long)fptr);
#ifndef CPI_BOUNDS
            __llvm__cpi_set(tmp_dst, ptr->value);
#else
            __llvm__cpi_set_bounds((void**)tmp_dst, ptr->value, ptr->upper, ptr->lower);
#endif
        }
    }
#endif
}

// =============================================
// Debug functions
// =============================================

/*** Interface function ***/
__CPI_EXPORT
void __llvm__cpi_dump(void **fptr) {
  __llvm__cpi_ptrval *ptr = __llvm__cpi_get(fptr);

  //  printf("fptr: %p\nfval: %p\nhloc: %p\n", fptr, ptr->value, ptr);
  fprintf(stderr, "Pointer  address: %p\n", fptr);
  fprintf(stderr, "Pointer  value  : %p\n", *fptr);
  fprintf(stderr, "Metadata address: %p\n", ptr);
  fprintf(stderr, "Metadata value  : %p\n", ptr->value);
#ifdef CPI_BOUNDS
  //  printf("bounds: [0x%lx, 0x%lx]\n", ptr->bounds[0], ptr->bounds[1]);
  fprintf(stderr, "Lower bound:    : 0x%lx\n", ptr->bounds[0]);
  fprintf(stderr, "Upper bound:    : 0x%lx\n", ptr->bounds[1]);
#endif

}

// =============================================
// Hash stat
// =============================================

#ifdef CPI_HASH_STAT
void __llvm__cpi_statistic() {
    char fname[512];
    snprintf(fname, sizeof(fname), CPI_FPTR_LOG "-%s",
#ifdef __gnu_linux__
             program_invocation_short_name
#else
             getprogname()
#endif
             );
    FILE *fd = fopen(fname, "a");
    fprintf(fd, "========================\n");
    struct timeval tp;
    gettimeofday(&tp, NULL);
    fprintf(fd, "%s\n", ctime(&tp.tv_sec));

    long long tot = __llvm__cpi_nrhit + __llvm__cpi_nrmissx +
                    __llvm__cpi_nrmiss1 + __llvm__cpi_nrmiss2 +
                    __llvm__cpi_nrmiss3 + __llvm__cpi_nrmiss4 +
                    __llvm__cpi_nrmiss5;
    long long stot = __llvm__cpi_nrset0 + __llvm__cpi_nrset1 +
                     __llvm__cpi_nrset2 + __llvm__cpi_nrset3 +
                     __llvm__cpi_nrset4 + __llvm__cpi_nrset5;
    fprintf(fd, "cpi statistics: get\n");
    fprintf(fd, "direct: %lld (%lld%%)\n", __llvm__cpi_nrhit,
            (long long)(__llvm__cpi_nrhit / ((double)tot) * 100));
    fprintf(fd, "1 miss: %lld (%lld%%)\n", __llvm__cpi_nrmiss1,
            (long long)(__llvm__cpi_nrmiss1 / ((double)tot) * 100));
    fprintf(fd, "2 miss: %lld (%lld%%)\n", __llvm__cpi_nrmiss2,
            (long long)(__llvm__cpi_nrmiss2 / ((double)tot) * 100));
    fprintf(fd, "3 miss: %lld (%lld%%)\n", __llvm__cpi_nrmiss3,
            (long long)(__llvm__cpi_nrmiss3 / ((double)tot) * 100));
    fprintf(fd, "4 miss: %lld (%lld%%)\n", __llvm__cpi_nrmiss4,
            (long long)(__llvm__cpi_nrmiss4 / ((double)tot) * 100));
    fprintf(fd, "5 miss: %lld (%lld%%)\n", __llvm__cpi_nrmiss5,
            (long long)(__llvm__cpi_nrmiss5 / ((double)tot) * 100));
    fprintf(fd, "new:    %lld (%lld%%)\n", __llvm__cpi_nrmissx,
            (long long)(__llvm__cpi_nrmissx / ((double)tot) * 100));
    fprintf(fd, "\ncpi statistics: set\n");
    fprintf(fd, "direct: %lld (%lld%%)\n", __llvm__cpi_nrset0,
            (long long)(__llvm__cpi_nrset0 / ((double)stot) * 100));
    fprintf(fd, "1 miss: %lld (%lld%%)\n", __llvm__cpi_nrset1,
            (long long)(__llvm__cpi_nrset1 / ((double)stot) * 100));
    fprintf(fd, "2 miss: %lld (%lld%%)\n", __llvm__cpi_nrset2,
            (long long)(__llvm__cpi_nrset2 / ((double)stot) * 100));
    fprintf(fd, "3 miss: %lld (%lld%%)\n", __llvm__cpi_nrset3,
            (long long)(__llvm__cpi_nrset3 / ((double)stot) * 100));
    fprintf(fd, "4 miss: %lld (%lld%%)\n", __llvm__cpi_nrset4,
            (long long)(__llvm__cpi_nrset4 / ((double)stot) * 100));
    fprintf(fd, "5 miss: %lld (%lld%%)\n", __llvm__cpi_nrset5,
            (long long)(__llvm__cpi_nrset5 / ((double)stot) * 100));
    fprintf(fd, "new:    %lld (%lld%%)\n", __llvm__cpi_nrsetx,
            (long long)(__llvm__cpi_nrsetx / ((double)stot) * 100));
    fprintf(fd, "\n\n");
    fclose(fd);
}
#endif

// =============================================
