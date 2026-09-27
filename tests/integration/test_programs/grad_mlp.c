/* exit: 0 */
float loss(tensor<float, 3, 4> W1, tensor<float, 4, 1> W2, tensor<float, 2, 3> x, tensor<float, 2, 1> y) {
  tensor<float, 2, 4> h = tanh(matmul(x, W1));
  tensor<float, 2, 1> d = matmul(h, W2) - y;
  return sum(d * d);
}
int main() {
  tensor<float, 3, 4> W1 = [[0.5, -0.25, 0.75, 0.1], [-0.5, 0.25, 0.3, -0.6], [0.2, 0.4, -0.7, 0.35]];
  tensor<float, 4, 1> W2 = [[0.5], [-0.75], [0.25], [0.6]];
  tensor<float, 2, 3> x = [[1, -1, 0.5], [0.5, 2, -1]];
  tensor<float, 2, 1> y = [[1], [-1]];
  print(grad(loss, 0, W1, W2, x, y));
  print(grad(loss, 1, W1, W2, x, y));
  for (int step = 0; step <= 100; step += 1) {
    if (step % 25 == 0) { print(loss(W1, W2, x, y)); }
    tensor<float, 3, 4> d1 = grad(loss, 0, W1, W2, x, y);
    tensor<float, 4, 1> d2 = grad(loss, 1, W1, W2, x, y);
    W1 -= d1 * 0.05;
    W2 -= d2 * 0.05;
  }
  print(W2);
  return 0;
}
