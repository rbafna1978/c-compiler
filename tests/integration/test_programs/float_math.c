/* exit: 15 */
float half(float x) { return x / 2; }
int main() {
  float a = 7; a = half(a) * 4 + 0.5;   /* 14.5 */
  int n = a;                            /* truncates to 14 */
  float b = -a;
  if (b < -14 && a >= 14.5) { return n + 1; }
  return 0;
}
