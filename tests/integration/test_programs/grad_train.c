/* exit: 0 */
float loss(tensor<float, 2, 1> W, float b, tensor<float, 4, 2> x, tensor<float, 4, 1> y) {
  tensor<float, 4, 1> err = matmul(x, W) + b - y;
  return sum(err * err);
}
int main() {
  tensor<float, 4, 2> x = [[1, 2], [2, 1], [3, 0.5], [0, 1]];
  tensor<float, 4, 1> y = [[0.5], [3.5], [6], [-0.5]];
  tensor<float, 2, 1> W;
  float b = 0;
  for (int step = 0; step <= 200; step += 1) {
    if (step % 50 == 0) { print(loss(W, b, x, y)); }
    tensor<float, 2, 1> dW = grad(loss, 0, W, b, x, y);
    float db = grad(loss, 1, W, b, x, y);
    W -= dW * 0.02;
    b -= db * 0.02;
  }
  print(W);
  print(b);
  return 0;
}
