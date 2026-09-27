/* exit: 0 */
float f(float x, float y) { return x * y + exp(x) / y; }
float g(float a, float b) { float t = a / b; return -t * t + tanh(a) - log(b); }
int main() {
  print(grad(f, 0, 1.5, 2));
  print(grad(f, 1, 1.5, 2));
  print(grad(g, 0, 0.75, 2));
  print(grad(g, 1, 0.75, 2));
  return 0;
}
