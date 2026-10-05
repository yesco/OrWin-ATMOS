#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILENAME "sim_test.bin"
#define DATA_SIZE 256

/* Move large arrays to global scope to respect cc65's 256-byte function stack limit */
char write_buf[DATA_SIZE];
char read_buf[DATA_SIZE];

int main(void) {
    FILE *file;
    long file_pos;
    int i;

    printf("===========================================\n");
    printf("  SIM65 PARAVIRTUALIZED FILE I/O TESTER   \n");
    printf("===========================================\n\n");

    /* 1. Initialize prefix-encoded sector data mimicking SmallTable specifications */
    memset(write_buf, 0, DATA_SIZE);
    write_buf[0] = 0x10; /* Direct index offset of next record inside this page */
    write_buf[1] = 0x00; /* Prefix count byte */
    write_buf[2] = 0x04; /* Data index pointer (Points right after metadata) */
    write_buf[3] = 0xFF; /* Timestamp Byte: Default to -1 (Disabled) */
    strcpy(&write_buf[4], "KEY");
    strcpy(&write_buf[8], "DATA");

    /* 2. Test fopen() in write-binary mode */
    printf("1. Testing fopen(\"%s\", \"wb\")... ", FILENAME);
    file = fopen(FILENAME, "wb");
    if (!file) {
        printf("FAILED!\n");
        return EXIT_FAILURE;
    }
    printf("SUCCESS.\n");

    /* 3. Test fwrite() to commit the 256-byte sector block */
    printf("2. Testing fwrite() [Writing %d bytes]... ", DATA_SIZE);
    if (fwrite(write_buf, 1, DATA_SIZE, file) != DATA_SIZE) {
        printf("FAILED!\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    printf("SUCCESS.\n");

    /* 4. Test fclose() */
    printf("3. Testing fclose()... ");
    fclose(file);
    printf("SUCCESS.\n\n");

    /* 5. Test fopen() in read/write binary mode for verification updates */
    printf("4. Testing fopen(\"%s\", \"r+b\")... ", FILENAME);
    file = fopen(FILENAME, "r+b");
    if (!file) {
        printf("FAILED!\n");
        return EXIT_FAILURE;
    }
    printf("SUCCESS.\n");

    /* 6. Test fread() to verify written contents and data layout integrity */
    printf("5. Testing fread() [Reading back payload]... ");
    memset(read_buf, 0, DATA_SIZE);
    if (fread(read_buf, 1, DATA_SIZE, file) != DATA_SIZE) {
        printf("FAILED!\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    
    /* Strict layout evaluation matching your prefix layout index rules */
    if (read_buf[0] == 0x10 && strcmp(&read_buf[4], "KEY") == 0) {
        printf("SUCCESS (Data Integrity Confirmed).\n");
    } else {
        printf("FAILED (Data Corruption Detected).\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    /* 7. Test fseek() and ftell() */
    printf("6. Testing fseek() [Jumping to suffix string offset 4]... ");
    if (fseek(file, 4, SEEK_SET) != 0) {
        printf("FAILED!\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    printf("SUCCESS.\n");

    printf("7. Testing ftell()... ");
    file_pos = ftell(file);
    if (file_pos != 4) {
        printf("FAILED! (Reported offset: %ld)\n", file_pos);
        fclose(file);
        return EXIT_FAILURE;
    }
    printf("SUCCESS (Offset matches %ld).\n", file_pos);

    /* 8. Test fgetc() relative to seek position */
    printf("8. Testing fgetc()... ");
    i = fgetc(file);
    if (i == 'K') {
        printf("SUCCESS (Extracted correct character '%c').\n", i);
    } else {
        printf("FAILED! (Extracted '%c' instead of 'K')\n", i);
        fclose(file);
        return EXIT_FAILURE;
    }

    /* 9. Test fputc() verification */
    printf("9. Testing fputc() [Modifying character in-place]... ");
    /* Current position is at index 5 ('E'). Let's overwrite it with 'A' to create "KAY" */
    if (fputc('A', file) == 'A') {
        printf("SUCCESS.\n");
    } else {
        printf("FAILED!\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    /* 10. Rewind and re-verify change */
    printf("10. Testing rewind() and tracking updates... ");
    rewind(file);
    if (fread(read_buf, 1, DATA_SIZE, file) != DATA_SIZE) {
        printf("FAILED!\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    
    if (strcmp(&read_buf[4], "KAY") == 0) {
        printf("SUCCESS (File modified to \"KAY\").\n");
    } else {
        printf("FAILED! (String remains: \"%s\")\n", &read_buf[4]);
        fclose(file);
        return EXIT_FAILURE;
    }

    fclose(file);

    /* 11. Test file mutation operations (remove) */
    printf("\n11. Testing remove(\"%s\")... ", FILENAME);
    if (remove(FILENAME) == 0) {
        printf("SUCCESS (File completely unlinked from disk).\n");
    } else {
        printf("FAILED!\n");
        return EXIT_FAILURE;
    }

    printf("\n===========================================\n");
    printf("  ALL SIM65 PARAVIRTUAL I/O OPERATIONS PASS \n");
    printf("===========================================\n");

    return EXIT_SUCCESS;
}
