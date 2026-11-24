// test-string.c - the library calls: memmove memcpy (V), strlen ($),
// strcmp strncmp memcmp, strchr memchr, strcpy strncpy (x y)
//
// Plain C: gcc test-string.c && ./a.out gives the reference output.
// Compares: only the sign is C's (glibc: -128 / 128, ours: -1 / 1), and
// our ints are unsigned: sg() prints 0, + or -.
// Only the printf forms the grammar knows: "%d ", "%d\n" and "%s".

#include <stdio.h>
#include <string.h>

char a[16];
char b[16];

// the sign of a compare: 0, + or -
void sg(int r) {
  if (r == 0) putchar('0');
  else if (r & 0x8000) putchar('-');
  else putchar('+');
  putchar(' ');
}

int main(void) {
  char *p;
  int n;

  // --- strlen, strcpy --------------------------------
  putchar('A'); putchar(' ');
  strcpy(a, "hello");
  printf("%d ", strlen(a));
  printf("%d ", strlen("12345678"));
  printf("%d ", strlen(""));
  p = strcpy(b, a);
  printf("%d ", p == b);
  printf("%s", b);
  putchar('\n');

  // --- strcmp strncmp memcmp -------------------------
  putchar('B'); putchar(' ');
  sg(strcmp(a, b));
  b[1] = 'a';
  sg(strcmp(a, b));
  sg(strcmp(b, a));
  sg(strncmp(a, b, 1));
  sg(memcmp(a, b, 2));
  sg(strcmp(a, "hell"));
  sg(strcmp("hell", a));
  putchar('\n');

  // --- strchr memchr: where, or 0 --------------------
  putchar('C'); putchar(' ');
  p = strchr(a, 'l');
  printf("%d ", p - a);
  p = strchr(a, 'z');
  printf("%d ", p == 0);
  p = strchr(a, 0);
  printf("%d ", p - a);
  p = memchr(a, 'o', 5);
  printf("%d ", p - a);
  p = memchr(a, 'o', 4);
  printf("%d ", p == 0);
  putchar('\n');

  // --- memmove (overlapping, both ways), memcpy ------
  putchar('D'); putchar(' ');
  strcpy(a, "hello");
  memmove(a + 1, a, 5);
  printf("%s", a);
  putchar(' ');
  memmove(a, a + 1, 6);
  printf("%s", a);
  putchar(' ');
  memcpy(b, "xyz", 4);
  printf("%s", b);
  putchar('\n');

  // --- strncpy: NULs up to n -------------------------
  putchar('E'); putchar(' ');
  memcpy(b, "ABCDEFGH", 9);
  strncpy(b, "ab", 6);
  printf("%d ", b[2]);
  printf("%d ", b[5]);
  printf("%d ", b[6]);
  n = strlen(b);
  printf("%d\n", n);

  return 0;
}
