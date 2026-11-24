// test-getchar.c - a silly typewriter: getchar, putchar, printf
//
// Each letter you type prints a word starting with it (and a space), so
// typing "hi" gives "hat ice ". Delete / backspace rubs out a random
// number (1-5) of characters: back, space over it, back. Return is a
// new line, other keys are echoed. Esc or EOF (end of input) quits.
//
// Plain C: gcc test-getchar.c && ./a.out (the terminal is line buffered,
// type a line and Return). On the Oric: ./oric --live test-getchar, then
// type (CAPS on: capitals are words too).
// No input (ptest: EOF at once): just the two lines.

#include <stdio.h>

int seed;

// a pseudo random 1..5 (a LCG, the hi bits)
int rnd5() {
  int n;
  seed = seed * 75;
  seed = seed + 74;
  seed = seed & 32767;                    // 15 bits: the same in C (32 bit)
  n = seed >> 12;
  n = n % 5;
  return n + 1;
}

void word(int c) {
  if (c == 'a') printf("apple ");
  if (c == 'b') printf("banana ");
  if (c == 'c') printf("cat ");
  if (c == 'd') printf("dog ");
  if (c == 'e') printf("egg ");
  if (c == 'f') printf("fish ");
  if (c == 'g') printf("goat ");
  if (c == 'h') printf("hat ");
  if (c == 'i') printf("ice ");
  if (c == 'j') printf("jam ");
  if (c == 'k') printf("kite ");
  if (c == 'l') printf("lemon ");
  if (c == 'm') printf("moon ");
  if (c == 'n') printf("nose ");
  if (c == 'o') printf("owl ");
  if (c == 'p') printf("pig ");
  if (c == 'q') printf("queen ");
  if (c == 'r') printf("rat ");
  if (c == 's') printf("sun ");
  if (c == 't') printf("tree ");
  if (c == 'u') printf("umbrella ");
  if (c == 'v') printf("van ");
  if (c == 'w') printf("wolf ");
  if (c == 'x') printf("xray ");
  if (c == 'y') printf("yak ");
  if (c == 'z') printf("zebra ");
}

int main(void) {
  int c, n;

  seed = 4711;
  printf("type away (Esc quits)\n");

  while (1) {
    c = getchar();
    if (c < 0) break;    // EOF: C's -1
    if (c > 255) break;  //      ours (unsigned) $FFFF
    if (c == 255) break; //      a char: oscar64's emulator
    if (c == 27) break;  // Esc

    // Delete: as backspace
    if (c == 127) c = 8; 
    if (c == 8) {
      n = rnd5();
      while (n) { putchar(8); putchar(' '); putchar(8); n--; }
      continue;
    }

    // capitals: the same words
    if (c >= 'A') if (c <= 'Z') c = c + 32; 

    // letter
    if (c >= 'a') if (c <= 'z') { word(c); continue; }

    // printable
    if (c >= ' ') if (c <= 126) { putchar(c); continue; }

    // just print code
    putchar('[');
    printf("%d", c);
    putz("] ");
  }

  printf("\nbye\n");
  return 0;
}
