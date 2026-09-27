/* exit: 43 */
struct P { int x; int y; };
struct P make(int a, int b) { struct P p; p.x = a; p.y = b; return p; }
int scale(struct P p, int k) { p.x = p.x * k; return p.x + p.y; }
int main() { struct P q = make(3, 4); return make(5, 6).y + scale(q, 10) + q.x; }
