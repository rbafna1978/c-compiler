/* exit: 0 */
tensor<float, 1, 4> relu(tensor<float, 1, 4> h) {
  for (int j = 0; j < 4; j += 1) { if (h[0][j] < 0) { h[0][j] = 0; } }
  return h;
}
int main() {
  tensor<float, 1, 3> x = [[1, -2, 0.5]];
  tensor<float, 3, 4> w1 = [[0.5, -1, 0.25, 2], [1, 0.5, -0.5, 0], [-2, 1, 1, 0.5]];
  tensor<float, 1, 4> b1 = [[0.25, 0.25, -1, 0]];
  tensor<float, 4, 2> w2 = [[1, -1], [0.5, 2], [-0.25, 1], [1, 0.5]];
  tensor<float, 1, 2> out = matmul(relu(matmul(x, w1) + b1), w2);
  print(out);
  return 0;
}
