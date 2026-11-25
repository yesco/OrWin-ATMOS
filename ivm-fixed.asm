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
        bcc     pfixnext
        inc     _ivm_ipc+1
        jmp     pfixnext

pnext:
        ldy     savey
        iny
pfixnext:
        bpl     xnext
        FIXIPC
        jmp     xnext

ynext:                          ; kept for compatibility, but rarely needed
        ldy     savey

xnext:
        lda     (_ivm_ipc),y
        iny
        tax                     ; X = opcode (and for 0..31 the slot!)
        bmi     HIBIT
        asl
        sta     pc+1
pc:     jmp     (dispvec)

;;; ----- HIBIT -----
HIBIT:
        asl     a
        bmi     subvec

;;; 0x80..0xBF relative if ACC==0
rjp:
        FIXIPC
        lda     _ivm_acc
        ora     _ivm_acc+1
        bne     xnext
        txa
        and     #$3F
        clc
        adc     _ivm_ipc
        sta     _ivm_ipc
        bcc     xnext
        inc     _ivm_ipc+1
        bcs     xnext

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
        jmp     xnext

;;; ============================================================
;;; Rewritten handlers — memory via absolute,X  (Y untouched)
;;; ============================================================

;;; 0..31: load slot=opcode → ACC.  X already holds the slot!
op_31:
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        jmp     xnext

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
        jmp     xnext

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
        jmp     xnext

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
        jmp     xnext

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
        jmp     pfixnext        ; or just fall if we adjust

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
        jmp     pfixnext

;;; Lxx   ACC = mem[xx]
op_L:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     MEM_HI_BASE,x
        sta     _ivm_acc+1
        jmp     pfixnext

;;; Sxx   mem[xx] = ACC
op_S:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     _ivm_acc
        sta     MEM_LO_BASE,x
        lda     _ivm_acc+1
        sta     MEM_HI_BASE,x
        jmp     pfixnext

;;; Zxx   mem[xx] = 0
op_Z:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     #0
        sta     MEM_LO_BASE,x
        sta     MEM_HI_BASE,x
        jmp     pfixnext

;;; Yxx   mem[xx] = 1
op_Y:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     #1
        sta     MEM_LO_BASE,x
        lda     #0
        sta     MEM_HI_BASE,x
        jmp     pfixnext

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
        jmp     pfixnext

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
        jmp     xnext

op_not:
        lda     _ivm_acc
        ora     _ivm_acc+1
        beq     op_acc1
op_0:
        lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1
        jmp     xnext

op_acc1:
        lda     #1
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        jmp     xnext

op_acc0p:
        lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1
        jmp     pfixnext

op_acc1p:
        lda     #1
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        jmp     pfixnext

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
        jmp     pfixnext

;;; ============================================================
;;; Remaining operators (fixed absolute,X style)
;;; ============================================================

;;; NOP
op_sp:
        jmp     xnext

;;; * / %  — PAR in _ivm_par, then C helper

;;; @xx  ACC = *(word*)mem[xx]
op_at:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        sty     savey
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        iny
        lda     (tmp_lo),y
        sta     _ivm_acc+1
        jmp     pnext

;;; !xx  *(word*)mem[xx] = ACC
op_bang:
        lda     (_ivm_ipc),y
        tax
        iny
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        sty     savey
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
        sta     swpb                ; base slot
        iny
        lda     (_ivm_ipc),y        ; ii
        tax
        iny
        sty     savey
        ; scaled = mem[ii] * 2
        lda     MEM_LO_BASE,x
        asl
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        rol
        sta     tmp_hi
        ; ptr = mem[aa] + scaled
        ldx     swpb
        clc
        lda     MEM_LO_BASE,x
        adc     tmp_lo
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        adc     tmp_hi
        sta     tmp_hi
        ; load word
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
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
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

;;; R aa ii   ACC = ((uchar*)mem[aa])[mem[ii]]
op_bidxl:
        lda     (_ivm_ipc),y        ; aa
        sta     swpb
        iny
        lda     (_ivm_ipc),y        ; ii
        tax
        iny
        sty     savey
        ; idx = mem[ii]
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
        sta     tmp_hi
        ; ptr = mem[aa] + idx
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
        lda     #0
        sta     _ivm_acc+1
        jmp     pnext

;;; W aa ii   ((uchar*)mem[aa])[mem[ii]] = lo(ACC)
op_bidxs:
        lda     (_ivm_ipc),y
        sta     swpb
        iny
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
        lda     MEM_LO_BASE,x
        sta     tmp_lo
        lda     MEM_HI_BASE,x
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
        jmp     pnext

;;; 'xx  ACC = *(uchar*)ptr; ptr++   (ptr in slot xx)
op_ri:
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
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
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
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
        iny
        sty     savey
        lda     MEM_LO_BASE,x
        sta     _ivm_par
        lda     MEM_HI_BASE,x
        sta     _ivm_par+1
        jsr     _opmul
        jmp     pnext

op_div:
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
        lda     MEM_LO_BASE,x
        sta     _ivm_par
        lda     MEM_HI_BASE,x
        sta     _ivm_par+1
        jsr     _opdiv
        jmp     pnext

op_mod:
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
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
        jmp     pfixnext

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
        jmp     pfixnext

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
        jmp     pfixnext

op_bnot:
        lda     _ivm_acc
        eor     #$FF
        sta     _ivm_acc
        lda     _ivm_acc+1
        eor     #$FF
        sta     _ivm_acc+1
        jmp     xnext

;;; { } shifts
op_shl:
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
        lda     MEM_LO_BASE,x
        tax
        beq     :+
:       asl     _ivm_acc
        rol     _ivm_acc+1
        dex
        bne     :-
        jmp     pnext

op_shr:
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
        lda     MEM_LO_BASE,x
        tax
        beq     :+
:       lsr     _ivm_acc+1
        ror     _ivm_acc
        dex
        bne     :-
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
        lda     (_ivm_ipc),y
        iny
        sty     savey
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
        jmp     pfixnext

op_s:                           ; mem[xx] = lo(ACC)
        lda     (_ivm_ipc),y
        tax
        iny
        lda     _ivm_acc
        sta     MEM_LO_BASE,x
        lda     #0
        sta     MEM_HI_BASE,x
        jmp     pfixnext

op_j:                           ; char --
        lda     (_ivm_ipc),y
        tax
        iny
        lda     #$FF
        bne     bincdec

op_i:                           ; char ++
        lda     (_ivm_ipc),y
        tax
        iny
        lda     #1
bincdec:
        clc
        adc     MEM_LO_BASE,x
        sta     MEM_LO_BASE,x
        sta     _ivm_acc
        lda     #0
        sta     MEM_HI_BASE,x
        sta     _ivm_acc+1
        jmp     pfixnext

;;; rxx / wxx  byte pointer via slot
op_r:
        lda     (_ivm_ipc),y
        tax
        iny
        sty     savey
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
        iny
        sty     savey
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
        iny
        sty     savey
        lda     _ivm_acc
        ldx     _ivm_acc+1
        jsr     _ivm_zalloc
        sta     _ivm_acc
        stx     _ivm_acc+1
        ldx     swpb
        sta     MEM_LO_BASE,x
        txa
        sta     MEM_HI_BASE,x
        jmp     pnext

op_F:
        lda     (_ivm_ipc),y
        sta     swpb
        iny
        sty     savey
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
op_hash:
        FIXIPC
:       lda     (_ivm_ipc),y
        beq     :+
        iny
        bne     :-
:       iny
        jmp     pfixnext

;;; `  (address-of) — not supported with byte planes
op_tick:
        lda     #0
        sta     tmp_lo
@msg:   ldy     tmp_lo
        lda     tick_msg,y
        beq     done
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
        ; fall through to done

;;; unrecognized / not-yet-ported fall here
op_dollar:
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
op_A:
op_a:
op_b:
op_U:
op_G:
op_B:
op_t:
op_u:
op_n:
op_v:
op_V:
op_x:
op_y:
op_str:
op_H:
op_J:
op_P:
op_m:
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
