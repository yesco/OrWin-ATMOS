#include <stdio.h>

// A custom low-level character writer
extern void oric_putc(char c);

// This macro accepts an arbitrary list of bytes, characters, or variables
// and unfolds them into sequential, lightning-fast instructions.
#define GLUE_PRINT(...) do { \
    char _bytes[] = { __VA_ARGS__ }; \
    unsigned char _i; \
    for(_i = 0; _i < sizeof(_bytes); ++_i) oric_putc(_bytes[_i]); \
} while(0)

int main(void) {
    char warning[] = "BOOM";
    
    // You can mix strings, characters, numbers, and attributes completely freely!
    // It creates an inline array inside a local scope block, meaning no permanent RAM waste.
    GLUE_PRINT(0x01, 'W', 'a', 'r', 'n', 'i', 'n', 'g', ':', 0x02, 0x1A, 20, '*', '\n');
    
    return 0;
}
