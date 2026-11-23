// test-precedence-err.c - what must NOT parse, all in one run
//
// Each line marked ERR (in a // comment) must be reported as "file:N: error"; every
// other line must parse (te/run-err.sh compares the two lists). The
// parser reports a statement it cannot parse, skips it up to its ; and
// goes on (\! in rule C), so one run lists all of them; then no .ivm.
// Rule O (c.bnf) only lets an operator be followed by one of the same or
// a looser level, so a tighter one after a looser one is an error here
// instead of a wrong value (left to right would give (a + b) * c):
//   * / %    + -    << >>    < <= > >=    == !=    &    ^    |
// The fix in the source is parentheses: a + (b * c).
// Also: errors in nested blocks, an undefined name, junk; good lines in
// between still parse (the error does not run on).

int id(int x) {
  return x;
}

int main(void) {
  int a, b, c;

  a = 12; b = 5; c = 3;

  // tighter after looser: each level against the next tighter one
  a = a + b * c;                    // ERR + then *
  a = a - b / c;                    // ERR - then /
  a = a << b + c;                   // ERR << then +
  a = a < b << c;                   // ERR < then <<
  a = a == b < c;                   // ERR == then <
  a = a & b == c;                   // ERR & then ==
  a = a ^ b & c;                    // ERR ^ then &
  a = a | b ^ c;                    // ERR | then ^

  // two levels apart, and with R operands
  a = a | b * c;                    // ERR | then *
  a = a + id(b) * c;                // ERR + then * (R left)
  a = id(a) == b + c;               // ERR == then +

  // fine: the same, parenthesized or looser after tighter
  a = a + (b * c);
  a = a * b + c;
  a = a | b & c | 1;                // ERR | then &

  // in nested blocks: the block still closes
  if (a) {
    a = a + b % c;                  // ERR + then %
    b = 1;
  }
  while (b < 3) {
    b = b + 1;
    c = c > b * 2;                  // ERR > then *
  }

  // in a condition: the whole statement
  if (a + b * c) b = 1;             // ERR + then * in if
  while (a | b & c) a = 0;          // ERR | then & in while

  // not operators / names
  a = a + ;                         // ERR operand missing
  b = a + b;                        // fine: between two errors
  d = 5;                            // ERR undefined name
  a = a @ b;                        // ERR not an operator

  return a;
}
