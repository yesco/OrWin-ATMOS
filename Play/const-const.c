#include <stdio.h>

#define STRING "bar"

#define XYZ STRING[0

// Turns a raw byte value into a string literal pointer
#define BYTE_STR(val) (char[]){val, 0}

// Turns your Ctrl+Z sequence (Ctrl+Z, count, char) into a string pointer
#define REPEAT_STR(count, ch) (char[]){0x1A, count, ch, 0}

int main(void) {

  puts("x" ## bar);

  // You can now pass it directly into functions that expect a string pointer!
  //puts(BYTE_STR(42)); 
    
  // Generates a 3-byte escape token stream to repeat '*' 20 times
  //puts(REPEAT_STR(20, '*')); 
}
