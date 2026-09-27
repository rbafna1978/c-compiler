/* exit: 25 */
int is_prime(int n) {
  if (n < 2) { return 0; }
  for (int d = 2; d * d <= n; d += 1) { if (n % d == 0) { return 0; } }
  return 1;
}
int main() { int count = 0; for (int i = 0; i < 100; i += 1) { count += is_prime(i); } return count; }
