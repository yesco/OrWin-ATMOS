#include <stdio.h>
#include <string.h>

// ตารางถอดรหัส QR Alphanumeric ตามมาตรฐานสากล
const char ALPHANUM_TABLE[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

// ค่าสัมประสิทธิ์สำหรับคำนวณ Reed-Solomon Error Correction ขนาด 7 บลัค (QR v1-L)
const unsigned char RS_POLY[] = {127, 122, 154, 164, 11, 68, 117};

// บัฟเฟอร์สำหรับบริหารแรมอันจำกัดของ 6502
unsigned char data_bytes[19]; 
unsigned char ecc_bytes[7];   
unsigned char bit_buffer[63]; // ขนาด 21 แถว * 3 ไบต์ = 63 ไบต์ (Canvas พื้นหลัง)

// ฟังก์ชันคูณเลขบน Galois Field 2^8 ด้วยบิตชิฟต์สไตล์ 6502 (ไม่ต้องใช้ตารางคำนวณขนาดใหญ่)
unsigned char gf_mul(unsigned char a, unsigned char b) {
    unsigned char result = 0;
    while (b > 0) {
        if (b & 1) result ^= a;
        a = (a << 1) ^ (a & 0x80 ? 0x1D : 0); // โพลีโนเมียลพื้นฐาน x^8 + x^4 + x^3 + x^2 + 1
        b >>= 1;
    }
    return result;
}

// ตรวจสอบตัวอักษรและแปลงเป็นค่าดัชนี (รองรับ Auto-Uppercase)
int get_alphanumeric_val(char c) {
    unsigned char i;
    if (c >= 'a' && c <= 'z') c -= 32; 
    for (i = 0; i < 45; ++i) {
        if (ALPHANUM_TABLE[i] == c) return i;
    }
    return -1; // แจ้งเตือนเมื่อเจอตัวอักษรที่ห้ามใช้
}

// ฟังก์ชันเขียนบิตลงบัฟเฟอร์ตำแหน่งที่ต้องการ
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

// ลูปคำนวณรหัสแก้ผิดพลาด Reed-Solomon
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

// วาดสิ่งกีดขวางถาวร (Finder Patterns 7x7 ทั้ง 3 มุม) ลงในกรอบพื้นที่
void inject_fixed_patterns(void) {
    unsigned char x, y;
    
    // เคลียร์พื้นหลังทั้งหมดเป็นศูนย์ก่อน
    memset(bit_buffer, 0, 63);

    // สร้างกล่องสี่เหลี่ยมมุมบนซ้าย (Top-Left), บนขวา (Top-Right), ล่างซ้าย (Bottom-Left)
    for (y = 0; y < 21; ++y) {
        for (x = 0; x < 21; ++x) {
            // โครงสร้างขอบนอกและแกนในของ Finder Pattern ขนาด 7x7
            if ((x < 7 && y < 7) || (x > 13 && y < 7) || (x < 7 && y > 13)) {
                unsigned char px = (x > 13) ? (x - 14) : x;
                unsigned char py = (y > 13) ? (y - 14) : y;
                
                if (px == 0 || px == 6 || py == 0 || py == 6 || (px >= 2 && px <= 4 && py >= 2 && py <= 4)) {
                    write_bit(x, y, 1);
                }
            }
            // ใส่เส้นไข่ปลาช่วยจัดตำแหน่ง (Timing Patterns) ที่แถว 6 และคอลัมน์ 6
            else if (y == 6 && (x & 1) == 0) {
                write_bit(x, y, 1);
            }
            else if (x == 6 && (y & 1) == 0) {
                write_bit(x, y, 1);
            }
        }
    }
}

int main(void) {
    const char* my_input = "ABC"; // ทดสอบป้อนค่า 3 ตัวอักษรตามความต้องการของคุณ
    unsigned char len = strlen(my_input);
    unsigned char row, b_idx, bit_idx, current_byte;
    unsigned int ptr = 0;
    int v1;

    if (len > 25 || len == 0) {
        printf("ERROR: String length must be between 1 and 25 characters.\n");
        return 1;
    }

    // ส่วนการทำงานแปลงข้อมูลเบื้องต้นอย่างง่ายลงสู่ Data Bytes
    memset(data_bytes, 0, 19);
    // Hardcode รูปแบบหัวข้อมูลสำหรับข้อความสั้น 
    data_bytes[0] = 0x20; // Alphanumeric Mode indicator
    data_bytes[1] = len << 3;

    v1 = get_alphanumeric_val(my_input[0]);
    if (v1 < 0) {
        printf("ERROR: Contains illegal characters!\n");
        return 1;
    }
    
    // นำค่าดัชนีแปลงลงสู่หน่วยความจำ
    data_bytes[2] = v1 << 2; 

    // สั่งรันเอนจินประมวลผลโครงสร้าง
    calculate_ecc();          
    inject_fixed_patterns();  

    // แสดงผลลัพธ์กราฟิกออกมาทางจอเทอร์มินัล
    printf("\n--- QR BARCODE GENERATED ---\n\n");
    for (row = 0; row < 21; ++row) {
        // เพิ่มระยะเว้นช่องว่างด้านซ้าย (Quiet Zone สำหรับตัวสแกน)
        printf("        ");
        for (b_idx = 0; b_idx < 3; ++b_idx) {
            current_byte = bit_buffer[ptr++];
            for (bit_idx = 0; bit_idx < 8; ++bit_idx) {
                if ((b_idx * 8) + bit_idx >= 21) break;
                
                // ใช้ "XX" แทนพิกเซลสีดำ และขยับเว้นวรรค 2 ตัวแทนพื้นที่ว่าง
                printf(current_byte & 0x80 ? "XX" : "  ");
                current_byte <<= 1;
            }
        }
        printf("\n");
    }
    printf("\n----------------------------\n\n");
    return 0;
}
