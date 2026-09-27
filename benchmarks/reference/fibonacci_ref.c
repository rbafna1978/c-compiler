/* Plain-C twin of ../fibonacci.c, compiled with system clang -O2 as an external reference point. */
#include <stdio.h>
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
int main(void) { printf("%d\n", fib(35)); return 0; }
