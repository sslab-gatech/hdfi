#include <stdio.h>

void start() {
  char buf[1000];
  printf("Give me something...\n");
  read(fileno(stdin), buf, 0x1000);
}

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stdin, NULL, _IONBF, 0);

  start();
  return 0;
}

void sigret(void) {
  asm("movq $15,%rax\n");
  asm("syscall\n");
  asm("ret\n");
}

