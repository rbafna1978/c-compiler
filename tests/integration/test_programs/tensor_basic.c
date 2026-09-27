/* exit: 0 */
int main() {
  tensor<float, 2, 3> a = [[1, 2, 3], [4, 5, 6]];
  tensor<float, 3, 2> b = transpose(a);
  tensor<float, 2, 2> c = matmul(a, b);
  print(c);
  print(b);
  print(sum(c));
  tensor<float, 2, 2> d = c * 2 - c / 4 + [[1, 1], [1, 1]];
  print(d);
  d += c;
  a[0][1] = 9;
  print(a[0]);
  print(-a);
  print(42);
  print("hello");
  return 0;
}
