/* Naive recursive Fibonacci: pure scalar recursion/branching, no tensors -- a baseline check that
   the compiler's ordinary (non-ML) code generation is competitive with clang, independent of the
   tensor-specific optimizations (fusion, tiling) this project's other benchmarks measure. */
int fib(int n) {
  if (n < 2) { return n; }
  return fib(n - 1) + fib(n - 2);
}
int main() {
  print(fib(35));
  return 0;
}
