/* 512x512 float matmul: the benchmark for Phase 10's cache-blocked (tiled) matmul. 512 is a
   multiple of the compiler's 32x32 tile size, so this exercises the tiled path; run_benchmarks.py
   times this compiled with and without --no-tile to measure the speedup directly. */
int main() {
  tensor<float, 512, 512> a;
  tensor<float, 512, 512> b;
  for (int i = 0; i < 512; i += 1) {
    for (int j = 0; j < 512; j += 1) {
      a[i][j] = (i * 512 + j) % 97 * 0.01 - 0.5;
      b[i][j] = (i - j) % 89 * 0.01;
    }
  }
  tensor<float, 512, 512> c = matmul(a, b);
  print(sum(c));
  print(c[0][0]);
  print(c[511][511]);
  return 0;
}
