/* Plain-C twin of ../quicksort.c, compiled with system clang -O2 as an external reference point. */
#include <stdio.h>
static int a[2048];
static void swap(int i, int j) { int t = a[i]; a[i] = a[j]; a[j] = t; }
static void quicksort(int lo, int hi) {
  if (lo >= hi) return;
  int pivot = a[(lo + hi) / 2], i = lo, j = hi;
  while (i <= j) {
    while (a[i] < pivot) i++;
    while (a[j] > pivot) j--;
    if (i <= j) { swap(i, j); i++; j--; }
  }
  quicksort(lo, j);
  quicksort(i, hi);
}
int main(void) {
  for (int i = 0; i < 2048; i++) a[i] = (i * 48271) % 2053 - 1024;
  quicksort(0, 2047);
  long long s = 0;
  for (int i = 0; i < 2048; i++) s += a[i];
  printf("%d\n%d\n%d\n%lld\n", a[0], a[1023], a[2047], s);
  return 0;
}
