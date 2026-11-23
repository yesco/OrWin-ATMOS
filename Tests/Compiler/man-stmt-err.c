// man-stmt-err.c - MeteoriC-man.md 6, 7: statements and functions that do
// not compile
//
// Each line marked ERR (in a // comment) must be reported, no other, and
// no .ivm written.

int f(int a);                                   // ERR no prototypes

int main(void) {
  int i, a;

  a = 0;
  do { a++; } while (a < 3);                    // ERR no do
  for (i = 0; i < 9; i++) { if (i == 3) break; }  // ERR no break
  for (i = 0; i < 9; i++) { if (i == 3) continue; }  // ERR no continue
  switch (a) { case 1: a = 2; }                 // ERR no switch
  goto end;                                     // ERR no goto
  for (;;) { a++; }                             // ERR no for (;;)
  for (int j = 0; j < 3; j++) a++;              // ERR no declaration in for
  for (i = 0; i < 9; i += 2) a++;               // ERR a const step: put it in a var
  for (i = 0; i < 9; i++) a++;
  return a;
}
