;;; ivm-fixed.asm — 6502 IVM with fixed absolute mem planes (ALTCODE=1 only)
;;;
;;; ASSUMPTION: the lo/hi memory planes live at fixed absolute addresses.
;;; We therefore use absolute,X addressing for all slot accesses.
;;; This leaves Y completely free to stay as the IPC offset counter,
;;; eliminating almost all USEY / savey / ynext juggling.
;;;
;;; These resolve at link time to the fixed slice arrays in ivm.c:
MEM_LO_BASE = _slice_lo
MEM_HI_BASE = _slice_hi

;;; Everything else is as in the original ALTCODE=1 path.

;TRACE=1
TRACE=0

.export         _run_ivm_asm
.export         _IVM_ASM_START
.export         _IVM_ASM_END

.import         _slice_lo, _slice_hi
.importzp       _ivm_ipc, _ivm_fp, _ivm_acc, _ivm_op
.importzp       _ivm_par
.import         pushax, _memmove, _strlen
.import         _strcmp, _strncmp, _memcmp, _strchr, _memchr, _strcpy, _strncpy
.importzp       _ivm_fptab, _ivm_callv

.import         _opmul, _opdiv, _opmod, _opputnum
.import         _putchar, _getchar
.import         _ivm_zalloc, _ivm_free

FR_RET  = 0
FR_VAL  = 1
FR_PREV = 2
FR_ACC  = 3
FP_MAIN = $FF

.data

.zeropage
jmpvec: .res 2
tmp_lo: .res 1
tmp_hi: .res 1
swpb:   .res 1
savey:  .res 1          ; still used by a few multi-byte / flow ops

.code
.export _IVM_ASM_START
_IVM_ASM_START:

;;; ============================================================
;;; ALTCODE=1 IPC offset in Y  (kept from original)
;;; ============================================================

.macro FIXIPC
        tya
        clc
        adc     _ivm_ipc
        sta     _ivm_ipc
        bcc     :+
        inc     _ivm_ipc+1
:       ldy     #0
.endmacro

_run_ivm_asm:
        ldy     #0
        beq     xnext

;;; advance by A (used by some skips)
anext:
        clc
        adc     savey
        tay
        bcc     pnext_no_load   ; N from tay
        inc     _ivm_ipc+1
        tya
        jmp     pnext_no_load

;;; Y overflow rule: only ops WITH operands check Y (bit 7 -> FIXIPC).
;;; No-param ops just Y+1 and jmp xnext: Y stays < 256 unless ~125 of
;;; them run back to back (they're slot loads etc: operand ops follow).

;;; Operand op, Y already at the next opcode: check
.macro PNEXT_HAVEY
        tya
        jmp     pnext_no_load
.endmacro

;;; No-param op: no check
.macro NEXT
        jmp     xnext
.endmacro

;;; savey = offset of the NEXT opcode, no-param op (after a jsr)
ynext:
        ldy     savey
        jmp     xnext

;;; savey = offset of LAST consumed byte (operand); continue after it
pnext:
        ldy     savey
        iny
pnext_no_load:                  ; entry: Y = next opcode offset, N = Y's bit 7
        bmi     gofix

xnext:
        lda     (_ivm_ipc),y

        .if TRACE
        sta     _ivm_op
        sty     savey
        lda     #'>'
        jsr     _putchar
        lda     _ivm_op
        jsr     _putchar
        ldy     savey
        lda     _ivm_op
        .endif

        iny
        tax                     ; X = opcode (and for 0..31 the slot!)
        bmi     HIBIT
        asl
        sta     pc+1
pc:     jmp     (dispvec)

;;; Y >= 128: fold into ipc
gofix:
        FIXIPC
        jmp     xnext

;;; ----- HIBIT -----
HIBIT:
        asl     a
        bmi     subvec

;;; 0x80..0xBF relative if ACC==0  (Y-relative: no FIXIPC)
rjp:
        lda     _ivm_acc
        ora     _ivm_acc+1
        bne     rjp_no
        txa
        and     #$3F
        sty     savey
        clc
        adc     savey               ; target = Y (after opcode) + off
        tay
        bcc     pnext_no_load       ; N from tay
        inc     _ivm_ipc+1
        tya
        jmp     pnext_no_load
rjp_no: NEXT

subvec:
        FIXIPC
        txa
        and     #$3F
        tay
        lda     (_ivm_fptab),y
        sta     tmp_lo          ; nfp

        tay
        lda     _ivm_ipc
        sta     MEM_LO_BASE,y
        lda     _ivm_ipc+1
        sta     MEM_HI_BASE,y

        iny                     ; skip FR_VAL
        iny                     ; FR_PREV
        lda     _ivm_fp
        sta     MEM_LO_BASE,y
        lda     #0
        sta     MEM_HI_BASE,y

        iny                     ; FR_ACC
        lda     _ivm_acc
        sta     MEM_LO_BASE,y
        lda     _ivm_acc+1
        sta     MEM_HI_BASE,y

        lda     tmp_lo
        sta     _ivm_fp

        txa
        and     #$3F
        asl
        tay
        lda     (_ivm_callv),y
        sta     _ivm_ipc
        iny
        lda     (_ivm_callv),y
        sta     _ivm_ipc+1

        ldy     #0
        NEXT

;;; ============================================================
;;; Rewritten handlers — memory via absolute,X  (Y untouched)
;;; ============================================================

;;; 0..31: load slot=opcode → ACC.  X already holds the slot!
op_31:
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        NEXT

op_q:   jmp     done

op_Q:
        lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1
        jmp     op_X

op_semi:
        lda     _ivm_fp
        clc
        adc     #FR_VAL
        tax
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        ; fall into op_X

op_X:   FIXIPC
        ldx     _ivm_fp
        lda     MEM_LO_BASE,x
        sta     _ivm_ipc
        lda     MEM_HI_BASE,x
        sta     _ivm_ipc+1
        inx
        inx                     ; FR_PREV
        lda     MEM_LO_BASE,x
        sta     _ivm_fp
        ldy     #0
        NEXT

;;; arg 8
op_8:
        lda     _ivm_fp
        clc
        adc     #FR_ACC
        tax
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        NEXT

;;; args 1..7
op_1to7:
        txa
        sec
        sbc     #'0'
        clc
        adc     #FR_ACC
        adc     _ivm_fp
        tax
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        NEXT

;;; -xx   ACC -= mem[xx]
op_sub:
        lda     (_ivm_ipc),y    ; slot
        tax
        iny
        sec
        lda     _ivm_acc
        sbc     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     _ivm_acc+1
        sbc     MEM_HI_BASE,x
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; +xx
op_add:
        lda     (_ivm_ipc),y
        tax
        iny
        clc
        lda     _ivm_acc
        adc     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     _ivm_acc+1
        adc     MEM_HI_BASE,x
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; Lxx   ACC = mem[xx]
op_L:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; Sxx   mem[xx] = ACC
op_S:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     _ivm_acc
        sta     MEM_LO_BASE,x
        lda     _ivm_acc+1
        sta     MEM_HI_BASE,x
        PNEXT_HAVEY

;;; Zxx   mem[xx] = 0
op_Z:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     #0
        sta     MEM_LO_BASE,x
        sta     MEM_HI_BASE,x
        PNEXT_HAVEY

;;; Yxx   mem[xx] = 1
op_Y:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     #1
        sta     MEM_LO_BASE,x
        lda     #0
        sta     MEM_HI_BASE,x
        PNEXT_HAVEY

;;; C dest src
op_C:
        lda     (_ivm_ipc),y
        sta     tmp_lo          ; dest
        iny
        lda     (_ivm_ipc),y
        tax                     ; src
        iny
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        ldx     tmp_lo
        lda     _ivm_acc
        sta     MEM_LO_BASE,x
        lda     _ivm_acc+1
        sta     MEM_HI_BASE,x
        PNEXT_HAVEY

;;; = < > compares (unsigned)
op_eq:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        cmp     _ivm_acc
        bne     op_acc0p
        lda     MEM_HI_BASE,x
        cmp     _ivm_acc+1
        bne     op_acc0p
        beq     op_acc1p

op_lt:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     _ivm_acc
        cmp     MEM_LO_BASE,x
        lda     _ivm_acc+1
        sbc     MEM_HI_BASE,x
        bcc     op_acc1p
        bcs     op_acc0p

op_gt:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        cmp     _ivm_acc
        lda     MEM_HI_BASE,x
        sbc     _ivm_acc+1
        bcc     op_acc1p
        bcs     op_acc0p

op_9:
        lda     #$FF
        sta     _ivm_acc
        sta     _ivm_acc+1
        NEXT

op_not:
        lda     _ivm_acc
        ora     _ivm_acc+1
        beq     op_acc1
op_0:
        lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1
        NEXT

op_acc1:
        lda     #1
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        NEXT

op_acc0p:
        lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1
        PNEXT_HAVEY

op_acc1p:
        lda     #1
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; Ixx  ACC = ++mem[xx]
op_I:
        lda     (_ivm_ipc),y
        tax
        iny
        clc
        lda     MEM_LO_BASE,x
        adc     #1
        sta     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        adc     #0
        sta     MEM_HI_BASE,x
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; ============================================================
;;; Remaining operators (fixed absolute,X style)
;;; ============================================================

;;; NOP: the table points straight at xnext
op_sp   = xnext

;;; * / %  — PAR in _ivm_par, then C helper

;;; @xx  ACC = *(word*)mem[xx]
op_at:
        sty     savey               ; Y at operand; pnext skips it
        lda     (_ivm_ipc),y
        tax
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        iny
        lda     (tmp_lo),y
        sta     _ivm_acc+1
        jmp     pnext

;;; !xx  *(word*)mem[xx] = ACC
op_bang:
        sty     savey
        lda     (_ivm_ipc),y
        tax
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        ldy     #0
        lda     _ivm_acc
        sta     (tmp_lo),y
        iny
        lda     _ivm_acc+1
        sta     (tmp_lo),y
        jmp     pnext

;;; ( aa ii   ACC = base[index]  (word elements, index *= 2)
op_idxl:
        lda     (_ivm_ipc),y        ; aa
        sta     swpb
        iny
        sty     savey               ; Y at ii; pnext skips it
        lda     (_ivm_ipc),y        ; ii
        tax
        lda     MEM_LO_BASE,x
        asl
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        rol
        sta     tmp_hi
        ldx     swpb
        clc
        lda     MEM_LO_BASE,x
        adc     tmp_lo
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        adc     tmp_hi
        sta     tmp_hi
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        iny
        lda     (tmp_lo),y
        sta     _ivm_acc+1
        jmp     pnext

;;; ) aa ii   base[index] = ACC
op_idxs:
        lda     (_ivm_ipc),y
        sta     swpb
        iny
        sty     savey
        lda     (_ivm_ipc),y
        tax
        lda     MEM_LO_BASE,x
        asl
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        rol
        sta     tmp_hi
        ldx     swpb
        clc
        lda     MEM_LO_BASE,x
        adc     tmp_lo
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        adc     tmp_hi
        sta     tmp_hi
        ldy     #0
        lda     _ivm_acc
        sta     (tmp_lo),y
        iny
        lda     _ivm_acc+1
        sta     (tmp_lo),y
        jmp     pnext

;;; [ aa ii   ACC = ((uchar*)mem[aa])[mem[ii]]
;;; self-modified lda abs,x: Y untouched
op_bidxl:
        lda     (_ivm_ipc),y        ; aa
        tax
        iny
        lda     MEM_LO_BASE,x
        sta     bidxl_smc+1
        lda     MEM_HI_BASE,x
        sta     bidxl_smc+2
        lda     (_ivm_ipc),y        ; ii
        tax
        iny
        clc
        lda     MEM_HI_BASE,x
        adc     bidxl_smc+2
        sta     bidxl_smc+2
        lda     MEM_LO_BASE,x
        tax
bidxl_smc:
        lda     $FFFF,x
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; ] aa ii   ((uchar*)mem[aa])[mem[ii]] = lo(ACC)
op_bidxs:
        lda     (_ivm_ipc),y        ; aa
        tax
        iny
        lda     MEM_LO_BASE,x
        sta     bidxs_smc+1
        lda     MEM_HI_BASE,x
        sta     bidxs_smc+2
        lda     (_ivm_ipc),y        ; ii
        tax
        iny
        clc
        lda     MEM_HI_BASE,x
        adc     bidxs_smc+2
        sta     bidxs_smc+2
        lda     MEM_LO_BASE,x
        tax
        lda     _ivm_acc
bidxs_smc:
        sta     $FFFF,x
        PNEXT_HAVEY

;;; 'xx  ACC = *(uchar*)ptr; ptr++   (ptr in slot xx)
op_ri:
        sty     savey
        lda     (_ivm_ipc),y
        tax
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        ; ptr++
        inc     tmp_lo
        bne     :+
        inc     tmp_hi
:       lda     tmp_lo
        sta     MEM_LO_BASE,x
        lda     tmp_hi
        sta     MEM_HI_BASE,x
        jmp     pnext

;;; ,xx  *(uchar*)ptr = ACC; ptr++
op_wi:
        sty     savey
        lda     (_ivm_ipc),y
        tax
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        ldy     #0
        lda     _ivm_acc
        sta     (tmp_lo),y
        inc     tmp_lo
        bne     :+
        inc     tmp_hi
:       lda     tmp_lo
        sta     MEM_LO_BASE,x
        lda     tmp_hi
        sta     MEM_HI_BASE,x
        jmp     pnext

op_mul:
        lda     (_ivm_ipc),y
        tax
        sty     savey               ; Y at operand; pnext skips it
        lda     MEM_LO_BASE,x
        sta     _ivm_par
        lda     MEM_HI_BASE,x
        sta     _ivm_par+1
        jsr     _opmul
        jmp     pnext

op_div:
        lda     (_ivm_ipc),y
        tax
        sty     savey               ; Y at operand; pnext skips it
        lda     MEM_LO_BASE,x
        sta     _ivm_par
        lda     MEM_HI_BASE,x
        sta     _ivm_par+1
        jsr     _opdiv
        jmp     pnext

op_mod:
        lda     (_ivm_ipc),y
        tax
        sty     savey               ; Y at operand; pnext skips it
        lda     MEM_LO_BASE,x
        sta     _ivm_par
        lda     MEM_HI_BASE,x
        sta     _ivm_par+1
        jsr     _opmod
        jmp     pnext

;;; bitwise
op_and:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        and     _ivm_acc
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        and     _ivm_acc+1
        sta     _ivm_acc+1
        PNEXT_HAVEY

op_or:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        ora     _ivm_acc
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        ora     _ivm_acc+1
        sta     _ivm_acc+1
        PNEXT_HAVEY

op_xor:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        eor     _ivm_acc
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        eor     _ivm_acc+1
        sta     _ivm_acc+1
        PNEXT_HAVEY

op_bnot:
        lda     _ivm_acc
        eor     #$FF
        sta     _ivm_acc
        lda     _ivm_acc+1
        eor     #$FF
        sta     _ivm_acc+1
        NEXT

;;; { } shifts
op_shl:
        lda     (_ivm_ipc),y
        tax
        sty     savey               ; Y at operand; pnext skips it
        lda     MEM_LO_BASE,x
        tax
        beq     :++
:       asl     _ivm_acc
        rol     _ivm_acc+1
        dex
        bne     :-
:
        jmp     pnext

op_shr:
        lda     (_ivm_ipc),y
        tax
        sty     savey               ; Y at operand; pnext skips it
        lda     MEM_LO_BASE,x
        tax
        beq     :++
:       lsr     _ivm_acc+1
        ror     _ivm_acc
        dex
        bne     :-
:
        jmp     pnext

;;; print / I/O
op_dot:
        sty     savey
        jsr     _opputnum
        jmp     ynext

op_K:                           ; getchar
        sty     savey
        jsr     _getchar
        sta     _ivm_acc
        stx     _ivm_acc+1
        jmp     ynext

op_N:                           ; newline
        sty     savey
        lda     #10
        jsr     _putchar
        jmp     ynext

op_E:                           ; putchar(ACC)
        sty     savey
        lda     _ivm_acc
        jsr     _putchar
        jmp     ynext

op_O:                           ; inline putchar
        sty     savey
        lda     (_ivm_ipc),y
        jsr     _putchar
        jmp     pnext

;;; byte ops
op_l:                           ; ACC = lo(mem[xx])
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        PNEXT_HAVEY

op_s:                           ; mem[xx] = lo(ACC)
        lda     (_ivm_ipc),y
        tax
        iny
        lda     _ivm_acc
        sta     MEM_LO_BASE,x
        lda     #0
        sta     MEM_HI_BASE,x
        PNEXT_HAVEY

op_i:                           ; char ++  ACC = mem[xx] = (uchar)(mem[xx]+1)
        lda     (_ivm_ipc),y
        tax
        iny
        inc     MEM_LO_BASE,x
        jmp     bincdec

op_j:                           ; char --
        lda     (_ivm_ipc),y
        tax
        iny
        dec     MEM_LO_BASE,x
bincdec:
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        sta     MEM_HI_BASE,x
        PNEXT_HAVEY

;;; rxx / wxx  byte pointer via slot
op_r:
        lda     (_ivm_ipc),y
        tax
        sty     savey               ; Y at operand; pnext skips it
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        jmp     pnext

op_w:
        lda     (_ivm_ipc),y
        tax
        sty     savey               ; Y at operand; pnext skips it
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        ldy     #0
        lda     _ivm_acc
        sta     (tmp_lo),y
        jmp     pnext

;;; M / F  malloc / free
op_M:
        lda     (_ivm_ipc),y
        sta     swpb                ; slot
        sty     savey               ; Y at operand; pnext skips it
        lda     _ivm_acc
        ldx     _ivm_acc+1
        jsr     _ivm_zalloc
        sta     _ivm_acc
        stx     _ivm_acc+1
        ldx     swpb
        sta     MEM_LO_BASE,x
        lda     _ivm_acc+1
        sta     MEM_HI_BASE,x
        jmp     pnext

op_F:
        lda     (_ivm_ipc),y
        sta     swpb
        sty     savey               ; Y at operand; pnext skips it
        ldx     swpb
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        lda     tmp_lo
        ldx     tmp_hi
        jsr     _ivm_free
        ldx     swpb
        lda     #0
        sta     MEM_LO_BASE,x
        sta     MEM_HI_BASE,x
        jmp     pnext

;;; # name\0  skip zero-terminated string
;;; # name\0  skip zero-terminated string
op_hash:
        FIXIPC
:       lda     (_ivm_ipc),y
        beq     :+
        iny
        bne     :-
:       iny
        PNEXT_HAVEY

;;; "  print string at ipc until 0
op_str:
        FIXIPC
strlp:
        ldy     #0
        lda     (_ivm_ipc),y
        beq     strdone
        jsr     _putchar
        inc     _ivm_ipc
        bne     strlp
        inc     _ivm_ipc+1
        bne     strlp
strdone:
        iny
        PNEXT_HAVEY

;;; P  print string from ACC until 0
op_P:
        sty     savey
        lda     _ivm_acc
        sta     tmp_lo
        lda     _ivm_acc+1
        sta     tmp_hi
P_plp:
        ldy     #0
        lda     (tmp_lo),y
        beq     pdone
        jsr     _putchar
        inc     tmp_lo
        bne     P_plp
        inc     tmp_hi
        bne     P_plp
pdone:
        jmp     ynext

;;; Hxx  ACC = mem[xx] += 2
op_H:
        lda     (_ivm_ipc),y
        tax
        iny
        clc
        lda     MEM_LO_BASE,x
        adc     #2
        sta     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        adc     #0
        sta     MEM_HI_BASE,x
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; Jxx  ACC = --mem[xx]
op_J:
        lda     (_ivm_ipc),y
        tax
        iny
        sec
        lda     MEM_LO_BASE,x
        sbc     #1
        sta     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sbc     #0
        sta     MEM_HI_BASE,x
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; Uab  mem[a] += mem[b]; ACC = mem[a]
op_U:
        lda     (_ivm_ipc),y        ; a
        sta     swpb
        iny
        lda     (_ivm_ipc),y        ; b
        tax
        iny
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        ldx     swpb
        clc
        lda     MEM_LO_BASE,x
        adc     tmp_lo
        sta     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        adc     tmp_hi
        sta     MEM_HI_BASE,x
        sta     _ivm_acc+1
        PNEXT_HAVEY

;;; `  (address-of) — not supported with byte planes
op_tick:
        lda     #0
        sta     tmp_lo
@msg:   ldy     tmp_lo
        lda     tick_msg,y
        bne     :+
	jmp     done
        jsr     _putchar
        inc     tmp_lo
        bne     @msg
tick_msg:
        .byte   10, "%Error: ivm-fixed: no & of a variable (`): mem[] is byte planes", 10, 0

;;; unknown opcode — report it
op_bad:
        stx     tmp_lo              ; save opcode
        FIXIPC
        lda     #10
        jsr     _putchar
        lda     #'%'
        jsr     _putchar
        lda     tmp_lo
        jsr     _putchar
        lda     #10
        jsr     _putchar
        jmp     done

;;; ------------------------------------------------------------
;;; Jumps / flow control
;;; ------------------------------------------------------------

;;; G delta  — if ACC > 0 then ipc += delta; always skip delta
;;; Y-relative: target = Y (at delta) + delta + 1
op_G:
        lda     _ivm_acc+1
        bmi     gskip
        ora     _ivm_acc
        beq     gskip
        tya
        sec                         ; +1
        adc     (_ivm_ipc),y
        tay
        bcs     :+
        jmp     pnext_no_load       ; N from tay
:       inc     _ivm_ipc+1
        tya
        jmp     pnext_no_load
gskip:
        iny
        PNEXT_HAVEY

;;; B delta  — ipc -= delta; skip delta
;;; Y-relative: target = Y (at delta) - delta + 1, FIXIPC only on borrow
op_B:
        tya
        sec
        sbc     (_ivm_ipc),y
        bcc     b_far
        tay
        iny                         ; <= old Y: no check
        jmp     xnext
b_far:
        dec     _ivm_ipc+1          ; ipc+256 = ipc+A, A = Y - delta + 256
        tay
        FIXIPC
        inc     _ivm_ipc            ; +1 (FIXIPC left Y = 0)
        bne     :+
        inc     _ivm_ipc+1
:       jmp     xnext

;;; t v u oo  for-test (v <= u)   u v u oo  (v < u)
;;; ACC = mem[v]; true: body after oo, false: skip oo+1 more
;;; n back  — ++v of the t/u at Y+1-back, then its test
;;; Y-relative: Y = offset of v on entry, FIXIPC only on n's borrow
;;; (n first: its exits fall / branch into tu_enter / tu_exit)
op_n:
        tya
        clc
        adc     #1
        sec
        sbc     (_ivm_ipc),y
        tay
        bcs     n_step
        dec     _ivm_ipc+1
        FIXIPC                      ; Y may be >= 253: re-base on opcode
n_step:
        iny                         ; v (Y >= 1: dey below is safe)
        lda     (_ivm_ipc),y        ; v
        tax
        inc     MEM_LO_BASE,x
        bne     :+
        inc     MEM_HI_BASE,x
:       lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        iny
        lda     (_ivm_ipc),y        ; u
        tax
        lda     _ivm_acc            ; v == u: t enters, u exits
        cmp     MEM_LO_BASE,x
        bne     n_ne
        lda     _ivm_acc+1
        cmp     MEM_HI_BASE,x
        bne     n_ne
        dey                         ; peek the t / u (only when equal)
        dey
        lda     (_ivm_ipc),y
        iny
        iny
        cmp     #'u'
        bne     tu_enter
        beq     tu_exit
n_ne:   lda     MEM_LO_BASE,x       ; v != u: u - v, borrow = exit
        cmp     _ivm_acc
        lda     MEM_HI_BASE,x
        sbc     _ivm_acc+1
        bcc     tu_exit
tu_enter:
        iny                         ; oo
        iny                         ; body
        PNEXT_HAVEY

op_t:
        lda     (_ivm_ipc),y        ; v
        tax
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        iny
        lda     (_ivm_ipc),y        ; u
        tax
        lda     MEM_LO_BASE,x       ; u - v: borrow = v > u
        cmp     _ivm_acc
        lda     MEM_HI_BASE,x
        sbc     _ivm_acc+1
        bcs     tu_enter
tu_exit:
        iny                         ; Y at oo
        tya
        sec                         ; +1
        adc     (_ivm_ipc),y
        tay
        bcs     :+
        jmp     pnext_no_load       ; N from tay
:       inc     _ivm_ipc+1
        tya
        jmp     pnext_no_load

op_u:
        lda     (_ivm_ipc),y        ; v
        tax
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        iny
        lda     (_ivm_ipc),y        ; u
        tax
        lda     _ivm_acc            ; v - u: borrow = v < u
        cmp     MEM_LO_BASE,x
        lda     _ivm_acc+1
        sbc     MEM_HI_BASE,x
        bcc     tu_enter
        bcs     tu_exit

;;; v a #k oo  — if mem[a] == k then body else skip oo
op_v:
        FIXIPC
        sty     savey
        lda     (_ivm_ipc),y
        tax
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        bne     v_skip          ; k is a byte
        ldy     #1
        lda     _ivm_acc
        cmp     (_ivm_ipc),y
        bne     v_skip
        lda     #3
        jmp     anext
v_skip:
        ldy     #2
        lda     (_ivm_ipc),y
        clc
        adc     #3
        bcc     :+
        inc     _ivm_ipc+1
:       jmp     anext


;;; a ll ii  striped word load: lo byte at mem[ll]+i, hi byte at mem[ll+1]+i
;;; b ll ii  striped word store (same layout)
op_a:
        jsr     striped_ptrs
        lda     (tmp_lo),y          ; Y = 0
        sta     _ivm_acc
        lda     (jmpvec),y
        sta     _ivm_acc+1
        jmp     pnext

op_b:
        jsr     striped_ptrs
        lda     _ivm_acc
        sta     (tmp_lo),y
        lda     _ivm_acc+1
        sta     (jmpvec),y
        jmp     pnext

;;; tmp = mem[ll] + mem[ii]  (lo plane), jmpvec = mem[ll+1] + mem[ii] (hi)
;;; savey = offset of ii; returns Y = 0
striped_ptrs:
        lda     (_ivm_ipc),y        ; ll
        sta     swpb
        iny
        sty     savey               ; Y at ii; pnext skips it
        lda     (_ivm_ipc),y        ; ii
        tax
        ldy     swpb
        clc
        lda     MEM_LO_BASE,y
        adc     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,y
        adc     MEM_HI_BASE,x
        sta     tmp_hi
        iny                         ; ll+1
        clc
        lda     MEM_LO_BASE,y
        adc     MEM_LO_BASE,x
        sta     jmpvec
        lda     MEM_HI_BASE,y
        adc     MEM_HI_BASE,x
        sta     jmpvec+1
        ldy     #0
        rts

;;; $ vv  ACC = strlen(mem[vv])
op_dollar:
        lda     (_ivm_ipc),y
        tax
        sty     savey               ; Y at operand; pnext skips it
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        lda     tmp_lo
        ldx     tmp_hi
        jsr     _strlen
        sta     _ivm_acc
        stx     _ivm_acc+1
        jmp     pnext

;;; m pp vv  memset
op_m:
        ;; exact port of ivm-vm.asm op_m (ALTCODE path)
	;; tmp = ptr;
        lda     (_ivm_ipc),y            ; PARY equivalent
        tax
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi

	;; A = vv.lo;
        iny
        sty     savey

        lda     (_ivm_ipc),y
        tax
        lda     MEM_LO_BASE,x           ; fill byte in A

        pha
        ldx     _ivm_acc+1              ; hi
        txa
        clc
        adc     tmp_hi
        sta     tmp_hi
        pla

        ldy     _ivm_acc                ; lo
        beq     op_m_page
op_m_loop:
        dey
        sta     (tmp_lo),y
        bne     op_m_loop
op_m_page:
        dec     tmp_hi
        dex
        bpl     op_m_loop

        jmp     pnext
;;; ------------------------------------------------------------
;;; Not yet ported (code exists in ivm-vm.asm; needs param rewrite)
;;; ------------------------------------------------------------
op_A:           ; swap word array elements
op_V:           ; memmove
op_x:           ; call with 1 arg
op_y:           ; call with 2 args
op_colon:
op_D:
op_R:
op_T:
op_W:
op_bslash:
op__:
op_c:
op_d:
op_e:
op_f:
op_g:
op_h:
op_k:
op_o:
op_p:
op_z:
op_del:
        jmp     op_bad

done:
        lda     _ivm_acc
        ldx     _ivm_acc+1
        rts


.export _IVM_ASM_END
_IVM_ASM_END:

.segment "ALIGNDATA"
.align 256, $42

.linecont +
.define JumpTable \
  op_31, op_31, op_31, op_31, op_31, op_31, op_31, op_31, \
  op_31, op_31, op_31, op_31, op_31, op_31, op_31, op_31, \
  op_31, op_31, op_31, op_31, op_31, op_31, op_31, op_31, \
  op_31, op_31, op_31, op_31, op_31, op_31, op_31, op_31, \
  op_sp, op_bang, op_str, op_hash, op_dollar, op_mod, op_and, op_ri, \
  op_idxl, op_idxs, op_mul, op_add, op_wi, op_sub, op_dot, op_div, \
  op_0, op_1to7, op_1to7, op_1to7, op_1to7, op_1to7, op_1to7, op_1to7, \
  op_8, op_9, op_colon, op_semi, op_lt, op_eq, op_gt, op_not, \
  op_at, op_A, op_B, op_C, op_D, op_E, op_F, op_G, \
  op_H, op_I, op_J, op_K, op_L, op_M, op_N, op_O, \
  op_P, op_Q, op_R, op_S, op_T, op_U, op_V, op_W, \
  op_X, op_Y, op_Z, op_bidxl, op_bslash, op_bidxs, op_xor, op__, \
  op_tick, op_a, op_b, op_c, op_d, op_e, op_f, op_g, \
  op_h, op_i, op_j, op_k, op_l, op_m, op_n, op_o, \
  op_p, op_q, op_r, op_s, op_t, op_u, op_v, op_w, \
  op_x, op_y, op_z, op_shl, op_or, op_shr, op_bnot, op_del

.export _dispvec
_dispvec:
dispvec:
    .word JumpTable
