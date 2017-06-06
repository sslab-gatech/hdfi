#include <stdio.h>

int simple(int a, int b) {
    return a + b;
}

int main() {
    int a = 1, b = 2;

    printf("simple = %d\n", simple(a, b));
}
