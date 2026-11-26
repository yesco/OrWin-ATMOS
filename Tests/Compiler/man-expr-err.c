// man-expr-err.c - MeteoriC-man.md 5: expressions that do not compile
// (a rewrite for each is in the manual and in man-expr.c)
//
// Each line marked ERR (in a // comment) must be reported, no other, and
// no .ivm written.

int f(int a) {
  return a;
}

int main(void) {
  int a, b, c, x;

  a = 1; b = 2; c = 3;
  x = a + b * c;             // ERR a tighter operator after a looser one
  x = 35 + 7 * 4;            // ERR the same: 35 + (7 * 4)
  x = a < b + 1;             // ERR + after <
  x = a & b == c;            // ERR == after &
  x = a | b ^ c;             // ERR ^ after |
  x = a << b + 1;            // ERR + after <<
  x = -a;                    // ERR no unary minus: 0 - a
  x = ~a;                    // ERR no ~: a ^ 0xffff
  x = 09;                    // ERR 0... is octal: no 8 or 9
  x = a * b + c;
  x = f(a) + (b * c);
  return x;
}
