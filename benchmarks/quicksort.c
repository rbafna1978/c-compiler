/* Quicksort over a tensor<int, N>: the language has no native C array, so this also demonstrates
   that tensor indexing/assignment (a[i] = x) is general enough for classic in-place algorithms,
   not just elementwise math. Recursive, branch- and memory-access-heavy -- a different performance
   profile from matmul/elementwise, so it's a useful second data point on general codegen quality. */
tensor<int, 2048> a;

void swap(int i, int j) {
  int t = a[i];
  a[i] = a[j];
  a[j] = t;
}

void quicksort(int lo, int hi) {
  if (lo >= hi) { return; }
  int pivot = a[(lo + hi) / 2];
  int i = lo;
  int j = hi;
  while (i <= j) {
    while (a[i] < pivot) { i += 1; }
    while (a[j] > pivot) { j -= 1; }
    if (i <= j) {
      swap(i, j);
      i += 1;
      j -= 1;
    }
  }
  quicksort(lo, j);
  quicksort(i, hi);
}

int main() {
  for (int i = 0; i < 2048; i += 1) { a[i] = (i * 48271) % 2053 - 1024; }
  quicksort(0, 2047);
  print(a[0]);
  print(a[1023]);
  print(a[2047]);
  print(sum(a));
  return 0;
}
