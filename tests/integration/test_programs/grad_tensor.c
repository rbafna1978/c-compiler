/* exit: 0 */
float loss(tensor<float, 2, 3> A, tensor<float, 3, 2> B, float s) {
  tensor<float, 2, 2> C = matmul(A, B) * s;
  tensor<float, 2, 2> E = exp(C * 0.25) / (C * C + 1);
  tensor<float, 3, 2> Bt = transpose(A);
  return sum(E) + sum(tanh(Bt * B)) - sum(log(C * C + 2));
}
int main() {
  tensor<float, 2, 3> A = [[0.5, -1, 0.25], [1, 0.5, -0.5]];
  tensor<float, 3, 2> B = [[1, 0.5], [-0.5, 1], [0.25, -1]];
  print(grad(loss, 0, A, B, 0.5));
  print(grad(loss, 1, A, B, 0.5));
  print(grad(loss, 2, A, B, 0.5));
  return 0;
}
