#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BIN_BASE (char*)0x2aaaaaa000

int main() {
  void** dest;
  void* got = BIN_BASE + 0x1a68;
  printf("GOT : %p\n", got);
  __asm__ __volatile__ (                                                                                                                                                                   "ldchk1 a5,(%0) \n"
          "sd     a5, %1\n"
      : : "r" (got), "m" (dest)
      );
  printf("In GOT : %p\n", dest);
  exit(0);
}
