// test-shiftrules.c - shifts by a const that the { } rules do in the JIT
//
// test-shifts.c has the common counts; this one has the rest:
//   << 5, 6  >> 5, 6   a loop on Y: STX $FF; LDY #n; l: ASL A; ROL $FF;
//                      DEY; BNE l; LDX $FF  (smaller than 5, 6 x ASL/ROL)
//   char >> n          A only: n x LSR A, X = 0; >> 7: ASL A; LDA #0; ROL A
//   char >> 8..        0: LDA #0; TAX
//   char << n          as a word (a char is promoted to int)
//   word >> 9          the hi byte to A, X = 0, then as a char >> 1
// Each case: its name and the values C gives, then (indented) what we got:
//   x << k:         18080 36160
//                   18080 36160
// x = 37429 ($9235), c = 105 ($69). Values are 16 bit unsigned.

int main() {
  int x, y;
  char c;

  x = 37429;
  c = 105;

  printf("x<< 5 6:\t18080 36160\n\t\t");
  y = x << 5;
  printf("%u ", y);
  y = x << 6;
  printf("%u\n", y);

  printf("x>> 5 6:\t1169 584\n\t\t");
  y = x >> 5;
  printf("%u ", y);
  y = x >> 6;
  printf("%u\n", y);

  printf("c>> 1 3 7 8 10:\t52 13 0 0 0\n\t\t");
  y = c >> 1;
  printf("%u ", y);
  y = c >> 3;
  printf("%u ", y);
  y = c >> 7;
  printf("%u ", y);
  y = c >> 8;
  printf("%u ", y);
  y = c >> 10;
  printf("%u\n", y);

  printf("c<< 5 9:\t3360 53760\n\t\t");
  y = c << 5;
  printf("%u ", y);
  y = c << 9;
  printf("%u\n", y);

  printf("(x + c) >> 9:\t73\n\t\t");
  y = (x + c) >> 9;
  printf("%u\n", y);
  return 0;
}
