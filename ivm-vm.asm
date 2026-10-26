;;; ivm-vm.asm — 6502 IVM (ca65)
;;;
;;; ZP symbols are DEFINED IN C (ivm.c / glue) and only IMPORTed here so a
;;; process switch is memcpy of that ZP block — several processes share the
;;; same engine, different saved ZP images.
;;;
;;;   .importzp  _ivm_ipc, _ivm_fp, _ivm_acc, _ivm_op, _ivm_memlo, _ivm_memhi, _ivm_par
;;;   (C names get _ prefix under cc65; 16-bit: ipc/acc/memlo/memhi/par; 8-bit: fp/op)
;;;
;;; Dispatch:
;;;   op = *ipc++
;;;   if (op & $80) goto HIBIT
;;;   jmp (disphi[op] << 8 | displo[op])     ; index = op, NO ASL
;;;                                          ; slots 0..31 are normal table entries
;;; HIBIT:
;;;   ASL A          ; N = old bit6
;;;   BPL zrel       ; 80-BF: if !acc  ipc += op & $3F
;;;   BMI subvec     ; C0-FF: vector call  (op & $3F)  inverse of X/;/Q

.export         _run_ivm_asm

.importzp       _ivm_ipc, _ivm_fp, _ivm_acc, _ivm_op
.importzp       _ivm_memlo, _ivm_memhi, _ivm_par
.importzp       _ivm_fptab, _ivm_callv

.import         _opmul, _opdiv, _opmod, _opputnum
.import         _putchar

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
	.byte	<op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31
	.byte	<op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31
	.byte	<op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31
	.byte	<op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31
	.byte	<op_sp, <op_c, <op_str, <op_c, <op_c, <op_mod, <op_and, <op_c
	.byte	<op_c, <op_c, <op_mul, <op_add, <op_c, <op_sub, <op_dot, <op_div
	.byte	<op_0, <op_17, <op_17, <op_17, <op_17, <op_17, <op_17, <op_17
	.byte	<op_8, <op_9, <op_c, <op_semi, <op_c, <op_c, <op_c, <op_not
	.byte	<op_c, <op_c, <op_B, <op_c, <op_c, <op_c, <op_c, <op_G
	.byte	<op_c, <op_I, <op_J, <op_c, <op_L, <op_c, <op_N, <op_O
	.byte	<op_c, <op_Q, <op_c, <op_S, <op_c, <op_U, <op_c, <op_c
	.byte	<op_X, <op_c, <op_Z, <op_c, <op_c, <op_c, <op_xor, <op_c
	.byte	<op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c
	.byte	<op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c
	.byte	<op_c, <op_q, <op_c, <op_c, <op_c, <op_c, <op_c, <op_c
	.byte	<op_c, <op_c, <op_c, <op_c, <op_or, <op_c, <op_c, <op_c
disphi:
	.byte	>op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31
	.byte	>op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31
	.byte	>op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31
	.byte	>op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31
	.byte	>op_sp, >op_c, >op_str, >op_c, >op_c, >op_mod, >op_and, >op_c
	.byte	>op_c, >op_c, >op_mul, >op_add, >op_c, >op_sub, >op_dot, >op_div
	.byte	>op_0, >op_17, >op_17, >op_17, >op_17, >op_17, >op_17, >op_17
	.byte	>op_8, >op_9, >op_c, >op_semi, >op_c, >op_c, >op_c, >op_not
	.byte	>op_c, >op_c, >op_B, >op_c, >op_c, >op_c, >op_c, >op_G
	.byte	>op_c, >op_I, >op_J, >op_c, >op_L, >op_c, >op_N, >op_O
	.byte	>op_c, >op_Q, >op_c, >op_S, >op_c, >op_U, >op_c, >op_c
	.byte	>op_X, >op_c, >op_Z, >op_c, >op_c, >op_c, >op_xor, >op_c
	.byte	>op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c
	.byte	>op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c
	.byte	>op_c, >op_q, >op_c, >op_c, >op_c, >op_c, >op_c, >op_c
	.byte	>op_c, >op_c, >op_c, >op_c, >op_or, >op_c, >op_c, >op_c

.code

;;  TODO: move the most common/expensive OP that tkaes a parameter here!

;;; ----- consume 1-byte operand -----
pnext:
;;; 8c (12c)
;;; TODO: combine +2 of xnext if xnext as a jump in point
        inc     _ivm_ipc
        bne     :+
        inc     _ivm_ipc+1
:	
	;; fall-through
_run_ivm_asm:	

;;; ----- main loop -----
xnext:
;;; a) selfmodify can do -5c
;;; b) use Y for lo ipc
;;; c) if page aligned load - no need INC+BNE: 8c but can do 2c!!!
;;;    using NEXT_PAGE opoerator at 0x??ff slots
;;; d) consider keeping ACC in AX (extra cost one STA, if not use X for disp)
;;;    but savingss could be big
	
	;; 10c
        ldy     #0
        lda     (_ivm_ipc),y
        sta     _ivm_op		; TODO: getrid if, keep in reg

;;; print
	jsr     _putchar

	;; 8-12c
        inc     _ivm_ipc
        bne     :+
        inc     _ivm_ipc+1
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
        lda     _ivm_acc
        ora     _ivm_acc+1
        bne     xnext
	;; +12c
        lda     _ivm_op
        and     #$3F

        clc
        adc     _ivm_ipc
        sta     _ivm_ipc

	;; lo byte overflow?
	;; +3-10c
        bcc     xnext
        inc     _ivm_ipc+1
        jmp     xnext		; TODO: bcs

subvec:
;;; print trace nl
	lda     #10	
	jsr     _putchar

        ;; v = op & $3F
        ;; nfp = fptab[v]
        ;; mem[nfp+FR_PREV] = fp
        ;; mem[nfp+FR_RET]  = ipc
        ;; mem[nfp+FR_ACC]  = acc   (last param)
        ;; fp  = nfp
        ;; ipc = callv[v]
        ;; (inverse of op_X / op_semi)

        lda     _ivm_op
        and     #$3F
        tay                     ; Y = v
        lda     (_ivm_fptab),y
        sta     tmp_lo          ; nfp

        ;; FR_PREV = old fp
        clc
        adc     #FR_PREV
        tay
        lda     _ivm_fp
        sta     (_ivm_memlo),y
        lda     #0
        sta     (_ivm_memhi),y

        ;; FR_RET = ipc (return address)
        lda     tmp_lo
        clc
        adc     #FR_RET
        tay
        lda     _ivm_ipc
        sta     (_ivm_memlo),y
        lda     _ivm_ipc+1
        sta     (_ivm_memhi),y

        ;; FR_ACC = acc (last parameter)
        lda     tmp_lo
        clc
        adc     #FR_ACC
        tay
        lda     _ivm_acc
        sta     (_ivm_memlo),y
        lda     _ivm_acc+1
        sta     (_ivm_memhi),y

        ;; fp = nfp
        lda     tmp_lo
        sta     _ivm_fp

        ;; ipc = callv[v]  (word table, index v*2)
        lda     _ivm_op
        and     #$3F
        asl     a
        tay
        lda     (_ivm_callv),y
        sta     _ivm_ipc
        iny
        lda     (_ivm_callv),y
        sta     _ivm_ipc+1

        jmp     xnext


;;; ============================================================
;;; Handlers (0..31 loads installed in init_disp as op_31)
;;; hi first → X, lo → A
;;; ============================================================

;;; load mem[Y] → acc (and AX)
.macro  LOAD_SLOT_Y 
	;; 18c 9 B
        lda     (_ivm_memhi),y
        tax
        lda     (_ivm_memlo),y

        sta     _ivm_acc
        stx     _ivm_acc+1
.endmacro

;;; AX= *ipc is slot → AX A=lo X=hi
.macro  PARAX
	;; 21c  10 B
        ldy     #0
        lda     (_ivm_ipc),y
        tay

        lda     (_ivm_memhi),y
        tax
        lda     (_ivm_memlo),y
.endmacro

;;; opcodes 0..31: slot = op itself
op_31:
        ldy     _ivm_op 		; TODO: isn't it in Y already?
        LOAD_SLOT_Y
        jmp     xnext

;;; TODO: rename to 0x7f ??? or is that future nextpage?
jmp_done:	
op_q:   jmp     done


op_Q:   lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1

op_X:
	lda     _ivm_fp
	;; magic done, TODO: remove: use 0x7f as exit()
        cmp     #FP_MAIN
        beq     jmp_done

	;; restore IPC from FR_RET
        clc
        adc     #FR_RET

        tay
        lda     (_ivm_memlo),y
        sta     _ivm_ipc
        lda     (_ivm_memhi),y
        sta     _ivm_ipc+1

	;; restore FP from FR_PREV
        lda     _ivm_fp 		; TODO: why again? should INY?"
        clc
        adc     #FR_PREV
        tay
        lda     (_ivm_memlo),y
        sta     _ivm_fp

        jmp     xnext

;;; return by keeping FUNNAME (value)
op_semi:
        lda     _ivm_fp
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
op_8:   lda     _ivm_fp
        clc
        adc     #FR_ACC
        tay

        LOAD_SLOT_Y
        jmp     xnext

;;; args 1..7 
op_17:  lda     _ivm_op
        sec
        sbc     #'0'
        clc
        adc     #FR_ACC
        adc     _ivm_fp
        tay

        LOAD_SLOT_Y
        jmp     xnext


op_sub: PARAX
	;; alt: do xor on neg value before add!
	;; then can just fall into op_add!!!
        sta     tmp_lo
        stx     tmp_hi
        sec
        lda     _ivm_acc
        sbc     tmp_lo
        sta     _ivm_acc
        lda     _ivm_acc+1
        sbc     tmp_hi
        sta     _ivm_acc+1

        jmp     pnext

op_add: PARAX
        clc
        adc     _ivm_acc
        sta     _ivm_acc
        txa
        adc     _ivm_acc+1
        sta     _ivm_acc+1

        jmp     pnext


op_L:   PARAX
        sta     _ivm_acc
        stx     _ivm_acc+1

        jmp     pnext

op_S:   ldy     #0
        lda     (_ivm_ipc),y
        tay
        lda     _ivm_acc
        sta     (_ivm_memlo),y
        lda     _ivm_acc+1
        sta     (_ivm_memhi),y

        jmp     pnext

op_Z:   ldy     #0
        lda     (_ivm_ipc),y
        tay
        lda     #0
        sta     (_ivm_memlo),y
        sta     (_ivm_memhi),y

        jmp     pnext


;;; ACC = -1  or 0xFF or TRUE (forth/basic)
op_9:   lda     #$FF
        sta     _ivm_acc
        sta     _ivm_acc+1

        jmp     xnext

op_not: lda     _ivm_acc
        ora     _ivm_acc+1
        beq     ret1

;;; ACC = 0
op_0:   lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1

        jmp     xnext

;;; ACC = 1
ret1:   lda     #1
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1

        jmp     xnext


;;; Ixx  acc = ++mem[xx]
op_I:   ldy     #0
        lda     (_ivm_ipc),y
        tay
        clc
        lda     (_ivm_memlo),y
        adc     #1
        sta     (_ivm_memlo),y
        sta     _ivm_acc
        lda     (_ivm_memhi),y
        adc     #0
        sta     (_ivm_memhi),y
        sta     _ivm_acc+1
        jmp     pnext

;;; Jxx  acc = --mem[xx]
op_J:   ldy     #0
        lda     (_ivm_ipc),y
        tay
        sec
        lda     (_ivm_memlo),y
        sbc     #1
        sta     (_ivm_memlo),y
        sta     _ivm_acc
        lda     (_ivm_memhi),y
        sbc     #0
        sta     (_ivm_memhi),y
        sta     _ivm_acc+1
        jmp     pnext

;;; Uab  mem[a] += mem[b]; acc = mem[a]
op_U:   ldy     #0
        lda     (_ivm_ipc),y
        sta     tmp_lo              ; a
        iny
        lda     (_ivm_ipc),y
        tay                         ; b
        lda     (_ivm_memlo),y
        sta     tmp_hi              ; b_lo
        lda     (_ivm_memhi),y
        tax                         ; b_hi
        ldy     tmp_lo
        clc
        lda     (_ivm_memlo),y
        adc     tmp_hi
        sta     (_ivm_memlo),y
        sta     _ivm_acc
        txa
        adc     (_ivm_memhi),y
        sta     (_ivm_memhi),y
        sta     _ivm_acc+1
        ; consume 2 operand bytes
        inc     _ivm_ipc
        bne     :+
        inc     _ivm_ipc+1
:       jmp     pnext

;;; Conditional GOTO +1B iff acc > 
;;; TODO: replace usage by rjmp, or make it an FUSED LOOP:
;;; TODO: Giiuuee (ii=index var, uu=upper limit pascal, ee== rjmp end)
op_G:
;;; print trace nl
	lda     #10	
	jsr     _putchar

	lda     _ivm_acc+1
        bmi     gsk
        ora     _ivm_acc
        beq     gsk
        ldy     #0
        lda     (_ivm_ipc),y
        clc
        adc     _ivm_ipc
        sta     _ivm_ipc
        bcc     gsk
        inc     _ivm_ipc+1

gsk:    jmp     pnext


op_B:   ldy     #0
        lda     _ivm_ipc
        sec
        sbc     (_ivm_ipc),y
        sta     _ivm_ipc
        bcs     :+
        dec     _ivm_ipc+1
:
	jmp     pnext


;;; NOP - SPACE
op_sp:  jmp     xnext


;;; PAR ops that need C (mul/div/mod) — PAR already in _ivm_par
op_mul: PARAX
        sta     _ivm_par
        stx     _ivm_par+1
        jsr     _opmul
        jmp     pnext

op_div: PARAX
        sta     _ivm_par
        stx     _ivm_par+1
        jsr     _opdiv
        jmp     pnext

op_mod: PARAX
        sta     _ivm_par
        stx     _ivm_par+1
        jsr     _opmod
        jmp     pnext

op_and: PARAX
        and     _ivm_acc
        sta     _ivm_acc
        txa
        and     _ivm_acc+1
        sta     _ivm_acc+1
        jmp     pnext

op_or:  PARAX
        ora     _ivm_acc
        sta     _ivm_acc
        txa
        ora     _ivm_acc+1
        sta     _ivm_acc+1
        jmp     pnext

op_xor: PARAX
        eor     _ivm_acc
        sta     _ivm_acc
        txa
        eor     _ivm_acc+1
        sta     _ivm_acc+1
        jmp     pnext

;;; print ACC as unsigned decimal
op_dot: jsr     _opputnum
        jmp     xnext

;;; newline
op_N:   lda     #10
        jsr     _putchar
        jmp     xnext

;;; putchar ACC lo (or PAR?) — opcode 'O'
op_O:   lda     _ivm_acc
        jsr     _putchar
        jmp     xnext

;;; string at ipc until 0
op_str:
strlp:  ldy     #0
        lda     (_ivm_ipc),y
        beq     strdn
        jsr     _putchar
        inc     _ivm_ipc
        bne     strlp
        inc     _ivm_ipc+1
        jmp     strlp
strdn:  jmp     pnext

;;; default unknown
op_c:   jmp     xnext


;;; C style return ACC in AX
done:   lda     _ivm_acc
        ldx     _ivm_acc+1

        rts
