/* error: shape mismatch in '+': tensor<float,2,3> vs tensor<float,3,2> */
int main() {
  tensor<float, 2, 3> a = [[1, 2, 3], [4, 5, 6]];
  tensor<float, 3, 2> b = transpose(a);
  tensor<float, 2, 3> c = a + b;
  return 0;
}
