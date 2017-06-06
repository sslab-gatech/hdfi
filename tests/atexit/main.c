#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#define SIZE 0x100

void good() {
  printf("Hello World\n");
}

int main() {
  atexit(good);
}
