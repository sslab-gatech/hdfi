#include <stdio.h>
#include <sys/mman.h>

// FIXME: the following should be removed
#ifdef __FreeBSD__
#define __linux__
#endif

#ifdef __linux__
#include <errno.h>
#endif

#include "interception/interception.h"

namespace __llvm__safestack {

//#define SAFESTACK_PROFILE_STATS

// Thread local variables

#ifdef __gnu_linux__
extern "C" char *program_invocation_short_name;
#endif

#define __SAFESTACK_DEFAULT_STACK_SIZE 0x2000000;

#if defined(__gnu_linux__) && defined(__x86_64__)
# define __SAFESTACK_USE_TCB

#ifdef __pic__
# define IMM_MODE "nr"
#else
# define IMM_MODE "ir"
#endif

# define __SAFESTACK_THREAD_GETMEM_L(offset) \
  ({ unsigned long  __value;            \
      asm volatile ("movq %%fs:%P1,%q0" \
                       : "=r" (__value) \
                       : "i" (offset)); \
     __value; })

# define __SAFESTACK_THREAD_SETMEM_L(offset, value)      \
  ({ asm volatile ("movq %q0,%%fs:%P1" :            \
                : IMM_MODE ((unsigned long) value), \
                  "i" (offset)); })                 \

# define __SAFESTACK_UNSAFE_STACK_PTR_OFFSET   0x280
# define __SAFESTACK_UNSAFE_STACK_START_OFFSET 0x288
# define __SAFESTACK_UNSAFE_STACK_SIZE_OFFSET  0x290

#else
__thread void  *unsafe_stack_start = 0;
__thread size_t unsafe_stack_size = 0;

extern "C" {
  __attribute__((visibility ("default")))
  __thread void  *__llvm__unsafe_stack_ptr = 0;
}
#endif

struct tinfo {
  void *(*start_routine)(void*);
  void *start_rouinte_arg;

  void *unsafe_stack_start;
  size_t unsafe_stack_size;
};

static inline void *unsafe_stack_alloc(size_t size) {
  return mmap(NULL, size, 
              PROT_WRITE  | PROT_READ, 
              MAP_PRIVATE | MAP_ANON, // | MAP_STACK | MAP_GROWSDOWN, 
              -1, 0);
}

static inline void unsafe_stack_setup(void *start, size_t size) {
  void* stack_ptr = (void*) (((char*) start) + size
                             - 2*sizeof(struct tinfo));
#ifdef __SAFESTACK_USE_TCB
  __SAFESTACK_THREAD_SETMEM_L(__SAFESTACK_UNSAFE_STACK_PTR_OFFSET, stack_ptr);
  __SAFESTACK_THREAD_SETMEM_L(__SAFESTACK_UNSAFE_STACK_START_OFFSET, start);
  __SAFESTACK_THREAD_SETMEM_L(__SAFESTACK_UNSAFE_STACK_SIZE_OFFSET, size);
#else
  __llvm__unsafe_stack_ptr = stack_ptr;
  unsafe_stack_start = start;
  unsafe_stack_size = size;
#endif
}

static void unsafe_stack_free() {
  // Call might come from main thread
#ifdef __SAFESTACK_USE_TCB
  void *addr = (void*) __SAFESTACK_THREAD_GETMEM_L(__SAFESTACK_UNSAFE_STACK_START_OFFSET);
  size_t size = __SAFESTACK_THREAD_GETMEM_L(__SAFESTACK_UNSAFE_STACK_SIZE_OFFSET);
  __SAFESTACK_THREAD_SETMEM_L(__SAFESTACK_UNSAFE_STACK_START_OFFSET, 0);
#else
  void *addr = unsafe_stack_start;
  size_t size = unsafe_stack_size;
  unsafe_stack_size = 0;
#endif

  if (addr)
    munmap(addr, size);
}

static void* thread_start(void *arg) {
  struct tinfo *tinfo = (struct tinfo*) arg;
  unsafe_stack_setup(tinfo->unsafe_stack_start, tinfo->unsafe_stack_size);


  // Start the original thread rutine
  void *result = tinfo->start_routine(tinfo->start_rouinte_arg);

  // If thread exits with pthread_exit() this is not going to get called,
  // this is why we intercept pthread_exit() as well and call destroy.
  unsafe_stack_free();

  return result;
}

INTERCEPTOR(int, pthread_create, void *thread, void *attr,
      void *(*start_routine)(void*), void *arg) {

  // TODO: if stack size is set, use the same size for
  // unsafe stack as well.
  //
  // size_t safe_stack_size = 0;
  // if (attr) {
  //     pthread_attr_getstacksize (attr, &safe_stack_size);
  // }

  size_t size = __SAFESTACK_DEFAULT_STACK_SIZE;
  void *addr = unsafe_stack_alloc(size);
  struct tinfo *tinfo = (struct tinfo*) (
        ((char*)addr) + size - sizeof(struct tinfo));
  tinfo->start_routine = start_routine;
  tinfo->start_rouinte_arg = arg;
  tinfo->unsafe_stack_start = addr;
  tinfo->unsafe_stack_size = size;

  return REAL(pthread_create)(thread, attr, thread_start, tinfo);
}

INTERCEPTOR(void, pthread_exit, void *retval) {
  unsafe_stack_free();
  REAL(pthread_exit)(retval);
}

int inited = 0;

extern "C"
__attribute__((visibility ("default")))
__attribute__((constructor(0)))
void __llvm__safestack_init() {
  if (inited)
    return;

  inited = 1;

  // Allocate unsafe stack for main thread
  size_t size = __SAFESTACK_DEFAULT_STACK_SIZE;
  void *start = unsafe_stack_alloc(size);
  unsafe_stack_setup(start, size);

  // Initialize pthread interceptors for thread allocation
  INTERCEPT_FUNCTION(pthread_create);
  INTERCEPT_FUNCTION(pthread_exit);
}

#ifdef SAFESTACK_PROFILE_STATS

#include <stdlib.h>
#include <string.h>

#define SAFESTACK_PROFILE_LOG "/tmp/safestack_profile_stat"
struct ProfileStat {
    size_t num;
    char *name;
};

struct ProfileTable {
    ProfileStat *table;
    size_t size;
};

ProfileTable stats_dir[256];
size_t stats_dir_size = 0;
size_t stats_total_size = 0;

extern "C"
__attribute__((visibility ("default")))
void __llvm__safestack_register_profile_table(void *table, size_t size) {
    if (stats_dir_size >= 256) {
        fprintf(stderr,
                "SafeStack: profile table overflow (too many modules)\n");
        abort();
    }
    stats_dir[stats_dir_size].table = (ProfileStat*) table;
    stats_dir[stats_dir_size].size = size;
    stats_total_size += size;
    stats_dir_size += 1;
}

int profile_item_compar(const void *p1, const void *p2) {
    return ((ProfileStat*) p2)->num - ((ProfileStat*) p1)->num;
}

extern "C"
__attribute__((destructor(0)))
__attribute__((visibility("default")))
void __llvm__safestack_profile_statistic() {
    if (stats_total_size == 0)
        return;

    ProfileStat *table = (ProfileStat*) malloc(stats_total_size*
                                                       sizeof(ProfileStat));
    ProfileStat *table_ptr = table;
    for (size_t i = 0; i < stats_dir_size; ++i) {
        memcpy(table_ptr, stats_dir[i].table,
               stats_dir[i].size * sizeof(ProfileStat));
        table_ptr += stats_dir[i].size;
    }

    qsort(table, stats_total_size, sizeof(ProfileStat),
          profile_item_compar);

    char fname[512];
    snprintf(fname, sizeof(fname), SAFESTACK_PROFILE_LOG "-%s",
#ifdef __gnu_linux__
             program_invocation_short_name
#else
             getprogname()
#endif
             );
    FILE *fd = fopen(fname, "w");
    for (size_t i = 0; i < stats_total_size; ++i) {
        fprintf(fd, "%zu\t%s\n", table[i].num, table[i].name);
    }
    fclose(fd);
}

#endif  // defined(SAFESTACK_PROFILE_STATS)

} // namespace __llvm__safestack
