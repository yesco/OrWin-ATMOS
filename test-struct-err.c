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
// - an array of structs is SoA (field-major): only a[i].f has a meaning.
//   a alone, a.f, a[i], &a[i] would silently be the first field's array

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
  struct pt s[3];
  struct pt *q;

  b.x = 7;
  a = b;                            // ERR a static struct's address
  w = v;                            // ERR an array's address
  N = 4;                            // ERR a #define
  two = 3;                          // ERR a const int
  two++;                            // ERR the same, ++
  for (N = 0; N < 9; N++) i = 1;    // ERR a loop on a #define
  i.x = 1;                          // ERR i is no struct
  ps[1] = ps[0];
  q = s;                            // ERR an array of structs, no pointer
  q = &s[1];                        // ERR the same
  i = s.x;                          // ERR a.f: no index
  i = s[1];                         // ERR a[i]: no field
  s[2] = i;                         // ERR the same
  s[i + 1] = i;                     // ERR the same
  s[1].y = s[2].x + 1;
  a.x = b.x + N + two;
  return a.x;
}
