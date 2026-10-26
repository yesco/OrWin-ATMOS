;;; ivm-vm.asm — 6502 IVM (ca65)
;;;
;;; ZP symbols are DEFINED IN C (ivm.c / glue) and only IMPORTed here so a
;;; process switch is memcpy of that ZP block — several processes share the
;;; same engine, different saved ZP images.
;;;
;;;   .importzp  ivm_ipc, ivm_fp, ivm_acc, ivm_op, ivm_memlo, ivm_memhi
;;;   (ivm_ipc, ivm_acc, ivm_memlo, ivm_memhi = 16-bit; ivm_fp, ivm_op = 8-bit)
;;;
;;; Dispatch:
;;;   op = *ipc++
;;;   if (op & $80) goto HIBIT
;;;   jmp (disphi[op] << 8 | displo[op])     ; index = op, NO ASL
;;;                                          ; slots 0..31 are normal table entries
;;; HIBIT:
;;;   ASL A          ; N = old bit6
;;;   BPL zrel       ; 80-BF: if !acc  ipc += op & $3F
;;;   BMI jsrvec     ; C0-FF: vector call  (op & $3F)  via C once-helper for now

.export         _run_ivm_asm

.importzp       ivm_ipc, ivm_fp, ivm_acc, ivm_op
.importzp       ivm_memlo, ivm_memhi
.import         _run_ivm_once

FR_RET  = 0
FR_VAL  = 1
FR_PREV = 2
FR_ACC  = 3
FP_MAIN = $FF

.bss
tmp_lo: .res 1
tmp_hi: .res 1
jmpvec: .res 2
disp_ok:.res 1

.data
displo: .res 128
disphi: .res 128

.code

.proc   _run_ivm_asm
        lda     disp_ok
        bne     xnext
        jsr     init_disp
        lda     #1
        sta     disp_ok

	jmp     xnext

;;; ----- consume 1-byte operand -----
pnext:
        inc     ivm_ipc
        bne     xnext
        inc     ivm_ipc+1

;;; ----- main loop -----
xnext:
        ldy     #0
        lda     (ivm_ipc),y
        sta     ivm_op
        inc     ivm_ipc
        bne     :+
        inc     ivm_ipc+1
:

        ;; hibit → HIBIT; else table[op] (includes 0..31)
        lda     ivm_op
        bmi     HIBIT

        ldx     ivm_op          ; index 0..127, no shift
        lda     disphi,x
        sta     jmpvec+1
        lda     displo,x
        sta     jmpvec
        jmp     (jmpvec)

;;; ----- HIBIT: ASL → N = former bit6 -----
HIBIT:
        asl     a               ; N=1 if original 0xC0..0xFF
        bmi     jsrvec          ; vector subroutine
        ;; 0x80..0xBF: relative forward if ACC == 0
        lda     ivm_acc
        ora     ivm_acc+1
        bne     xnext
        lda     ivm_op
        and     #$3F
        clc
        adc     ivm_ipc
        sta     ivm_ipc
        bcc     xnext
        inc     ivm_ipc+1
        jmp     xnext

jsrvec:
        ;; vector index = op & $3F — for now C helper (needs call/fp tables)
        jsr     _run_ivm_once
        jmp     xnext


;;; ============================================================
;;; Handlers (0..31 loads installed in init_disp as op_fused)
;;; hi first → X, lo → A
;;; ============================================================

;;; load mem[Y] → acc
.macro  LOAD_SLOT_Y
        lda     (ivm_memhi),y
        tax
        lda     (ivm_memlo),y
        sta     ivm_acc
        stx     ivm_acc+1
.endmacro

;;; *ipc is slot → A=lo X=hi
.macro  PAR_TO_AX
        ldy     #0
        lda     (ivm_ipc),y
        tay
        lda     (ivm_memhi),y
        tax
        lda     (ivm_memlo),y
.endmacro

;;; opcodes 0..31: slot = op itself
op_fused:
        ldy     ivm_op
        LOAD_SLOT_Y
        jmp     xnext

op_q:   jmp     done

op_Q:   lda     #0
        sta     ivm_acc
        sta     ivm_acc+1
op_X:   lda     ivm_fp
        cmp     #FP_MAIN
        beq     done
        clc
        adc     #FR_RET
        tay
        lda     (ivm_memlo),y
        sta     ivm_ipc
        lda     (ivm_memhi),y
        sta     ivm_ipc+1
        lda     ivm_fp
        clc
        adc     #FR_PREV
        tay
        lda     (ivm_memlo),y
        sta     ivm_fp
        jmp     xnext

op_semi:
        lda     ivm_fp
        cmp     #FP_MAIN
        beq     done
        clc
        adc     #FR_VAL
        tay

        LOAD_SLOT_Y
        jmp     op_X

op_8:   lda     ivm_fp
        clc
        adc     #FR_ACC
        tay

        LOAD_SLOT_Y
        jmp     xnext

op_17:  lda     ivm_op
        sec
        sbc     #'0'
        clc
        adc     #FR_ACC
        adc     ivm_fp
        tay

        LOAD_SLOT_Y
        jmp     xnext

op_add: PAR_TO_AX
        clc
        adc     ivm_acc
        sta     ivm_acc
        txa
        adc     ivm_acc+1
        sta     ivm_acc+1

        jmp     pnext

op_sub: PAR_TO_AX
        sta     tmp_lo
        stx     tmp_hi
        sec
        lda     ivm_acc
        sbc     tmp_lo
        sta     ivm_acc
        lda     ivm_acc+1
        sbc     tmp_hi
        sta     ivm_acc+1

        jmp     pnext

op_L:   PAR_TO_AX
        sta     ivm_acc
        stx     ivm_acc+1

        jmp     pnext

op_S:   ldy     #0
        lda     (ivm_ipc),y
        tay
        lda     ivm_acc
        sta     (ivm_memlo),y
        lda     ivm_acc+1
        sta     (ivm_memhi),y

        jmp     pnext

op_Z:   ldy     #0
        lda     (ivm_ipc),y
        tay
        lda     #0
        sta     (ivm_memlo),y
        sta     (ivm_memhi),y

        jmp     pnext

op_0:   lda     #0
        sta     ivm_acc
        sta     ivm_acc+1

        jmp     xnext

op_9:   lda     #$FF
        sta     ivm_acc
        sta     ivm_acc+1

        jmp     xnext

op_not: lda     ivm_acc
        ora     ivm_acc+1
        beq     not0
        lda     #0
        sta     ivm_acc
        sta     ivm_acc+1

        jmp     xnext

not0:   lda     #1
        sta     ivm_acc
        lda     #0
        sta     ivm_acc+1

        jmp     xnext

op_G:   lda     ivm_acc+1
        bmi     gsk
        ora     ivm_acc
        beq     gsk
        ldy     #0
        lda     (ivm_ipc),y
        clc
        adc     ivm_ipc
        sta     ivm_ipc
        bcc     gsk
        inc     ivm_ipc+1

gsk:    jmp     pnext

op_B:   ldy     #0
        lda     ivm_ipc
        sec
        sbc     (ivm_ipc),y
        sta     ivm_ipc
        bcs     :+
        dec     ivm_ipc+1

:       jmp     pnext

op_sp:  jmp     xnext

; NYI → C one-op
op_c:   jsr     _run_ivm_once

        jmp     xnext

done:   lda     ivm_acc
        ldx     ivm_acc+1

        rts

; ----- fill displo/disphi once -----
.proc   init_disp
        ldx     #0

        ; 0..31 → op_fused
f31:    lda     #<op_fused
        sta     displo,x
        lda     #>op_fused
        sta     disphi,x
        inx
        cpx     #32
        bne     f31

        ; 32..127 → op_c default
dfl:    lda     #<op_c
        sta     displo,x
        lda     #>op_c
        sta     disphi,x
        inx
        bpl     dfl             ; until x wraps past 127


.macro  SET op, lab
	;; 10 bytes for eaxch usage
	;; 23 times => 230 bytes + 28 === 258 bytes
	;; why not just create the table?
	;; NOW WE HAVE THE CODE + TABLE == 512 bytes (258 bytes waste!)
        lda     #<(lab)
        sta     displo + (op)
        lda     #>(lab)
        sta     disphi + (op)
.endmacro
        SET     'q', op_q
        SET     'Q', op_Q
        SET     'X', op_X
        SET     ';', op_semi
        SET     '8', op_8

        SET     '1', op_17
        SET     '2', op_17
        SET     '3', op_17
        SET     '4', op_17
        SET     '5', op_17

        SET     '6', op_17
        SET     '7', op_17
        SET     '+', op_add
        SET     '-', op_sub
        SET     'L', op_L

        SET     'S', op_S
        SET     'Z', op_Z
        SET     '0', op_0
        SET     '9', op_9
        SET     '?', op_not

        SET     'G', op_G
        SET     'B', op_B
        SET     ' ', op_sp

        rts
.endproc

.endproc
