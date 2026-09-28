#include <stdio.h>
#include <string.h>

const char ALPHANUM_TABLE[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
// The mathematically correct generator polynomial sequence for QR v1-L division
const unsigned char RS_POLY[] = {0x75, 0x44, 0x0B, 0xA4, 0x9A, 0x7A, 0x7F};

unsigned char data_bytes[19]; 
unsigned char ecc_bytes[7];   
unsigned char bit_buffer[63]; 

unsigned char gf_mul(unsigned char a, unsigned char b) {
    unsigned char result = 0;
    while (b > 0) {
        if (b & 1) result ^= a;
        a = (a << 1) ^ (a & 0x80 ? 0x1D : 0); 
        b >>= 1;
    }
    return result;
}

int get_alphanumeric_val(char c) {
    unsigned char i;
    if (c >= 'a' && c <= 'z') c -= 32; 
    for (i = 0; i < 45; ++i) {
        if (ALPHANUM_TABLE[i] == c) return i;
    }
    return -1; 
}

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

int encode_string(const char* str, unsigned char length) {
    unsigned int bit_offset = 0;
    unsigned char i;
    int v1, v2;
    unsigned int pair_val;

    memset(data_bytes, 0, 19);
    append_bits_to_buffer(0x02, 4, &bit_offset);   
    append_bits_to_buffer(length, 9, &bit_offset); 

    for (i = 0; i < length; i += 2) {
        v1 = get_alphanumeric_val(str[i]);
        if (v1 < 0) return 0;
        if (i + 1 < length) {
            v2 = get_alphanumeric_val(str[i + 1]);
            if (v2 < 0) return 0;
            pair_val = (v1 * 45) + v2;
            append_bits_to_buffer(pair_val, 11, &bit_offset);
        } else {
            append_bits_to_buffer(v1, 6, &bit_offset);
        }
    }
    append_bits_to_buffer(0x00, 4, &bit_offset);   
    
    int byte_offset = (bit_offset + 7) / 8;
    int pad_toggle = 0;
    while (byte_offset < 19) {
        data_bytes[byte_offset++] = pad_toggle ? 0x11 : 0xEC;
        pad_toggle = !pad_toggle;
    }
    return 1;
}

void calculate_ecc(void) {
    unsigned char i, j, feedback;
    memset(ecc_bytes, 0, 7);
    
    for (i = 0; i < 19; ++i) {
        feedback = data_bytes[i] ^ ecc_bytes[0];
        
        for (j = 0; j < 6; ++j) {
            ecc_bytes[j] = ecc_bytes[j + 1];
        }
        ecc_bytes[6] = 0;
        
        if (feedback != 0) {
            for (j = 0; j < 7; ++j) {
                ecc_bytes[j] ^= gf_mul(feedback, RS_POLY[j]);
            }
        }
    }
}

// FULLY FIXED COORDINATE PROTECTION ZONE: Shields finders, timing lines, and format metadata
unsigned char is_fixed_zone(unsigned char x, unsigned char y) {
    if (x < 9 && y < 9) return 1;    // Top-Left Finder Zone + White Separation margin
    if (x > 12 && y < 9) return 1;   // Top-Right Finder Zone + White Separation margin
    if (x < 9 && y > 12) return 1;   // Bottom-Left Finder Zone + White Separation margin
    if (x == 6 || y == 6) return 1;  // Timing track stripes
    if (x == 8 && (y <= 8 || y >= 13)) return 1; // Vertical Format Information blocks
    if (y == 8 && (x <= 8 || x >= 14)) return 1; // Horizontal Format Information blocks
    return 0;
}

void generate_matrix(void) {
    unsigned char x, y;
    int col;
    unsigned int main_ptr = 0;
    unsigned char current_bit;
    unsigned char actual_byte;
    int dir = -1; 

    memset(bit_buffer, 0, 63);

    // 1. Plot Fixed Finder Patterns
    for (y = 0; y < 21; ++y) {
        for (x = 0; x < 21; ++x) {
            if ((x < 7 && y < 7) || (x > 13 && y < 7) || (x < 7 && y > 13)) {
                unsigned char px = (x > 13) ? (x - 14) : x;
                unsigned char py = (y > 13) ? (y - 14) : y;
                if (px == 0 || px == 6 || py == 0 || py == 6 || (px >= 2 && px <= 4 && py >= 2 && py <= 4)) {
                    write_bit(x, y, 1);
                }
            } 
            // 2. Plot Alternating Timing Track Dashes
            else if (y == 6 && (x >= 8 && x <= 12)) {
                if ((x & 1) == 0) write_bit(x, y, 1);
            } else if (x == 6 && (y >= 8 && y <= 12)) {
                if ((y & 1) == 0) write_bit(x, y, 1);
            }
        }
    }

    // 3. Inject Format Info (Mask 0, Level L) -> 0x2D33 pattern layout
    unsigned int format_register = 0x2D33; 
    for (x = 0; x < 8; ++x) {
        if (x != 6) write_bit(x, 8, (format_register >> x) & 1);
    }
    write_bit(8, 7, (format_register >> 8) & 1);
    write_bit(8, 8, (format_register >> 9) & 1);
    write_bit(7, 8, (format_register >> 10) & 1);
    for (y = 0; y < 6; ++y) {
        if (y != 6) write_bit(8, 5 - y, (format_register >> (11 + y)) & 1);
    }
    for (y = 0; y < 7; ++y) write_bit(14 + y, 8, (format_register >> y) & 1);
    for (x = 0; x < 7; ++x) write_bit(8, 20 - x, (format_register >> (7 + x)) & 1);
    write_bit(8, 13, 1); // Dark module anchor point

    // 4. Zigzag Matrix Generator
    y = 20; 
    for (col = 20; col > 0; col -= 2) {
        if (col == 6) col = 5; 
        while (1) {
            for (x = 0; x < 2; ++x) {
                unsigned char current_x = col - x;
                if (!is_fixed_zone(current_x, y)) {
                    if (main_ptr < 19 * 8) {
                        actual_byte = data_bytes[main_ptr / 8];
                    } else if (main_ptr < 26 * 8) {
                        actual_byte = ecc_bytes[(main_ptr - (19 * 8)) / 8];
                    } else {
                        actual_byte = 0; 
                    }
                    
                    current_bit = (actual_byte >> (7 - (main_ptr % 8))) & 1;
                    main_ptr++;

                    if ((current_x + y) % 2 == 0) {
                        current_bit ^= 1; 
                    }
                    write_bit(current_x, y, current_bit);
                }
            }
            if ((dir == -1 && y == 0) || (dir == 1 && y == 20)) {
                dir = -dir; 
                break;
            }
            y += dir;
        }
    }
}

int main(void) {
    const char* my_input = "HELLO WORLD"; 
    unsigned char len = strlen(my_input);
    unsigned char row, b_idx, bit_idx, current_byte;
    unsigned int ptr = 0;

    if (len > 25 || len == 0) return 1;
    if (!encode_string(my_input, len)) return 1;

    calculate_ecc();          
    generate_matrix();  

    printf("\n");
    for (row = 0; row < 21; ++row) {
        for (b_idx = 0; b_idx < 3; ++b_idx) {
            current_byte = bit_buffer[ptr++];
            for (bit_idx = 0; bit_idx < 8; ++bit_idx) {
                if ((b_idx * 8) + bit_idx >= 21) break;
                
                // Native black terminal theme rendering (1 = white module block)
                if (current_byte & 0x80) {
                    printf("██");
                } else {
                    printf("  ");
                }
                current_byte <<= 1;
            }
        }
        printf("\n"); 
    }
    printf("\n");
    return 0;
}
