#include <util.h>
int main() {
  char src[0x100];
  char dst[0x100];

  memcpy(dst, src, sizeof(src));
  check0(dst, sizeof(dst));
  printf("PASSED : memcpy -> check0\n");

  memcpy0(dst, src, sizeof(src));
  check0(dst, sizeof(dst));
  printf("PASSED : memcpy0 -> check0\n");

  memcpy1(dst, src, sizeof(src));
  check1(dst, sizeof(dst));
  printf("PASSED : memcpy1 -> check1\n");

  // This should be failed...
  memcpy0(dst, src, sizeof(src));
  check1(dst, sizeof(dst));
  printf("PASSED : memcpy0 -> check1\n");

  // This should be failed, too
  memcpy1(dst, src, sizeof(src));
  check0(dst, sizeof(dst));
  printf("PASSED : memcpy1 -> check0\n");
}
