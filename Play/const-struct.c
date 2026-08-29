#include <stdio.h>

// Define a tiny wrapper structure for your 3-byte and 1-byte outputs
typedef struct { char s[4]; } rle_str;
typedef struct { char s[2]; } one_str;

// Create inline anonymous structures and immediately pass their internal array address
#define REPEAT_STR(count, ch) (((rle_str){0x1A, count, ch, 0}).s)
#define BYTE_STR(val)         (((one_str){val, 0}).s)

int main(void) {
    // This compiles perfectly in cc65 because it treats struct initializers as values!
    puts(BYTE_STR(42));
    puts(REPEAT_STR(20, '*'));
    return 0;
}
