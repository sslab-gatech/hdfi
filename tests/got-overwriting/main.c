#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#define SIZE 0x100

void evil() {
  printf("EVIL\n");
}

int main() {
  char buf[SIZE];
  while(true) {
    ssize_t size = read(0, buf, sizeof(buf));
    if (size < 0)
      break;
    if (!memcmp(buf, "exit", 4))
      break;
    printf(buf);
  }

  exit(0);
}
