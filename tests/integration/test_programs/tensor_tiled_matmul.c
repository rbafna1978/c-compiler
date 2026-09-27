/* exit: 0 */
int main() {
  tensor<float, 64, 64> a;
  tensor<float, 64, 64> b;
  for (int i = 0; i < 64; i += 1) {
    for (int j = 0; j < 64; j += 1) {
      a[i][j] = (i * 64 + j) * 0.001 - 2;
      b[i][j] = (i - j) * 0.01;
    }
  }
  tensor<float, 64, 64> c = matmul(a, b);
  print(sum(c));
  print(c[0][0]);
  print(c[63][63]);
  print(c[10][20]);
  return 0;
}
