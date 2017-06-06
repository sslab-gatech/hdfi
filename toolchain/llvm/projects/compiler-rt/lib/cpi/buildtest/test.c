#include <string.h>
#include <stdio.h>

#include "../cpi.c"

int main() {

  void *p = (void *)0xDEADBEEF;
  __llvm__cpi_set((void **)&p, (void *)p);
  __llvm__cpi_dump((void **)&p);
  __llvm__cpi_assert((void **)&p, (void *)p, (char *)0x0);

  printf("------------------\n");

  void *pointers[10] = {
    (void *)1, (void *)2, (void *)3, (void *)4, (void *)5,
    (void *)6, (void *)7, (void *)8, (void *)9, (void *)10
  };

  for (int i = 0; i < 10; i++) {
    __llvm__cpi_set((void **)&pointers[i], (void *)pointers[i]);
    __llvm__cpi_dump((void **)&pointers[i]);
    __llvm__cpi_assert((void **)&pointers[i], (void *)pointers[i], (char *)0x0);
  }

  memmove((unsigned char *)&pointers[5], (unsigned char *)pointers,
          5 * sizeof(void *));
  __llvm__cpi_move_range((unsigned char *)&pointers[5],
                         (unsigned char *)pointers, 5 * sizeof(void *));

  for (int i = 0; i < 10; i++)
    __llvm__cpi_assert((void **)&pointers[i], (void *)pointers[i], (char *)0x0);

  printf("------------------\n");

  for (int i = 0; i < 10; i++)
    __llvm__cpi_dump((void **)&pointers[i]);

  return 0;
}
