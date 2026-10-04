; =========================================================================
; ORIC ATMOS BARE-METAL BOOT LOADER (Track 0, Sector 1)
; Assembled as a raw 512-byte flat binary block
; =========================================================================
.org $B000

Entry:
    sei                 ; Inhibit baseline system vectors
    cld                 ; Clear decimal execution bounds
    ldx #$FF
    txs                 ; Reset Stack Pointer position

    ; Print simple loading message directly to Oric Screen memory map
    ; Screen text memory on the Oric starts right at $BB80
    ldx #0
.print_msg:
    lda BootMsg,x
    beq .load_system
    sta $BB80,x         ; Write straight to the top line of the terminal display
    inx
    bne .print_msg

.load_system:
    ; Configure the FDC to stream the rest of our application blocks
    lda #$80            ; Select Drive 0, Side 0
    sta $0314
    
    lda #0              ; Start loading from Track 0
    sta $0311
    lda #2              ; Sector 1 was the bootloader, so start streaming from Sector 2!
    sta $0312

    ; Initialize destination pointers for our main SmallTable app code
    ; Let's load the main program workspace starting at $1000
    lda #$00
    sta $00             ; Target Dest Low
    lda #$10
    sta $01             ; Target Dest High

    ldx #16             ; Let's load 16 continuous sectors (8 Kilobytes of application space)

.read_sector_loop:
    lda #$80            ; Trigger FDC Read Command
    sta $0310

    ldy #0
.p1_wait:
    lda $0310
    bmi .p1_wait        ; Spin on FDC busy flag
    lda $0313
    sta ($00),y         ; Push byte directly into Oric system RAM
    iny
    bne .p1_wait

    inc $01             ; Jump destination high-byte pointer forward
.p2_wait:
    lda $0310
    bmi .p2_wait
    lda $0313
    sta ($00),y
    iny
    bne .p2_wait
    inc $01             ; Finished 512-byte block pass

    ; Increment sector index to fetch next chunk
    inc $0312           ; Advance FDC Sector targeting
    dex
    bne .read_sector_loop

.boot_complete:
    cli                 ; Restore base interrupt systems
    jmp $1000           ; JUMP DIRECTLY INTO YOUR COMPILED APP ENTRY POINT!

BootMsg:
    .ascii "LOADING SMALLTABLE OS..."
    .byte 0

; Fill out the rest of the 512-byte boot sector payload with zeros
.fill 512 - (* - Entry), 0
