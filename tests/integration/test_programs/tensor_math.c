/* exit: 0 */
int main() {
  print(exp(1.0));
  print(log(10));
  print(tanh(0.5));
  tensor<float, 2, 2> a = [[0.5, 1], [2, -1]];
  print(exp(a));
  print(tanh(a * 2));
  tensor<int, 3> k = [1, 2, 3];
  print(log(k));
  print(sum(exp(a)));
  return 0;
}
