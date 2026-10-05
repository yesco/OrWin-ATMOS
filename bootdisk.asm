; =============================================================================
; SmallTable Microdisc boot disk  (ca65 + ld65 → ORICDISK)
;
; Track 0: S1 signature, S2 boot, S3 SYSTEMDOS, S4 free, S5 dir, S6+ program
; program.bin is injected at S6 by build.sh and loaded to $0500 by this boot.
; =============================================================================

.setcpu "6502"

.ifndef LOAD_SECTORS
LOAD_SECTORS = 1
.endif

FIRST_DATA_SECTOR = 6
LOAD_ADDR         = $0500
SPT               = 17

FDC_CMD   = $0310
FDC_TRACK = $0311
FDC_SECT  = $0312
FDC_DATA  = $0313
FDC_CTRL  = $0314
FDC_DRQ   = $0318

; ---------------------------------------------------------------------------
.segment "DSKHDR"
        .byte   "ORICDISK"
        .dword  2
        .dword  80
        .dword  17
        .res    232, 0

; ---------------------------------------------------------------------------
.segment "SEC1"
        .byte   $01,$00,$00,$00,$00,$00,$00,$00
        .byte   $20,$20,$20,$20,$20,$20,$20,$20
        .byte   $00,$00,$03,$00,$00,$00,$01,$00
        .byte   $53,$45,$44,$4F,$52,$49,$43,$20
        .res    40, $20
        .byte   "SEDORIC V3.006 01/01/96"
        .res    161, 0

; ---------------------------------------------------------------------------
.segment "SEC2"
        ; SEDORIC prefix – required by Microdisc ROM
        .byte   $00,$00,$FF,$00,$D0,$9F,$D0,$9F
        .byte   $02,$B9,$01,$00,$FF,$00,$00,$B9
        .byte   $E4,$B9,$00,$00,$E6,$12,$00

Entry:
        sei
        cld
        ldx     #$FF
        txs
        lda     #0
        sta     $026A

        ldx     #0
        lda     #' '
@c:     sta     $BB80,x
        inx
        cpx     #40
        bne     @c

        lda     #'S'
        sta     $BB80
        lda     #'T'
        sta     $BB81
        lda     #' '
        sta     $BB82
        lda     #'L'
        sta     $BB83
        lda     #'O'
        sta     $BB84
        lda     #'A'
        sta     $BB85
        lda     #'D'
        sta     $BB86

        ; FDC drive 0 side 0 DD ; ROMDIS=0
        lda     #%10000100
        sta     FDC_CTRL

        ; Restore track 0
        lda     #$0C
        sta     FDC_CMD
@wr:    lda     FDC_CMD
        and     #1
        bne     @wr

        ; dest pointer
        lda     #<LOAD_ADDR
        sta     $00
        lda     #>LOAD_ADDR
        sta     $01

        ; sector / track / count
        ldx     #FIRST_DATA_SECTOR  ; X = sector
        lda     #0
        sta     $02                 ; $02 = track
        ldy     #LOAD_SECTORS       ; Y = remaining

@next:
        lda     $02
        sta     FDC_TRACK
        stx     FDC_SECT
        lda     #$88
        sta     FDC_CMD

        tya
        pha
        ldy     #0
@rd:    lda     FDC_DRQ
        bmi     @rd
        lda     FDC_DATA
        sta     ($00),y
        iny
        bne     @rd
        inc     $01

        pla
        tay

        inx
        cpx     #SPT+1
        bcc     @same
        ldx     #1
        inc     $02
@same:
        dey
        bne     @next

        jmp     LOAD_ADDR


; ---------------------------------------------------------------------------
.segment "SEC3"
        .byte   $00,$00,$02
        .byte   "SYSTEMDOS"
        .byte   $01,$00,$02,$00,$02,$00,$00
        .byte   "BOOTUPCOM"
        .res    228, 0

.segment "SEC4"
        .res    256, 0

.segment "SEC5"
        .res    256, 0
