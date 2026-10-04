; =========================================================================
; VALID BARE-METAL BOOTABLE ORIC ATMOS DISK LAYOUT
; =========================================================================

.setcpu "6502"          ; Explicit target instruction set

; -------------------------------------------------------------------------
; SEGMENT 1: THE MAGIC ORIC DSK GEOMETRY HEADER (Exactly 256 Bytes)
; -------------------------------------------------------------------------
.segment "DSKHDR"
    .byte "MFM_DISK"     ; 8-Byte Format Signature
    .dword 2             ; Number of Sides (Double Sided)
    .dword 80            ; Number of Tracks (80 Cylinders)
    .dword 1             ; Geometry Type 1 (Tracks sequential by side)
    .res 232, 0          ; Pad out the rest of the 256-byte header with zeros

; -------------------------------------------------------------------------
; SEGMENT 2: THE BOOT SECTOR (Track 0, Sector 1 - Exactly 512 Bytes)
; Mapped to run at memory address $B000 at startup
; -------------------------------------------------------------------------
.segment "BOOTCODE"

Entry:
    sei                  ; CRITICAL: Must be the absolute 1st byte ($78) to pass verification!
    cld                  ; Clear decimal execution bounds
    ldx #$FF
    txs                  ; Reset Stack Pointer position

    ; Print simple loading message directly to Oric Screen memory map
    ; Screen text memory on the Oric starts right at $BB80
    ldx #0
print_msg:
    lda BootMsg,x
    beq load_system
    sta $BB80,x          ; Write straight to the top line of the display
    inx
    bne print_msg

load_system:
    ; Configure the FDC to stream the rest of our application blocks
    lda #$80             ; Select Drive 0, Side 0
    sta $0314
    
    lda #0               ; Start loading from Track 0
    sta $0311
    lda #2               ; Sector 1 was us! Start streaming from Sector 2
    sta $0312

    ; Initialize destination pointers for our main application workspace
    lda #$00
    sta $00              ; Target Dest Low ($00)
    lda #$10
    sta $01              ; Target Dest High ($10) -> Loading to $1000

    ldx #16              ; Load 16 continuous sectors (8 Kilobytes of app space)

read_sector_loop:
    lda #$80             ; Trigger FDC Read Command
    sta $0310

    ldy #0
p1_wait:
    lda $0310
    bmi p1_wait          ; Spin on FDC busy flag
    lda $0313
    sta ($00),y          ; Push byte directly into Oric system RAM
    iny
    bne p1_wait

    inc $01              ; Jump destination high-byte pointer forward
p2_wait:
    lda $0310
    bmi p2_wait
    lda $0313
    sta ($00),y
    iny
    bne p2_wait
    inc $01              ; Finished 512-byte block pass

    inc $0312            ; Advance FDC Sector targeting
    dex
    bne read_sector_loop

boot_complete:
    cli                  ; Restore base interrupt systems
    jmp $1000            ; JUMP DIRECTLY INTO YOUR MAIN PIPELINE SYSTEM AT $1000!

BootMsg:
    .asciiz "LOADING SMALLTABLE OS..." ; Automatically null-terminates string

; -------------------------------------------------------------------------
; PAD OUT SECTOR 1 TO EXACTLY 512 BYTES
; Calculate current size of BOOTCODE and pad to exactly 512 bytes
; -------------------------------------------------------------------------
.res 512 - (* - Entry), 0
