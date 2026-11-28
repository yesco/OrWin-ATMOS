// test-constexpr.c - constant expressions, run by the compiler
//
// c.bnf rule y: an E whose code parse.c runs itself (gen_fold in
// ivm-gen.c), leaving only its value, as a const.
//
// Each case: name and the values C gives, (indented) what we got:
//
//   array size:     8194 11
//                   8194 11
//
// - array size: static int primes[number+1], char c[(N*2)+1]: sizeof
// - last elem:  primes[number], c[N*2]: written, read back
// - define:     #define SIZE (N*2), (SIZE-1), (1<<4): the values
// - paren:      (N+1)*(N-1), (100/N)%3: temps in the folding
// - elements:   {N+1, 2*N, 'A'+1}: initialized from expressions
// - global:     int g = 2+3; const int k = N*N;
// - local:      int a = N+1: folded; int b = a+1: not constant
// - char:       char d = 'A'+2; char e = v+1 (v = 255: wraps to 0)
// - struct:     struct pt p[N+1]: p[N].y
// - main first:     a static array as main's first line, with global inits

#include <stdio.h>

#define number 4096
#define N 5
#define SIZE (N*2)
#define LAST (SIZE-1)
#define BIT4 (1<<4)

static int i,j;
static int primes[number+1];
char c[(N*2)+1];
int w[SIZE];
int t[] = {N+1, 2*N, 7};
char s[] = {'A'+1, 'z', 0};

int g = 2+3;
const int k = N*N;

struct pt { int x; int y; };
struct pt pts[N+1];

int main() {
  static int first[N+1];
  int a = N+1;
  int b = a+1;
  char d = 'A'+2;
  int v = 255;
  char e = v+1;
  int n;

  putz("array size:\t8194 11\n\t\t");
  n = sizeof(primes); putn(n);
  n = sizeof(c); putnl(n);

  putz("last elem:\t11 22\n\t\t");
  primes[number] = 11;
  c[N*2] = 22;
  n = primes[number]; putn(n);
  n = c[10]; putnl(n);

  putz("define:\t\t10 9 16 20\n\t\t");
  n = SIZE; putn(n);
  n = LAST; putn(n);
  n = BIT4; putn(n);
  n = sizeof(w); putnl(n);

  putz("paren:\t\t24 2\n\t\t");
  {
    const int p = (N+1)*(N-1);
    const int q = (100/N)%3;
    n = p; putn(n);
    n = q; putnl(n);
  }

  putz("elements:\t6 10 7 66 122\n\t\t");
  n = t[0]; putn(n);
  n = t[1]; putn(n);
  n = t[2]; putn(n);
  n = s[0]; putn(n);
  n = s[1]; putnl(n);

  putz("global:\t\t5 25\n\t\t");
  putn(g);
  n = k; putnl(n);

  putz("local:\t\t6 7\n\t\t");
  putn(a);
  putnl(b);

  putz("char:\t\t67 0\n\t\t");
  n = d; putn(n);
  n = e; putnl(n);

  putz("struct:\t\t33\n\t\t");
  pts[N].y = 33;
  n = pts[N].y; putnl(n);

  putz("main first:\t12 5\n\t\t");
  n = sizeof(first); putn(n);
  putnl(g);
  return 0;
}
