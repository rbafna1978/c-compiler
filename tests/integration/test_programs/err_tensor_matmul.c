/* error: matmul: inner dimensions differ */
int main() {
  tensor<float, 2, 3> a = [[1, 2, 3], [4, 5, 6]];
  tensor<float, 2, 2> c = matmul(a, a);
  return 0;
}
