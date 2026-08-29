#include <stdio.h>

extern void oric_puts(const char* s);

// Drops raw text and explicit byte values sequentially directly into Oric ROM/RAM
#define ASM_PRINT(msg_str, ctrl_byte, count, char_byte) \
    __asm__ ( \
        ".rodata\n" \
        "L_str:\n" \
        ".string %s\n" \
        ".byte %b, %b, %b, 0\n" \
        ".code\n" \
        "lda #<L_str\n" \
        "ldx #>L_str\n" \
        "jsr _oric_puts\n", msg_str, ctrl_byte, count, char_byte \
    )

int main(void) {
    // This injects "SYSTEM ERROR" followed instantly by $1A, 20, and '*' directly into memory.
    ASM_PRINT("SYSTEM ERROR ", 0x1A, 20, '*');
    return 0;
}
