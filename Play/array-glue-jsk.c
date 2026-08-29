#include <stdio.h>

void putwz(void* arr) {
  unsigned int x, *a= arr;
  while((x= *a++)) {
    if (x < 256)
      putchar((int)x);
    else
      fputs((char*)x, stdout);
  }
}

#define X(val) ((unsigned int)val)

// This macro accepts an arbitrary list of bytes, characters, or variables
// and unfolds them into sequential, lightning-fast instructions.
//  struct c_or_s _words[] = { __VA_ARGS__, 0 };	\

#define PRINT(...) do { \
  unsigned int _words[] = { __VA_ARGS__, 0 }; \
  putwz(_words); \
} while(0)



#define CDE "CDE"

int main(void) {
    char warning[] = "BOOM";
    // You can mix strings, characters, numbers, and attributes completely freely!
    // It creates an inline array inside a local scope block, meaning no permanent RAM waste.
    //PRINT('a', 'b', CDE, "FISH", 'x', 'y', 65);
    PRINT(X('a'), X('b'), X(CDE), X("FISH"), X('x'), X('y'), X(65));
    
    return 0;
}
