#include <stdio.h>

#define RED    "\x01"
#define GREEN  "\x02"
#define CTRL_Z "\x1A"

// This macro glues a hex count byte and a hex char byte into a single \x literal string
#define RLE_HEX(hex_count, hex_char) CTRL_Z "\x" #hex_count "\x" #hex_char

int main(void) {
    // Passes 14 (20 in decimal) and 2a ('*' in ASCII) as raw tokens.
    // The preprocessor expands this cleanly to: "\x1A" "\x14" "\x2a"
    // Which cc65 compiles into a single sequence of 3 static bytes!
    printf(RED "Error! " GREEN RLE_HEX(14, 2a) " Done\n");
    return 0;
}
