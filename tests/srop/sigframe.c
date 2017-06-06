#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

volatile int VAL = 0;
void handler(int sig) {
  VAL=1;
}

int main(int argc, const char *argv[]) {
  int pid, status;
  signal(SIGCHLD, handler);

  pid = fork();
  if ( pid == 0 ) exit(0);

  wait(&status);
  printf("Returned from handler %d\n", VAL);
  return 0;
}