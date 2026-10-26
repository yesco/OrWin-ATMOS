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

.zeropage
jmpvec: .res 2

tmp_lo: .res 1
tmp_hi: .res 1


.data
;;; static dispatch tables (256 bytes) — no runtime init_disp
displo:
	.byte	<op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused
	.byte	<op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused
	.byte	<op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused
	.byte	<op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused, <op_fused
	.byte	<op_sp, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c
	.byte	<op_c, <op_c, <op_c, <op_add, <op_c, <op_sub, <op_c, <op_c
	.byte	<op_0, <op_17, <op_17, <op_17, <op_17, <op_17, <op_17, <op_17
	.byte	<op_8, <op_9, <op_c, <op_semi, <op_c, <op_c, <op_c, <op_not
	.byte	<op_c, <op_c, <op_B, <op_c, <op_c, <op_c, <op_c, <op_G
	.byte	<op_c, <op_c, <op_c, <op_c, <op_L, <op_c, <op_c, <op_c
	.byte	<op_c, <op_Q, <op_c, <op_S, <op_c, <op_c, <op_c, <op_c
	.byte	<op_X, <op_c, <op_Z, <op_c, <op_c, <op_c, <op_c, <op_c
	.byte	<op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c
	.byte	<op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c
	.byte	<op_c, <op_q, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c
	.byte	<op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c

disphi:
	.byte	>op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused
	.byte	>op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused
	.byte	>op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused
	.byte	>op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused, >op_fused
	.byte	>op_sp, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c
	.byte	>op_c, >op_c, >op_c, >op_add, >op_c, >op_sub, >op_c, >op_c
	.byte	>op_0, >op_17, >op_17, >op_17, >op_17, >op_17, >op_17, >op_17
	.byte	>op_8, >op_9, >op_c, >op_semi, >op_c, >op_c, >op_c, >op_not
	.byte	>op_c, >op_c, >op_B, >op_c, >op_c, >op_c, >op_c, >op_G
	.byte	>op_c, >op_c, >op_c, >op_c, >op_L, >op_c, >op_c, >op_c
	.byte	>op_c, >op_Q, >op_c, >op_S, >op_c, >op_c, >op_c, >op_c
	.byte	>op_X, >op_c, >op_Z, >op_c, >op_c, >op_c, >op_c, >op_c
	.byte	>op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c
	.byte	>op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c
	.byte	>op_c, >op_q, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c
	.byte	>op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c

.code

;;; ----- consume 1-byte operand -----
pnext:
;;; 8c (12c)
;;; TODO: combine +2 of xnext if xnext as a jump in point
	
        inc     ivm_ipc
        bne     :+
        inc     ivm_ipc+1
:	
	;; fall-through
_run_ivm_asm:	

;;; ----- main loop -----
xnext:
	;; 10c
        ldy     #0
        lda     (ivm_ipc),y
        sta     ivm_op		; TODO: getrid if, keep in reg

	;; 8-12c
        inc     ivm_ipc
        bne     :+
        inc     ivm_ipc+1
:

        ;; lobit - 0..127
	;; 6c
        tax
        bmi     HIBIT
	;; +18c
        lda     disphi,x
        sta     jmpvec+1

        lda     displo,x
        sta     jmpvec

        jmp     (jmpvec)

;;; ----- HIBIT: ASL → N = former bit6 -----
HIBIT:
	;; 5
        asl     a               ; N=1 if original 0xC0..0xFF
        bmi     subvec          ; vector subroutine

        ;; 0x80..0xBF: relative forward if ACC == 0
rjmp:	
;;; 21c - 28c
	;; if !acc then rjmp
	;; +8c
        lda     ivm_acc
        ora     ivm_acc+1
        bne     xnext
	;; +12c
        lda     ivm_op
        and     #$3F

        clc
        adc     ivm_ipc
        sta     ivm_ipc

	;; lo byte overflow?
	;; +3-10c
        bcc     xnext
        inc     ivm_ipc+1
        jmp     xnext		; TODO: bcs

subvec:
        ;; vector index = op & $3F — for now C helper (needs call/fp tables)
        jsr     _run_ivm_once
        jmp     xnext


;;; ============================================================
;;; Handlers (0..31 loads installed in init_disp as op_fused)
;;; hi first → X, lo → A
;;; ============================================================

;;; load mem[Y] → acc (and AX)
.macro  LOAD_SLOT_Y 
	;; 18c 9 B
        lda     (ivm_memhi),y
        tax
        lda     (ivm_memlo),y

        sta     ivm_acc
        stx     ivm_acc+1
.endmacro

;;; AX= *ipc is slot → AX A=lo X=hi
.macro  PAR_TO_AX
	;; 21c  10 B
        ldy     #0
        lda     (ivm_ipc),y
        tay

        lda     (ivm_memhi),y
        tax
        lda     (ivm_memlo),y
.endmacro

;;; opcodes 0..31: slot = op itself
op_fused:
        ldy     ivm_op 		; TODO: isn't it in Y already?
        LOAD_SLOT_Y
        jmp     xnext

;;; TODO: rename to 0x7f ??? or is that future nextpage?
jmp_done:	
op_q:   jmp     done


op_Q:   lda     #0
        sta     ivm_acc
        sta     ivm_acc+1

op_X:
	lda     ivm_fp
	;; magic done, TODO: remove: use 0x7f as exit()
        cmp     #FP_MAIN
        beq     jmp_done

	;; restore IPC from FR_RET
        clc
        adc     #FR_RET

        tay
        lda     (ivm_memlo),y
        sta     ivm_ipc
        lda     (ivm_memhi),y
        sta     ivm_ipc+1

	;; restore FP from FR_PREV
        lda     ivm_fp 		; TODO: why again? should INY?"
        clc
        adc     #FR_PREV
        tay
        lda     (ivm_memlo),y
        sta     ivm_fp

        jmp     xnext

;;; return by keeping FUNNAME (value)
op_semi:
        lda     ivm_fp
	;; TODO: seems redundant, no harm doing the other stuff?
        cmp     #FP_MAIN
        beq     jmp_done

	;; Loads FR_VAL index
        clc
        adc     #FR_VAL
        tay
        LOAD_SLOT_Y

        jmp     op_X


;;; arg 8 == last argument (ACC when called)
op_8:   lda     ivm_fp
        clc
        adc     #FR_ACC
        tay

        LOAD_SLOT_Y
        jmp     xnext

;;; args 1..7 
op_17:  lda     ivm_op
        sec
        sbc     #'0'
        clc
        adc     #FR_ACC
        adc     ivm_fp
        tay

        LOAD_SLOT_Y
        jmp     xnext


op_sub: PAR_TO_AX
	;; alt: do xor on neg value before add!
	;; then can just fall into op_add!!!
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

op_add: PAR_TO_AX
        clc
        adc     ivm_acc
        sta     ivm_acc
        txa
        adc     ivm_acc+1
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


;;; ACC = -1  or 0xFF or TRUE (forth/basic)
op_9:   lda     #$FF
        sta     ivm_acc
        sta     ivm_acc+1

        jmp     xnext

op_not: lda     ivm_acc
        ora     ivm_acc+1
        beq     ret1

;;; ACC = 0
op_0:   lda     #0
        sta     ivm_acc
        sta     ivm_acc+1

        jmp     xnext

;;; ACC = 1
ret1:   lda     #1
        sta     ivm_acc
        lda     #0
        sta     ivm_acc+1

        jmp     xnext


;;; Conditional GOTO +1B iff acc > 
;;; TODO: replace usage by rjmp, or make it an FUSED LOOP:
;;; TODO: Giiuuee (ii=index var, uu=upper limit pascal, ee== rjmp end)
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
:
	jmp     pnext


;;; NOP - SPACE
op_sp:  jmp     xnext


;;; NYI → C one-op
op_c:   jsr     _run_ivm_once

        jmp     xnext


;;; C style return ACC in AX
done:   lda     ivm_acc
        ldx     ivm_acc+1

        rts
