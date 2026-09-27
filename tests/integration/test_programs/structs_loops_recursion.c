/* exit: 3 */
struct P { int x; int y; };
int g = 2 + 3;
int fact(int n) { if (n <= 1) { return 1; } return n * fact(n - 1); }
int fib(int n) { if (n < 2) { return n; } return fib(n - 1) + fib(n - 2); }
int sum(struct P p) { return p.x + p.y; }
int main() {
  struct P p; p.x = fact(5); p.y = g;          /* 120 + 5 */
  int s = 0;
  for (int i = 0; i < 10; i += 1) { if (i % 2 == 0 && i > 2) { s += i; } }  /* 4+6+8 = 18 */
  float f = 2.5; f = f * 2;                     /* 5.0 */
  int t = f;                                    /* 5 */
  return sum(p) + s + t + fib(10) - 200;        /* 125+19+5+55-200 = 3 */
}
