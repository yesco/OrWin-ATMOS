// sim6502.c - abstracted ORIC ATMOS machine
//
// (c) 2026 Jonas S Karlsson (jsk@yesco.org)
//
// This is for the OrWIN-ATMOS project,
// to allow it to compile and run under a simulator.
//
// All ORIC specific "pokes" have been abstracted
// this this file and API and can be implemented
// for other platforms
// 
// This file targets to run under oscar64

#define GOTOXY

// Hmmm?

#define WRITE
size_t write(int fd, char* s, size_t count) {
  while(count-- >= 0) putchar(*s++);
}

#include <stdint.h>

#define ISLITERAL
//extern char __heap_start[];
extern char __heap_start;
#define HEAP_START ((uint16_t)__heap_start)
char isliteral(void* p) {
  return ((uint16_t)p > HEAP_START);
}

#define HEAPMEM

// no difference
//#pragma heapsize(0)

size_t heapfreex() {
  //char* p= malloc(1);
  return heapfree();
}
  
#define _heapmemavail heapfreex
#define _heapmaxavail heapfreex

#ifndef READSECTOR
#define FDC_STATUS   ((volatile uint8_t*)0x0310)
#define FDC_CMD      ((volatile uint8_t*)0x0310)
#define FDC_TRACK    ((volatile uint8_t*)0x0311)
#define FDC_SECTOR   ((volatile uint8_t*)0x0312)
#define FDC_DATA     ((volatile uint8_t*)0x0313)
#define FDC_CTRL     ((volatile uint8_t*)0x0314)

/* 
 * Reads a 512-byte physical sector from the Oric Microdisc
 * track:  0 - 79
 * sector: 1 - 17
 * side:   0 or 1
 * drive:  0 to 3 (usually 0 for Drive A:)
 */
uint8_t oric_read_sector(uint8_t track, uint8_t sector, uint8_t side, uint8_t drive, uint8_t *buffer) {
    // 1. Select the target Drive and Side via the Microdisc Control Register
    // Bit 0-1: Drive Select, Bit 4: Side Select, Bit 7: EPROM overlay disable (enable RAM mapping)
    *FDC_CTRL = (drive & 0x03) | ((side & 0x01) << 4) | 0x80;
    
    // 2. Set the target Track and Sector indices on the controller chip
    *FDC_TRACK  = track;
    *FDC_SECTOR = sector;
    
    // 3. Issue the FDC Read Command ($80 = Read Sector, Type II command)
    __asm("sei"); // CRITICAL: Disable interrupts so IRQs don't steal cycles mid-sector!
    *FDC_CMD = 0x80; 
    
    // 4. Tight high-speed assembly block loop to harvest 512 bytes
    // Using standard pointer registers passed from Oscar64 variables
    __asm(
        "ldx #0\n"               // Loop register for Page 1 (256 bytes)
        "ldy #0\n"               // Loop register for Page 2 (256 bytes)
        "lda %0\n"               // Load the buffer pointer low byte
        "sta $00\n"
        "lda %1\n"               // Load the buffer pointer high byte
        "sta $01\n"
        
".read_loop_p1:\n"
        "lda $0310\n"            // Read FDC status register
        "bmi .read_loop_p1\n"    // Wait while Bit 7 (Busy) is high
        "lda $0313\n"            // Pull raw data byte from FDC Data Register
        "sta ($00),y\n"          // Store into destination memory address
        "iny\n"
        "bne .read_loop_p1\n"    // Loop naturally rolls over after 256 bytes
        
        "inc $01\n"              // Increment memory pointer to the next 256-byte page block
".read_loop_p2:\n"
        "lda $0310\n"
        "bmi .read_loop_p2\n"
        "lda $0313\n"
        "sta ($00),y\n"
        "iny\n"
        "bne .read_loop_p2\n"
        : 
        : "r"((uint16_t)buffer & 0xFF), "r"((uint16_t)buffer >> 8)
        : "x", "y", "a"
    );
    __asm("cli"); // Re-enable scheduling interrupts safely
    
    // Verify execution status from FDC (0 = Success, non-zero = Read Error/CRC Fail)
    return *FDC_STATUS;
}
#endif // READSECTOR
