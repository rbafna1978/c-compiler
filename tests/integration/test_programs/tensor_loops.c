/* exit: 0 */
int main() {
  tensor<float, 3, 4> a;
  tensor<float, 4, 2> b;
  for (int i = 0; i < 3; i += 1) { for (int j = 0; j < 4; j += 1) { a[i][j] = i * 4 + j; } }
  for (int i = 0; i < 4; i += 1) { for (int j = 0; j < 2; j += 1) { b[i][j] = (i - j) * 0.5; } }
  tensor<float, 3, 2> c = matmul(a, b);
  print(c);
  print(sum(c));
  tensor<float, 2, 3> t = transpose(c);
  print(t);
  float total = 0;
  for (int i = 0; i < 2; i += 1) { total += t[i][2]; }
  print(total);
  return 0;
}
