#include <stdio.h>      /* puts */
#include <stdlib.h>     /* atexit */
#include <stdint.h>

void evil() {
  printf("EVIL\n");
}

int main ()
{
  // from __call_exitprocs assembly
  uint64_t ptr1 = ADDR;
  uint64_t ptr2 = *(uint64_t*)ptr1;
  uint64_t ptr3 = *(uint64_t*)(ptr2 + DIST);
  void** ptr4 = (void**)(ptr3 + 16);
  *ptr4 = evil;
  printf("%16llX\n", ptr4);
}
