/* exit: 25 */
int counter = 10;
int next() { counter += 1; return counter; }
int main() {
  int x = 1;
  { int x = 5; x += next(); }   /* inner x shadows; counter = 11 */
  { int x = 100; counter += x - 100; }
  int r = next();               /* 12 */
  return x + counter + r;
}
