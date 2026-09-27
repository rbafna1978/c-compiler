/* A long elementwise chain over a large tensor: the benchmark for Phase 10's fusion. Unfused, each
   operator is its own full sweep over memory (temp buffer in, temp buffer out); fused, the whole
   chain is one sweep with no temporaries. At 2M elements (8 MB/buffer) this is far bigger than any
   cache, so the difference is memory bandwidth, not arithmetic -- run_benchmarks.py times this
   compiled with and without --no-fuse to measure it. */
tensor<float, 2048, 1024> x;

int main() {
  for (int i = 0; i < 2048; i += 1) {
    for (int j = 0; j < 1024; j += 1) { x[i][j] = (i * 1024 + j) % 1013 * 0.001; }
  }
  tensor<float, 2048, 1024> y = x * 2 - x / 3 + 1 - tanh(x) * 0.5 + exp(x * 0.0001) - x * x * 0.01;
  print(sum(y));
  print(y[0][0]);
  print(y[2047][1023]);
  return 0;
}
