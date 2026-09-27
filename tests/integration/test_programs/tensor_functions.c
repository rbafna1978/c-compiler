/* exit: 0 */
tensor<int, 2, 2> scale(tensor<int, 2, 2> m, int k) { m *= k; return m; }
tensor<float, 3> axpy(float a, tensor<float, 3> x, tensor<float, 3> y) { return x * a + y; }
float dot(tensor<float, 3> x, tensor<float, 3> y) { return sum(x * y); }
tensor<float, 2, 2> g;
int main() {
  tensor<int, 2, 2> m = [[1, 2], [3, 4]];
  tensor<int, 2, 2> s = scale(m, 3);
  print(m);
  print(s);
  tensor<float, 3> x = [1, 2, 3];
  tensor<float, 3> y = [0.5, 0.25, 0.125];
  print(axpy(2, x, y));
  print(dot(x, y));
  tensor<float, 2, 3> a = [[1, 2, 3], [4, 5, 6]];
  a[1] = x * 2;
  print(a);
  g[1][1] = 7;
  g += 1;
  print(g);
  tensor<float, 2, 2> h = g;
  h[0][0] = 100;
  print(g[0][0]);
  print(h[0][0]);
  return 0;
}
