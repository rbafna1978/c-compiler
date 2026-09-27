/* exit: 17 */
int gcd(int a, int b) { while (b != 0) { int t = a % b; a = b; b = t; } return a; }
int collatz(int n) { int steps = 0; while (n != 1) { if (n % 2 == 0) { n = n / 2; } else { n = 3 * n + 1; } steps += 1; } return steps; }
int main() { return gcd(48, 18) + collatz(27) - 100; }
