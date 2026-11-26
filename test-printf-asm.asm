;;; test-printf-asm.asm - ivm-lib.asm's putf routines (printf's parts)
;;;
;;; Each case: a putf call and the C printf format it stands for, then
;;; "|" and a newline. test-printf-asm.out is what gcc's printf gives.
;;;
;;;   cl65 -t sim6502 -o test-printf-asm.sim test-printf-asm.asm ivm-lib.asm
;;;   sim65 test-printf-asm.sim | diff test-printf-asm.out -
;;;
;;; Tested:
;;;
;;; - u d x X s: width, -width (left), pad '0', .N, a value too wide
;;; - putfuz: the value, then the string after the JSR
;;; - the counter: putfn0 first, putfn at the end (as the sum of
;;;   printf's results)

        .import _opputfu, _opputfd, _opputfx, _opputfX, _opputfs
        .import _opputfz, _opputfuz, _opputfn, _opputfn0
        .import _ivmlib_pfpad, _ivmlib_pfprec
        .export _main

        .code

_main:
        jsr     _opputfn0               ;; the counter: 0

;;; %u  42
        lda     #<$002A
        ldx     #>$002A
        ldy     #<(0)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %5u  42
        lda     #<$002A
        ldx     #>$002A
        ldy     #<(5)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %-5u  42
        lda     #<$002A
        ldx     #>$002A
        ldy     #<(-5)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %05u  42
        lda     #'0'
        sta     _ivmlib_pfpad
        lda     #<$002A
        ldx     #>$002A
        ldy     #<(5)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %u  65535
        lda     #<$FFFF
        ldx     #>$FFFF
        ldy     #<(0)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %u  0
        lda     #<$0000
        ldx     #>$0000
        ldy     #<(0)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %.4u  42
        lda     #4
        sta     _ivmlib_pfprec
        lda     #<$002A
        ldx     #>$002A
        ldy     #<(0)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %6.4u  42
        lda     #4
        sta     _ivmlib_pfprec
        lda     #<$002A
        ldx     #>$002A
        ldy     #<(6)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %3u  12345
        lda     #<$3039
        ldx     #>$3039
        ldy     #<(3)
        jsr     _opputfu
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %5d  -42
        lda     #<$FFD6
        ldx     #>$FFD6
        ldy     #<(5)
        jsr     _opputfd
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %05d  -42
        lda     #'0'
        sta     _ivmlib_pfpad
        lda     #<$FFD6
        ldx     #>$FFD6
        ldy     #<(5)
        jsr     _opputfd
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %-5d  -42
        lda     #<$FFD6
        ldx     #>$FFD6
        ldy     #<(-5)
        jsr     _opputfd
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %d  1234
        lda     #<$04D2
        ldx     #>$04D2
        ldy     #<(0)
        jsr     _opputfd
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %d  -32768
        lda     #<$8000
        ldx     #>$8000
        ldy     #<(0)
        jsr     _opputfd
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %3d  0
        lda     #<$0000
        ldx     #>$0000
        ldy     #<(3)
        jsr     _opputfd
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %x  48879
        lda     #<$BEEF
        ldx     #>$BEEF
        ldy     #<(0)
        jsr     _opputfx
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %X  48879
        lda     #<$BEEF
        ldx     #>$BEEF
        ldy     #<(0)
        jsr     _opputfX
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %04x  255
        lda     #'0'
        sta     _ivmlib_pfpad
        lda     #<$00FF
        ldx     #>$00FF
        ldy     #<(4)
        jsr     _opputfx
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %x  0
        lda     #<$0000
        ldx     #>$0000
        ldy     #<(0)
        jsr     _opputfx
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %-3X  10
        lda     #<$000A
        ldx     #>$000A
        ldy     #<(-3)
        jsr     _opputfX
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %8s  "hello"
        lda     #<s20
        ldx     #>s20
        ldy     #<(8)
        jsr     _opputfs
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %-8s  "hello"
        lda     #<s21
        ldx     #>s21
        ldy     #<(-8)
        jsr     _opputfs
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %.3s  "hello"
        lda     #3
        sta     _ivmlib_pfprec
        lda     #<s22
        ldx     #>s22
        ldy     #<(0)
        jsr     _opputfs
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %5.2s  "hello"
        lda     #2
        sta     _ivmlib_pfprec
        lda     #<s23
        ldx     #>s23
        ldy     #<(5)
        jsr     _opputfs
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %2s  ""
        lda     #<s24
        ldx     #>s24
        ldy     #<(2)
        jsr     _opputfs
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %2s  "hello"
        lda     #<s25
        ldx     #>s25
        ldy     #<(2)
        jsr     _opputfs
        jsr     _opputfz
        .byte   "|", 10, 0

;;; %u seven  7: the value, then the string after the JSR
        lda     #7
        ldx     #0
        ldy     #0
        jsr     _opputfuz
        .byte   " seven", 10, 0

;;; the counter: the chars printed so far
        jsr     _opputfn
        ldy     #0
        jsr     _opputfu
        jsr     _opputfz
        .byte   " chars", 10, 0

        lda     #0
        tax
        rts

;;; the strings

s20:     .asciiz "hello"
s21:     .asciiz "hello"
s22:     .asciiz "hello"
s23:     .asciiz "hello"
s24:     .asciiz ""
s25:     .asciiz "hello"
