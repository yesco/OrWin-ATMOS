// test-memset.c - every JIT memset ('m') variant, with guard bytes
//
// b = malloc(BUF), p = b + 4: 4 guard bytes before p. Before each test
// b[] is filled with 170; after memset(p, v, len) chk() wants
//   b[3] == 170 (no underrun), p[0] == p[len-1] == v, p[len] == 170
// (no overrun). Prints len, or what failed.
//
// JIT variants ('m' in ivm-jit.c):
//   const 0          nothing emitted
//   const 1..256     inline: LDY #n; LDA #v; DEY; STA (zp),Y; BNE
//   const > 256      JSR _opmemset, count straight into X/Y
//   variable         JSR _opmemset, X/Y from the slot
//   computed (n+1)   count in AX: TAY, JSR _opmemset
//   for-loop form    for (i = 0; i < n; i++) p[i] = v;  -> 'm'
// Also: int* indexing uses the word ops ( ), char* the byte ops [ ].
//
// #TIMED "name": on the host simulator prints the cycles since the last
// #TIMED (e.g. --- memset 8191: 81985c (incl 0c page)); the VMs skip it.

#define BUF 8200

char *b;
char *p;
char *c;
int *w;
int n, a, k, i, err;

// fill b[0..len] with 170 (a while loop, so not itself a memset)
int fill(int len) {
  i = 0;
  while (i <= len) { b[i] = 170; i++; }
  return 0;
}

int chk(int len, int v) {
  a = b[3];
  if (a != 170) { printf("underrun "); err++; }
  if (len) {
    a = p[0];
    if (a != v) { printf("first "); err++; }
    k = len - 1;
    a = p[k];
    if (a != v) { printf("last "); err++; }
  }
  k = len + 4;
  a = b[k];
  if (a != 170) { printf("overrun "); err++; }
  printf("%d\n", len);
  return err;
}

int main() {
  err = 0;
  b = malloc(BUF);
  p = b + 4;

#TIMED "start"
#TIMED "zero"
  // const 0: nothing
  fill(20);  memset(p, 9, 0);     chk(0, 9);
  // const 1..256: inline
  fill(20);  memset(p, 1, 1);     chk(1, 1);
  fill(300); memset(p, 2, 255);   chk(255, 2);
  fill(300); memset(p, 3, 256);   chk(256, 3);
  // const > 256: JSR _opmemset (lo != 0, lo == 0)
  fill(300); memset(p, 4, 257);   chk(257, 4);
  fill(600); memset(p, 5, 512);   chk(512, 5);
#TIMED "fill+checks"
  fill(8195);
#TIMED "fill 8196"
  memset(p, 6, 8191);
#TIMED "memset 8191"
  chk(8191, 6);
  // variable count: X/Y from the slot
  n = 300;
  fill(400); memset(p, 7, n);     chk(300, 7);
  n = 0;
  fill(20);  memset(p, 8, n);     chk(0, 8);
  // computed count: in AX
  n = 99;
  fill(200); memset(p, 10, n + 1); chk(100, 10);
  // the for-loop form becomes 'm' too
  n = 77;
  fill(100);
  for (i = 0; i < n; i++) p[i] = 11;
  chk(77, 11);

  // int*: word ops ( ); the bytes in memory are lo, hi
  w = malloc(20);
  w[3] = 1234;
  a = w[3];
  printf("%d\n", a);
  c = w;
  a = c[6];
  printf("%d\n", a);
  a = c[7];
  printf("%d\n", a);

  printf("errors: ");
  printf("%d\n", err);
  return err;
}
