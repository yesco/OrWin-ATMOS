// test-struct-err.c - writes to consts, and struct links, that used to
// compile silently (wrong code, no error)
//
// Each line marked ERR (in a // comment) must be reported, no other, and
// no .ivm written.
// - a const slot written: a #define, a const int, the name of an array or
//   a static struct (its address). "a = b" made a an alias of b. Checked
//   on each statement's code (by the slot: also what the grammar emits)
// - a plain int declared after a struct one: it was linked to that
//   struct too, "i.x" compiled (and crashed)

#define N 3
const int two = 2;

struct pt { int x; int y; };

int main(void) {
  struct pt a;
  struct pt b;
  struct pt *ps[2];
  int i;
  int w[4];
  int v[4];

  b.x = 7;
  a = b;                            // ERR a static struct's address
  w = v;                            // ERR an array's address
  N = 4;                            // ERR a #define
  two = 3;                          // ERR a const int
  two++;                            // ERR the same, ++
  for (N = 0; N < 9; N++) i = 1;    // ERR a loop on a #define
  i.x = 1;                          // ERR i is no struct
  ps[1] = ps[0];
  a.x = b.x + N + two;
  return a.x;
}
