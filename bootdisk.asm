; =============================================================================
; SmallTable Microdisc boot disk  (ca65 + ld65 → ORICDISK)
;
; Track 0:
;    Sector 1 : signature
;    Sector 2 : BOOT
;    Sector 3 : SYSTEMDOS
;    Sector 4 : free
;    Sector 5 : SmallTable Directory
;    Sector 6+: program.bin loaded at LOAD_ADDR and called
; 	
; program.bin is injected at S6 by build.sh and loaded to $0500 by this boot.
; =============================================================================

.setcpu "6502"

;;; LOAD_SECTORS is set when building the disk doing ca65
;;; in ./bootdisk
	
.ifndef LOAD_SECTORS
LOAD_SECTORS = 1
.endif


LOAD_ADDR         = $0500

FIRST_DATA_SECTOR = 6
SECTORS_PER_TRACK = 17

FDC_CMD   = $0310
FDC_TRACK = $0311
FDC_SECT  = $0312
FDC_DATA  = $0313
FDC_CTRL  = $0314
FDC_DRQ   = $0318

;;; ---------------------------------------------------------------------------
;;; Zero Page usage
	
;;; $00, $01: load_addr start and counter
	
;;; track the current track
track     = $02

;;; ---------------------------------------------------------------------------
;;; Sector 1 : signature
	
.segment "DSKHDR"
        .byte   "ORICDISK"
        .dword  2
        .dword  80
        .dword  17
        .res    232, 0

;;; ---------------------------------------------------------------------------
;;; Sector 2 : BOOT

.segment "SEC1"
        .byte   $01,$00,$00,$00,$00,$00,$00,$00
        .byte   $20,$20,$20,$20,$20,$20,$20,$20
        .byte   $00,$00,$03,$00,$00,$00,$01,$00
        .byte   $53,$45,$44,$4F,$52,$49,$43,$20
        .res    40, $20
        .byte   "SEDORIC V3.006 01/01/96"
        .res    161, 0

;;; ---------------------------------------------------------------------------
;;; Sector 2 : BOOT
	
.segment "SEC2"
        ; SEDORIC prefix – required by Microdisc ROM
        .byte   $00,$00,$FF,$00,$D0,$9F,$D0,$9F
        .byte   $02,$B9,$01,$00,$FF,$00,$00,$B9
        .byte   $E4,$B9,$00,$00,$E6,$12,$00

;; TODO: Not clear where this code is loaded and executed

Entry:
	;; disable interrupts, not decimal, init stack
        sei
        cld
        ldx     #$FF
        txs

	;; ORIC: cursor blink off, no keyclick etc
        lda     #0
        sta     $026A

	;; ORIC: clears status line
        ldx     #0
        lda     #' '
@c:     sta     $BB80,x
        inx
        cpx     #40
        bne     @c

	;; ORIC: print loading message "ST LOAD"
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

	;; FDC drive 0 side 0 DD
	;; ORIC: ROMDIS=0 (unmaps ROM/BASIC making it RAM)
        lda     #%10000100
        sta     FDC_CTRL

        ;; Move head to track 0
        lda     #$0C
        sta     FDC_CMD
@wr:    lda     FDC_CMD
        and     #1
        bne     @wr

	;; load dest pointer start pointer
        lda     #<LOAD_ADDR
        sta     $00
        lda     #>LOAD_ADDR
        sta     $01

	;; sector / track / count
        ldx     #FIRST_DATA_SECTOR  ; X = sector
        lda     #0
        sta     track
        ldy     #LOAD_SECTORS       ; Y = remaining

	;; Track 2: load next of 256 bytes at ($00)
	;; from;
	;;   track == track to load
	;;   X     == sector
	;;   Y     == sector counting down
@next:
        lda     track
        sta     FDC_TRACK
        stx     FDC_SECT
        lda     #$88		; set destination
        sta     FDC_CMD

	;;  prepare to read one sector
        tya
        pha
        ldy     #0

	;; wait for data ready
@rd:    lda     FDC_DRQ
        bmi     @rd

	;; read byte, stuff it
        lda     FDC_DATA
        sta     ($00),y
        iny

        bne     @rd

	;; done with 256 bytes (Y wrapped)
        inc     $01

        pla
        tay

	;; move to next track?
        inx
        cpx     #SECTORS_PER_TRACK+1
        bcc     @sametrack
        ldx     #1
        inc     track
@sametrack:
	;; more pages to load?
        dey
        bne     @next

	;; All LOAD_SECTORS loaded
	;; Jump to it!
        jmp     LOAD_ADDR


	;; ^- about 159 bytes!



;;; ---------------------------------------------------------------------------
;;; Sector 3 : SYSTEMDOS
;;;   - fake directory data to make boot happy
	
.segment "SEC3"
        .byte   $00,$00,$02
        .byte   "SYSTEMDOS"
        .byte   $01,$00,$02,$00,$02,$00,$00
        .byte   "BOOTUPCOM"
        .res    228, 0

;;; ---------------------------------------------------------------------------
;;; Sector 4 : free - reserved

.segment "SEC4"
        .res    256, 0

;;; ---------------------------------------------------------------------------
;;; Sector 5 : SmallTable Directory

.segment "SEC5"
        .res    256, 0
