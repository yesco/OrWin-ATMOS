// man-io.c - MeteoriC-man.md 11: output (the printf forms that work: one
// number per printf, text in a printf of its own)
//
// Each case: its name and the values C gives, then (indented) what we got.
// gcc on the same file is the reference.

#include <stdio.h>

int main(void) {
  int a;
  char *s;

  a = 42;
  s = "str";

  printf("printf:\t\t42 42 7 a=42 str\n\t\t");
  printf("%d ", a);
  printf("%u ", a);
  printf("%d", 7);
  printf(" a=");
  printf("%d ", a);
  printf("%s", s);
  printf("\n");

  printf("three:\t\t123\n\t\t");
  printf("%u%u%u", 1, 2, 3);
  printf("\n");

  printf("putchar:\tAB\n\t\t");
  putchar('A');
  putchar(66);
  putchar('\n');

  printf("puts:\t\tx\n\t\t");
  puts("x");

  printf("fputs:\t\tyz\n\t\t");
  fputs("y", stdout);
  fputs("z\n", stdout);
  fflush(stdout);
  return 0;
}
