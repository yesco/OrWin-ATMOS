; SmallTable boot payload – offset 23 of T0S2
.setcpu "6502"
        sei
        cld
        ldx     #$FF
        txs
        lda     #0
        sta     $026A
        ldx     #0
        lda     #' '
clr:    sta     $BB80,x
        inx
        cpx     #40
        bne     clr
        lda     #'S'
        sta     $BB80
        lda     #'M'
        sta     $BB81
        lda     #'A'
        sta     $BB82
        lda     #'L'
        sta     $BB83
        lda     #'L'
        sta     $BB84
        lda     #'T'
        sta     $BB85
        lda     #'A'
        sta     $BB86
        lda     #'B'
        sta     $BB87
        lda     #'L'
        sta     $BB88
        lda     #'E'
        sta     $BB89
        lda     #' '
        sta     $BB8A
        lda     #'B'
        sta     $BB8B
        lda     #'O'
        sta     $BB8C
        lda     #'O'
        sta     $BB8D
        lda     #'T'
        sta     $BB8E
        lda     #' '
        sta     $BB8F
        lda     #'O'
        sta     $BB90
        lda     #'K'
        sta     $BB91
        sec
hang:   bcs     hang
