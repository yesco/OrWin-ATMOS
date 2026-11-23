// test-recover-err.c - after an error, parsing goes on at the next
// statement / declaration / function (\! in rules C and D, c.bnf)
//
// Each line marked ERR (in a // comment) must be reported, no other
// (te/run-err.sh), and no .ivm written. What is skipped after an error:
// - a statement: up to its ; or over a whole { } block (with its else),
//   never over the } of the block it is in
// - at the top: a declaration up to its ;, a function over its body
// Strings and comments are not looked into: a ; or } in them is not one.

int g;
int h = 1 + ;                       // ERR global initializer

int bad(unknown_t x) {              // ERR unknown type: whole function
  g = 1;
  g = g + 1 * 2;
}

int ok(int x) {
  return x + 1;
}

int main(void) {
  int a;

  a = ok(1) + ;                     // ERR operand missing
  a = 2;
  if (a + a * 2) {                  // ERR the if, its block and else
    a = 1;
    a = 2;
  } else {
    a = 3;
  }
  a = 3;
  printf("x;}y", a +* 2 + );        // ERR ; } in a string
  a = 4;
  { a = a + 1 * 2; }                // ERR in a nested block
  a = 5;
  /* a = 1 + ; } */ a = a + ;       // ERR after a comment with ; }
  a = 6;
  return a;
}
