// test-printf.c - printf's formats, as test-printf-asm.asm does them
//
// The same cases, in C: what printf gives (gcc) is test-printf.out,
// also test-printf-asm.out. For later: when the compiler splits a
// printf format into ivm-lib's putf calls (putfu putfd putfx putfX
// putfs, ...z, putfz), this must print the same.
//
// Tested:
//
// - %u %d %x %X %s: width, -width (left), 0 pad, .N, a value too wide
// - "%u seven": a value, then text
// - printf's result: the chars printed, summed (putfn)

#include <stdio.h>

int main(void) {
  int n = 0;
  n += printf("%u|\n", 42);
  n += printf("%5u|\n", 42);
  n += printf("%-5u|\n", 42);
  n += printf("%05u|\n", 42);
  n += printf("%u|\n", 65535);
  n += printf("%u|\n", 0);
  n += printf("%.4u|\n", 42);
  n += printf("%6.4u|\n", 42);
  n += printf("%3u|\n", 12345);
  n += printf("%5d|\n", -42);
  n += printf("%05d|\n", -42);
  n += printf("%-5d|\n", -42);
  n += printf("%d|\n", 1234);
  n += printf("%d|\n", -32768);
  n += printf("%3d|\n", 0);
  n += printf("%x|\n", 48879);
  n += printf("%X|\n", 48879);
  n += printf("%04x|\n", 255);
  n += printf("%x|\n", 0);
  n += printf("%-3X|\n", 10);
  n += printf("%8s|\n", "hello");
  n += printf("%-8s|\n", "hello");
  n += printf("%.3s|\n", "hello");
  n += printf("%5.2s|\n", "hello");
  n += printf("%2s|\n", "");
  n += printf("%2s|\n", "hello");
  n += printf("%u seven\n", 7);
  printf("%d chars\n", n);
  return 0;
}
