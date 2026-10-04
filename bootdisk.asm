; =========================================================================
; BARE-METAL BOOTABLE ORIC ATMOS DISK
; =========================================================================

.setcpu "6502"          

; -------------------------------------------------------------------------
; SEGMENT 1: THE CORRECT FORMAT SECTOR DUMP HEADER (Exactly 256 Bytes)
; -------------------------------------------------------------------------
.segment "DSKHDR"
    .byte "ORICDISK"     
    .dword 2             ; Number of Sides
    .dword 80            ; Number of Tracks
    .dword 17            ; Number of Sectors per Track
    .res 232, 0          ; Pad out the rest of the 256-byte header with zeros

; -------------------------------------------------------------------------
; SEGMENT 2: BARE-METAL BOOT DISK LOADER (Track 0, Sector 1 - Exactly 512 Bytes)
; Mapped to run at memory address $B000 at startup
; -------------------------------------------------------------------------
.segment "BOOTCODE"

Entry:
    sei                  ; Absolute 1st byte ($78) required by Microdisc ROM
    cld                  
    ldx #$FF
    txs                  

    ; Print simple loading message directly to Oric Screen memory map ($BB80)
    ldx #0
print_msg:
    lda BootMsg,x
    beq load_system
    sta $BB80,x          ; Write straight to the top line of the screen display
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

    ; POSITION REQUIREMENT: Load your pipeline program starting at 0x0500!
    lda #$00
    sta $00              ; Target Dest Low ($00)
    lda #$05
    sta $01              ; Target Dest High ($05) -> Target = 0x0500

    ldx #16              ; Load 16 continuous sectors (8 Kilobytes of space)

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
    jmp $0500            ; JUMP DIRECTLY INTO YOUR SMALLTABLE APP AT 0x0500!

BootMsg:
    .asciiz "LOADING SMALLTABLE OS TO 0x0500..."

; Pad Sector 1 out to exactly 512 bytes
.res 512 - (* - Entry), 0

; -------------------------------------------------------------------------
; SEGMENT 3: THE MANDATORY SYSTEM MAP SIGNATURE (Track 0, Sector 4)
; This forces the Microdisc controller ROM to pass the system validation check!
; -------------------------------------------------------------------------
.segment "SYSMAP"
    ; Sedoric / Microdisc Boot Signature pattern block
    .byte $00, $01, $00, $00, $53, $45, $44, $4F, $52, $49, $43, $20  ; "SEDORIC "
    .res 500, 0          ; Pad out Sector 4 to exactly 512 bytes

