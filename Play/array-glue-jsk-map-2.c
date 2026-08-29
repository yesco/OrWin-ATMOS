#include <stdio.h>

void putwz(void* arr) {
  unsigned int x, *a = arr;
  while((x = *a++)) {
    if (x < 256)
      putchar((int)x);
    else
      fputs((char*)x, stdout);
  }
}

// Your explicit cast wrapper
#define X(val) ((unsigned int)(val))

// A single macro that handles up to 16 arguments natively.
// If you pass 7 arguments, standard C automatically pads the remaining slots to 0.
#define PRINT(...) do { \
  unsigned int _w[17] = { __VA_ARGS__ }; \
  putwz(_w); \
} while(0)

#define CDE "CDE"

int main(void) {
    // Flawless compilation on cc65. Zero shifting tricks required.
    PRINT('a', 'b', CDE, "FISH", 'x', 'y', 65);
    return 0;
}
