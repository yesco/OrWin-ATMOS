// test-shifts.c - << >> and * / % by powers of two, 16 bit unsigned
//
// The JIT does these inline (no JSR):
//   n >= 8      a byte move: >> 8: TXA / LDX #0, << 8: TAX / LDA #0,
//               then the rest of n in A (LSR A / ASL A)
//   n < 8       hi via $FF: STX $FF; n x (ASL A; ROL $FF); LDX $FF
//   << 1, * 2   the doubling (ASL / ROL)
//   % 2^n       AND #mask (n < 8: and LDX #0), hi byte masked (n >= 8)
//   by a var    a loop on Y
//   const OP const: folded
// Each case: its name and the values C gives, then (indented) what we got:
//   x << k:         37429 9322 ...
//                   37429 9322 ...
// Each single shift sits between two #TIMED: on the host simulator the
// line "=======  Nc  NB - x << 3" is the cycles and bytes of just
// "y = x << 3;" (incl. its load and store); the VMs skip #TIMED.
// x = 37429 ($9235): bits in both bytes. Values are 16 bit unsigned.

int main() {
  int x, y, n;

  x = 37429;

  printf("x << k:\t37429 9322 18644 37288 9040 6784 13568 27136 20480 32768 0\n\t");
#TIMED
  y = x << 0;
#TIMED "x << 0"
  printf("%u ", y);
#TIMED
  y = x << 1;
#TIMED "x << 1"
  printf("%u ", y);
#TIMED
  y = x << 2;
#TIMED "x << 2"
  printf("%u ", y);
#TIMED
  y = x << 3;
#TIMED "x << 3"
  printf("%u ", y);
#TIMED
  y = x << 4;
#TIMED "x << 4"
  printf("%u ", y);
#TIMED
  y = x << 7;
#TIMED "x << 7"
  printf("%u ", y);
#TIMED
  y = x << 8;
#TIMED "x << 8"
  printf("%u ", y);
#TIMED
  y = x << 9;
#TIMED "x << 9"
  printf("%u ", y);
#TIMED
  y = x << 12;
#TIMED "x << 12"
  printf("%u ", y);
#TIMED
  y = x << 15;
#TIMED "x << 15"
  printf("%u ", y);
#TIMED
  y = x << 16;
#TIMED "x << 16"
  printf("%u ", y);
  printf("\n");

  printf("x >> k:\t37429 18714 9357 4678 2339 292 146 73 9 1 0\n\t");
#TIMED
  y = x >> 0;
#TIMED "x >> 0"
  printf("%u ", y);
#TIMED
  y = x >> 1;
#TIMED "x >> 1"
  printf("%u ", y);
#TIMED
  y = x >> 2;
#TIMED "x >> 2"
  printf("%u ", y);
#TIMED
  y = x >> 3;
#TIMED "x >> 3"
  printf("%u ", y);
#TIMED
  y = x >> 4;
#TIMED "x >> 4"
  printf("%u ", y);
#TIMED
  y = x >> 7;
#TIMED "x >> 7"
  printf("%u ", y);
#TIMED
  y = x >> 8;
#TIMED "x >> 8"
  printf("%u ", y);
#TIMED
  y = x >> 9;
#TIMED "x >> 9"
  printf("%u ", y);
#TIMED
  y = x >> 12;
#TIMED "x >> 12"
  printf("%u ", y);
#TIMED
  y = x >> 15;
#TIMED "x >> 15"
  printf("%u ", y);
#TIMED
  y = x >> 16;
#TIMED "x >> 16"
  printf("%u ", y);
  printf("\n");

  printf("x << n:\t37429 9322 18644 37288 9040 6784 13568 27136 20480 32768 0\n\t");
#TIMED
  n = 0; y = x << n;
#TIMED "n = 0; x << n"
  printf("%u ", y);
#TIMED
  n = 1; y = x << n;
#TIMED "n = 1; x << n"
  printf("%u ", y);
#TIMED
  n = 2; y = x << n;
#TIMED "n = 2; x << n"
  printf("%u ", y);
#TIMED
  n = 3; y = x << n;
#TIMED "n = 3; x << n"
  printf("%u ", y);
#TIMED
  n = 4; y = x << n;
#TIMED "n = 4; x << n"
  printf("%u ", y);
#TIMED
  n = 7; y = x << n;
#TIMED "n = 7; x << n"
  printf("%u ", y);
#TIMED
  n = 8; y = x << n;
#TIMED "n = 8; x << n"
  printf("%u ", y);
#TIMED
  n = 9; y = x << n;
#TIMED "n = 9; x << n"
  printf("%u ", y);
#TIMED
  n = 12; y = x << n;
#TIMED "n = 12; x << n"
  printf("%u ", y);
#TIMED
  n = 15; y = x << n;
#TIMED "n = 15; x << n"
  printf("%u ", y);
#TIMED
  n = 16; y = x << n;
#TIMED "n = 16; x << n"
  printf("%u ", y);
  printf("\n");

  printf("x >> n:\t37429 18714 9357 4678 2339 292 146 73 9 1 0\n\t");
#TIMED
  n = 0; y = x >> n;
#TIMED "n = 0; x >> n"
  printf("%u ", y);
#TIMED
  n = 1; y = x >> n;
#TIMED "n = 1; x >> n"
  printf("%u ", y);
#TIMED
  n = 2; y = x >> n;
#TIMED "n = 2; x >> n"
  printf("%u ", y);
#TIMED
  n = 3; y = x >> n;
#TIMED "n = 3; x >> n"
  printf("%u ", y);
#TIMED
  n = 4; y = x >> n;
#TIMED "n = 4; x >> n"
  printf("%u ", y);
#TIMED
  n = 7; y = x >> n;
#TIMED "n = 7; x >> n"
  printf("%u ", y);
#TIMED
  n = 8; y = x >> n;
#TIMED "n = 8; x >> n"
  printf("%u ", y);
#TIMED
  n = 9; y = x >> n;
#TIMED "n = 9; x >> n"
  printf("%u ", y);
#TIMED
  n = 12; y = x >> n;
#TIMED "n = 12; x >> n"
  printf("%u ", y);
#TIMED
  n = 15; y = x >> n;
#TIMED "n = 15; x >> n"
  printf("%u ", y);
#TIMED
  n = 16; y = x >> n;
#TIMED "n = 16; x >> n"
  printf("%u ", y);
  printf("\n");

  printf("x*2^k:\t9322 18644 37288 9040 6784 13568 27136 32768\n\t");
#TIMED
  y = x * 2;
#TIMED "x * 2"
  printf("%u ", y);
#TIMED
  y = x * 4;
#TIMED "x * 4"
  printf("%u ", y);
#TIMED
  y = x * 8;
#TIMED "x * 8"
  printf("%u ", y);
#TIMED
  y = x * 16;
#TIMED "x * 16"
  printf("%u ", y);
#TIMED
  y = x * 128;
#TIMED "x * 128"
  printf("%u ", y);
#TIMED
  y = x * 256;
#TIMED "x * 256"
  printf("%u ", y);
#TIMED
  y = x * 512;
#TIMED "x * 512"
  printf("%u ", y);
#TIMED
  y = x * 32768;
#TIMED "x * 32768"
  printf("%u ", y);
  printf("\n");

  printf("x/2^k:\t18714 9357 4678 2339 292 146 73 1\n\t");
#TIMED
  y = x / 2;
#TIMED "x / 2"
  printf("%u ", y);
#TIMED
  y = x / 4;
#TIMED "x / 4"
  printf("%u ", y);
#TIMED
  y = x / 8;
#TIMED "x / 8"
  printf("%u ", y);
#TIMED
  y = x / 16;
#TIMED "x / 16"
  printf("%u ", y);
#TIMED
  y = x / 128;
#TIMED "x / 128"
  printf("%u ", y);
#TIMED
  y = x / 256;
#TIMED "x / 256"
  printf("%u ", y);
#TIMED
  y = x / 512;
#TIMED "x / 512"
  printf("%u ", y);
#TIMED
  y = x / 32768;
#TIMED "x / 32768"
  printf("%u ", y);
  printf("\n");

  printf("x%2^k:\t1 1 5 5 53 53 53 4661 0\n\t");
#TIMED
  y = x % 2;
#TIMED "x % 2"
  printf("%u ", y);
#TIMED
  y = x % 4;
#TIMED "x % 4"
  printf("%u ", y);
#TIMED
  y = x % 8;
#TIMED "x % 8"
  printf("%u ", y);
#TIMED
  y = x % 16;
#TIMED "x % 16"
  printf("%u ", y);
#TIMED
  y = x % 128;
#TIMED "x % 128"
  printf("%u ", y);
#TIMED
  y = x % 256;
#TIMED "x % 256"
  printf("%u ", y);
#TIMED
  y = x % 512;
#TIMED "x % 512"
  printf("%u ", y);
#TIMED
  y = x % 32768;
#TIMED "x % 32768"
  printf("%u ", y);
#TIMED
  y = x % 1;
#TIMED "x % 1"
  printf("%u ", y);
  printf("\n");

  printf("const:\t9872 78 62 40\n\t");
#TIMED
  y = 1234 << 3;
#TIMED "1234 << 3"
  printf("%u ", y);
#TIMED
  y = 40000 >> 9;
#TIMED "40000 >> 9"
  printf("%u ", y);
#TIMED
  y = 1000 / 16;
#TIMED "1000 / 16"
  printf("%u ", y);
#TIMED
  y = 1000 % 64;
#TIMED "1000 % 64"
  printf("%u ", y);
  printf("\n");

  printf("in AX:\t146 37296 9357 54\n\t");
#TIMED
  y = (x + 1) >> 8;
#TIMED "(x + 1) >> 8"
  printf("%u ", y);
#TIMED
  y = (x + 1) << 3;
#TIMED "(x + 1) << 3"
  printf("%u ", y);
#TIMED
  y = (x + 1) / 4;
#TIMED "(x + 1) / 4"
  printf("%u ", y);
#TIMED
  y = (x + 1) % 512;
#TIMED "(x + 1) % 512"
  printf("%u ", y);
  printf("\n");

  return 0;
}
