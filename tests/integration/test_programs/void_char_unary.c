/* exit: 10 */
int g = 0;
void bump(int by) { g += by; return; g = 999; }
char next(char c) { return c + 1; }
int main() {
  bump(3); bump(4);                       /* g = 7 */
  char c = 'a'; c = next(c);              /* 'b' = 98 */
  float f = 10; f /= 4;                   /* 2.5 */
  int w = 0; while (!(w >= 5)) { w += 1; }
  int neg = -w;                           /* -5 */
  int lg = (0 || 1) + (1 && 0) + (f > 2.0);   /* 1+0+1 = 2 */
  return g + (c - 97) + w + neg + lg;     /* 7+1+5-5+2 = 10 */
}
