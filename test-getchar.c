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
  if (c == 'a') putz("apple ");
  if (c == 'b') putz("banana ");
  if (c == 'c') putz("cat ");
  if (c == 'd') putz("dog ");
  if (c == 'e') putz("egg ");
  if (c == 'f') putz("fish ");
  if (c == 'g') putz("goat ");
  if (c == 'h') putz("hat ");
  if (c == 'i') putz("ice ");
  if (c == 'j') putz("jam ");
  if (c == 'k') putz("kite ");
  if (c == 'l') putz("lemon ");
  if (c == 'm') putz("moon ");
  if (c == 'n') putz("nose ");
  if (c == 'o') putz("owl ");
  if (c == 'p') putz("pig ");
  if (c == 'q') putz("queen ");
  if (c == 'r') putz("rat ");
  if (c == 's') putz("sun ");
  if (c == 't') putz("tree ");
  if (c == 'u') putz("umbrella ");
  if (c == 'v') putz("van ");
  if (c == 'w') putz("wolf ");
  if (c == 'x') putz("xray ");
  if (c == 'y') putz("yak ");
  if (c == 'z') putz("zebra ");
}

int main(void) {
  int c, n;

  seed = 4711;
  putz("type away (Esc quits)\n");

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
    if (c >= 'A' && c <= 'Z') c = c + 32; 

    // letter
    if (c >= 'a' && c <= 'z') { word(c); continue; }

    // printable
    if (c >= ' ' && c <= 126) { putchar(c); continue; }

    // just print code
    putchar('[');
    printf("%d", c);
    putz("] ");
  }

  putz("\nbye\n");
  return 0;
}
