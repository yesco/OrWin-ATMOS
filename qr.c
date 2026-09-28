#include <stdio.h>
#include <string.h>

// Standard QR Alphanumeric conversion index chart
const char ALPHANUM_TABLE[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

// Reed-Solomon generator polynomial coefficients for 7 error-correction bytes (QR v1-L)
const unsigned char RS_POLY[] = {127, 122, 154, 164, 11, 68, 117};

// Global buffers explicitly sized to completely prevent stack overflow corruption
unsigned char data_bytes[19]; 
unsigned char ecc_bytes[7];   
unsigned char bit_buffer[63]; // Sized perfectly to 21 rows * 3 packed bytes

// GF(2^8) multiply optimized for minimal 8-bit code execution footprint (no big tables)
unsigned char gf_mul(unsigned char a, unsigned char b) {
    unsigned char result = 0;
    while (b > 0) {
        if (b & 1) result ^= a;
        a = (a << 1) ^ (a & 0x80 ? 0x1D : 0); // Primitive polynomial x^8 + x^4 + x^3 + x^2 + 1
        b >>= 1;
    }
    return result;
}

// Convert a single character to its QR Alphanumeric index value (includes Auto-Uppercase)
int get_alphanumeric_val(char c) {
    unsigned char i;
    if (c >= 'a' && c <= 'z') c -= 32; 
    for (i = 0; i < 45; ++i) {
        if (ALPHANUM_TABLE[i] == c) return i;
    }
    return -1; // Flag invalid characters
}

// Low-level function to write a single bit into the packed 63-byte frame buffer
void write_bit(unsigned char x, unsigned char y, unsigned char bit) {
    unsigned int bit_pos = (y * 24) + x;
    unsigned int byte_idx = bit_pos / 8;
    unsigned char bit_idx = 7 - (bit_pos % 8);
    
    if (bit) {
        bit_buffer[byte_idx] |= (1 << bit_idx);
    } else {
        bit_buffer[byte_idx] &= ~(1 << bit_idx);
    }
}

// Appends bits to a raw byte array (used for encoding the text string)
void append_bits_to_buffer(unsigned int value, unsigned char num_bits, unsigned int *bit_offset) {
    int i;
    for (i = num_bits - 1; i >= 0; --i) {
        unsigned int byte_idx = (*bit_offset) / 8;
        unsigned char bit_idx = 7 - ((*bit_offset) % 8);
        
        if ((value >> i) & 1) {
            data_bytes[byte_idx] |= (1 << bit_idx);
        }
        (*bit_offset)++;
    }
}

// Compiles the string into standard QR 11-bit pairs
int encode_string(const char* str, unsigned char length) {
    unsigned int bit_offset = 0;
    unsigned char i;
    int v1, v2;
    unsigned int pair_val;

    memset(data_bytes, 0, 19);

    // 1. Mode Indicator for Alphanumeric Mode: 0010 (4 bits)
    append_bits_to_buffer(0x02, 4, &bit_offset);

    // 2. Character Count Indicator for Version 1: length (9 bits)
    append_bits_to_buffer(length, 9, &bit_offset);

    // 3. Encode characters in 11-bit pairs
    for (i = 0; i < length; i += 2) {
        v1 = get_alphanumeric_val(str[i]);
        if (v1 < 0) return 0; // Error: Illegal character encountered!

        if (i + 1 < length) {
            // Two characters left: encode as a pair using 11 bits
            v2 = get_alphanumeric_val(str[i + 1]);
            if (v2 < 0) return 0; // Error: Illegal character encountered!
            pair_val = (v1 * 45) + v2;
            append_bits_to_buffer(pair_val, 11, &bit_offset);
        } else {
            // One single character left at the end: encode using 6 bits
            append_bits_to_buffer(v1, 6, &bit_offset);
        }
    }

    // 4. Terminator: Pad up to 4 zero bits if space remains
    append_bits_to_buffer(0x00, 4, &bit_offset);

    return 1; // Success
}

// Calculates Reed-Solomon error correction bytes
void calculate_ecc(void) {
    unsigned char i, j, feedback;
    memset(ecc_bytes, 0, 7);
    for (i = 0; i < 19; ++i) {
        feedback = data_bytes[i] ^ ecc_bytes[0];
        for (j = 0; j < 6; ++j) {
            ecc_bytes[j] = ecc_bytes[j + 1] ^ gf_mul(feedback, RS_POLY[j]);
        }
        ecc_bytes[6] = gf_mul(feedback, RS_POLY[6]);
    }
}

// Renders the fixed structural shapes (the 3 large corner targets and timing lines)
void inject_fixed_patterns(void) {
    unsigned char x, y;
    
    // Clear whole buffer to white/zero first
    memset(bit_buffer, 0, 63);

    // Draw Top-Left, Top-Right, and Bottom-Left 7x7 Finder Squares
    for (y = 0; y < 21; ++y) {
        for (x = 0; x < 21; ++x) {
            if ((x < 7 && y < 7) || (x > 13 && y < 7) || (x < 7 && y > 13)) {
                unsigned char px = (x > 13) ? (x - 14) : x;
                unsigned char py = (y > 13) ? (y - 14) : y;
                
                if (px == 0 || px == 6 || py == 0 || py == 6 || (px >= 2 && px <= 4 && py >= 2 && py <= 4)) {
                    write_bit(x, y, 1);
                }
            }
            // Add the dashed horizontal/vertical Timing Tracks
            else if (y == 6 && (x & 1) == 0) {
                write_bit(x, y, 1);
            }
            else if (x == 6 && (y & 1) == 0) {
                write_bit(x, y, 1);
            }
            // Data zone placeholder pattern to ensure structure alignment
            else {
                if ((x + y) & 1) {
                    write_bit(x, y, 1);
                }
            }
        }
    }
}

int main(void) {
    const char* my_input = "ABC"; // You can safely change this up to 25 characters now!
    unsigned char len = strlen(my_input);
    unsigned char row, b_idx, bit_idx, current_byte;
    unsigned int ptr = 0;

    if (len > 25 || len == 0) {
        printf("ERROR: String length must be between 1 and 25 characters.\n");
        return 1;
    }

    // Run dynamic string validation and encoding
    if (!encode_string(my_input, len)) {
        printf("ERROR: String contains illegal characters!\n");
        return 1;
    }

    // Run layout engines
    calculate_ecc();          
    inject_fixed_patterns();  

    // Print top edge of container frame using 'X's instead of dashes
    printf("\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
    printf("XX                                                          XX\n");
    printf("XX                                                          XX\n");

    for (row = 0; row < 21; ++row) {
        printf("XX        "); // Left protective frame + Quiet zone margin
        for (b_idx = 0; b_idx < 3; ++b_idx) {
            current_byte = bit_buffer[ptr++];
            for (bit_idx = 0; bit_idx < 8; ++bit_idx) {
                if ((b_idx * 8) + bit_idx >= 21) break;
                
                // Print "XX" for dark blocks, two spaces for light blocks
                printf(current_byte & 0x80 ? "XX" : "  ");
                current_byte <<= 1;
            }
        }
        printf("        XX\n"); // Right protective frame + Quiet zone margin
    }

    printf("XX                                                          XX\n");
    printf("XX                                                          XX\n");
    printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n\n");

    return 0;
}
