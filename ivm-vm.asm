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

TRACE=1
;TRACE=0
        
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

savey:  .res 1
        



.data
;;; static dispatch tables (256 bytes) — no runtime init_disp
displo:
        .byte   <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31
        .byte   <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31
        .byte   <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31
        .byte   <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31, <op_31
        .byte   <op_sp, <op_bad, <op_str, <op_bad, <op_bad, <op_mod, <op_and, <op_bad
        .byte   <op_bad, <op_bad, <op_mul, <op_add, <op_bad, <op_sub, <op_dot, <op_div
        .byte   <op_0, <op_1to7, <op_1to7, <op_1to7, <op_1to7, <op_1to7, <op_1to7, <op_1to7
        .byte   <op_8, <op_9, <op_bad, <op_semi, <op_lt, <op_eq, <op_gt, <op_not
        .byte   <op_bad, <op_bad, <op_B, <op_bad, <op_bad, <op_bad, <op_bad, <op_G
        .byte   <op_bad, <op_I, <op_J, <op_bad, <op_L, <op_bad, <op_N, <op_O
        .byte   <op_bad, <op_Q, <op_bad, <op_S, <op_bad, <op_U, <op_bad, <op_bad
        .byte   <op_X, <op_bad, <op_Z, <op_shl, <op_bad, <op_shr, <op_xor, <op_bad
        .byte   <op_bad, <op_bad, <op_bad, <op_bad, <op_bad, <op_bad, <op_bad, <op_bad
        .byte   <op_bad, <op_bad, <op_bad, <op_bad, <op_bad, <op_bad, <op_bad, <op_bad
        .byte   <op_bad, <op_q, <op_bad, <op_bad, <op_bad, <op_bad, <op_bad, <op_bad
        .byte   <op_bad, <op_bad, <op_bad, <op_shlm, <op_or, <op_shrm, <op_bnot, <op_bad
disphi:
        .byte   >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31
        .byte   >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31
        .byte   >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31
        .byte   >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31, >op_31
        .byte   >op_sp, >op_bad, >op_str, >op_bad, >op_bad, >op_mod, >op_and, >op_bad
        .byte   >op_bad, >op_bad, >op_mul, >op_add, >op_bad, >op_sub, >op_dot, >op_div
        .byte   >op_0, >op_1to7, >op_1to7, >op_1to7, >op_1to7, >op_1to7, >op_1to7, >op_1to7
        .byte   >op_8, >op_9, >op_bad, >op_semi, >op_lt, >op_eq, >op_gt, >op_not
        .byte   >op_bad, >op_bad, >op_B, >op_bad, >op_bad, >op_bad, >op_bad, >op_G
        .byte   >op_bad, >op_I, >op_J, >op_bad, >op_L, >op_bad, >op_N, >op_O
        .byte   >op_bad, >op_Q, >op_bad, >op_S, >op_bad, >op_U, >op_bad, >op_bad
        .byte   >op_X, >op_bad, >op_Z, >op_shl, >op_bad, >op_shr, >op_xor, >op_bad
        .byte   >op_bad, >op_bad, >op_bad, >op_bad, >op_bad, >op_bad, >op_bad, >op_bad
        .byte   >op_bad, >op_bad, >op_bad, >op_bad, >op_bad, >op_bad, >op_bad, >op_bad
        .byte   >op_bad, >op_q, >op_bad, >op_bad, >op_bad, >op_bad, >op_bad, >op_bad
        .byte   >op_bad, >op_bad, >op_bad, >op_shlm, >op_or, >op_shrm, >op_bnot, >op_bad

.code

;;  TODO: move the most common/expensive OP that tkaes a parameter here!


.if 1
;;; This optimization tries to avoid doing excessive
;;; ipc++ that is 8c (or 12c when wrap).
;;; 
;;; Instead, it uses an offset in Y and simply INY.
;;; Every op that takes a parameter will use Y by using PARAX,
;;; and every other op that modifies Y will need to say USEY.
;;; It saves Y. pnext and ynext restores Y.
;;; 
;;; To counter the wrap around, every time we have a parameter
;;; we check if Y>=128 (Minus), if so, we do ipc+= Y; Y=0.
;;; The *ASSUMPTION* is that we going to hit a parameter function
;;; within 128 ops of linear execution... (yes can create counter-
;;; example, but in practice?).
;;; 
;;; TODO: ? detect such during compilation.
;;; 
;;; So we save 8c-2c=6c every step,
;;; and for parameter 8c-2c + 2c= 8c (LDY #0 not needed), however
;;; we loose 2*3c=6c for saving and restoring Y.
;;; 
;;; So the win is modest for args, but higher (maybe for other ops?)
;;; 
;;; All "flow-control" ops first need to FIXIPC, to include Y.
;;; 
        
;;; TODO: crashes with illegal OP!
        
.macro USEY
        sty     savey
.endmacro
        
.macro PARY
        lda     (_ivm_ipc),y
        tay
.endmacro
        
;;; leaves Y=0
.macro FIXIPC
        jsr addy
.endmacro       

        ;; time to wrap around - ONLY every 128 ops
addy:   
        tya
        clc
        adc     _ivm_ipc
        sta     _ivm_ipc
        bcc     :+
        inc     _ivm_ipc+1
:       
        ;; new ipc
        ldy     #0
        rts
        

_run_ivm_asm:   
        ;; new ipc
        ldy     #0
        beq     xnext           ; always

;;; ----- consume 1-byte operand -----
;;; if used parameter always need to savey
pnext:
        ldy     savey
        iny
        ;; if Y>=128 fold into ipc (matches comment above); else keep Y offset
        bpl     xnext
        FIXIPC                  ; ipc += Y, Y := 0
        jmp     xnext
        
;;; ----- main loop -----
ynext:  
        ldy     savey
xnext:
        lda     (_ivm_ipc),y

        .if TRACE
        sta     _ivm_op
        sty     savey
        jsr     _putchar
        ldy     savey
        lda     _ivm_op
        .endif ; TRACE

        iny

        ; X contains op-code when op is called
        tax

        ;; lobit - 0..127
        bmi     HIBIT

        lda     disphi,x
        sta     jmpvec+1

        lda     displo,x
        sta     jmpvec

        ;; not needed! that' the whole point!
;       sty     savey
        
        jmp     (jmpvec)


.else
        
.macro USEY
.endmacro

.macro PARY
;;; TODO: is already Y or correct, even here...
        ldy     #0

        lda     (_ivm_ipc),y
        tay
.endmacro

.macro FIXIPC
.endmacro
        
;; ----- consume 1-byte operand -----
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
ynext:                          ; dummy
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

        .if TRACE
        sta     _ivm_op
        jsr     _putchar
        lda     _ivm_op
        .endif ; TRACE

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

.endif
        
;;; ----- HIBIT: ASL → N = former bit6 -----
;;; On entry: A=op, X=op (from dispatch tax). ASL only for N/C test — keep X!
HIBIT:
        ;; 5
        asl     a               ; N=1 if original 0xC0..0xFF
        bmi     subvec          ; vector subroutine

;;; 0x80..0xBF: relative forward if ACC == 0
rjp:    
;;; 21c - 28c ?
        ;; X still = original op
        FIXIPC                  ; doesn't modify X, Y=0
        ;; if !acc then rjmp
        ;; +8c
        lda     _ivm_acc
        ora     _ivm_acc+1
        bne     xnext
        ;; +12c
        txa
        and     #$3F            ; delta

        clc
        adc     _ivm_ipc
        sta     _ivm_ipc

        ;; lo byte overflow?
        ;; +3-10c

        bcc     xnext
        inc     _ivm_ipc+1

        bcs     xnext

subvec:
;;; print trace nl
        ;; X = original op (NOT the ASL'd A — that was the vector-index bug)
        FIXIPC

        .if TRACE
        lda     #10     
        jsr     _putchar
        .endif ; TRACE
        
        ;; v = op & $3F
        ;; nfp = fptab[v]
        ;; mem[nfp+FR_PREV] = fp
        ;; mem[nfp+FR_RET]  = ipc
        ;; mem[nfp+FR_ACC]  = acc   (last param)
        ;; fp  = nfp
        ;; ipc = callv[v]
        ;; (inverse of op_X / op_semi)

        txa                     ; original op (kept across ASL)
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
        txa
        and     #$3F
        asl     a
        tay
        lda     (_ivm_callv),y
        sta     _ivm_ipc
        iny
        lda     (_ivm_callv),y
        sta     _ivm_ipc+1

        ;; new ipc
        ldy     #0

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
        USEY
        ;; 21c  10 B
        PARY

        lda     (_ivm_memhi),y
        tax
        lda     (_ivm_memlo),y
.endmacro

;;; opcodes 0..31: slot = op itself
op_31:  USEY
        txa                     ; op in X from dispatch (slot 0..31)
        tay
        LOAD_SLOT_Y

        jmp     ynext

;;; TODO: rename to 0x7f ??? or is that future nextpage?
jmp_done:       
op_q:   jmp     done


;;; return acc=0
op_Q:   lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1

;;; return acc
op_X:   FIXIPC
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
        lda     _ivm_fp                 ; TODO: why again? should INY?"
        clc
        adc     #FR_PREV
        tay
        lda     (_ivm_memlo),y
        sta     _ivm_fp

        ;; new IPC, reset messed up Y
        ldy     #0
        
        jmp     xnext

;;; return FUNNAME
op_semi:
        USEY
        lda     _ivm_fp
        ;; TODO: seems redundant, no harm doing the other stuff?
        cmp     #FP_MAIN
        beq     jmp_done

        ;; Loads FR_VAL index
        clc
        adc     #FR_VAL
        tay
        LOAD_SLOT_Y

        ldy     savey           ; for Y pc
        
        jmp     op_X


;;; arg 8 == last argument (ACC when called)
op_8:   USEY
        lda     _ivm_fp
        clc
        adc     #FR_ACC
        tay

        LOAD_SLOT_Y

        jmp     ynext

;;; args 1..7, except last is 8!
op_1to7:
        USEY
        txa                     ; op '1'..'7' in X from dispatch
        sec
        sbc     #'0'
        clc
        adc     #FR_ACC
        adc     _ivm_fp
        tay

        LOAD_SLOT_Y

        jmp     ynext


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
        ;; TODO: axpnext to store?
        sta     _ivm_acc
        stx     _ivm_acc+1

        jmp     pnext

op_S:   USEY
        PARY

        lda     _ivm_acc
        sta     (_ivm_memlo),y
        lda     _ivm_acc+1
        sta     (_ivm_memhi),y

        jmp     pnext

op_Z:   USEY
        PARY

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
        beq     op_acc1
        ;; fall-through to 0
;;; ACC = 0
op_0:   lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1

        jmp     xnext

;;; ACC = 1 (no operand)
op_acc1:
        lda     #1
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1

        jmp     xnext

;;; set 0/1 then pnext (consume PAR)
op_acc0p:
        lda     #0
        sta     _ivm_acc
        sta     _ivm_acc+1

        jmp     pnext

op_acc1p:
        lda     #1
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1

        jmp     pnext

;;; < = >  unsigned compare → 0/1
;;; TODO: just use them directly?
;;; no need to store in tmp, as CMP ADC SBC can be done directly
;;; using ADC (zp),Y so.... No need using PARAX....

;;; acc == mem[xx]
op_eq:  USEY
        PARY

        lda     (_ivm_memlo),y
        cmp     _ivm_acc
        bne     op_acc0p
        lda     (_ivm_memhi),y
        cmp     _ivm_acc+1
        bne     op_acc0p
        beq     op_acc1p

;;; acc < mem[xx]
op_lt:  USEY
        PARY

        lda     _ivm_acc
        cmp     (_ivm_memlo),y
        lda     _ivm_acc+1
        sbc     (_ivm_memhi),y
        bcc     op_acc1p
        bcs     op_acc0p

;;; acc > mem[xx]
op_gt:  USEY
        PARY

        lda     (_ivm_memlo),y
        cmp     _ivm_acc
        lda     (_ivm_memhi),y
        sbc     _ivm_acc+1
        bcc     op_acc1p
        bcs     op_acc0p



;;; Ixx  acc = ++mem[xx]
op_I:   USEY
        PARY

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
op_J:   USEY
        PARY

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
op_U:   USEY
        lda     (_ivm_ipc),y

        sta     tmp_lo              ; a
        iny
        USEY
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
:
        jmp     pnext

;;; Conditional GOTO +1B iff acc > 
;;; TODO: replace usage by rjmp, or make it an FUSED LOOP:
;;; TODO: Giiuuee (ii=index var, uu=upper limit pascal, ee== rjmp end)
op_G:
        FIXIPC                  ; Y=0
        sty     savey
        
        .if TRACE
        lda     #10     
        jsr     _putchar
        ldy     savey
        .endif ; TRACE
        
        ;; test if acc > 0 (i.e. not neg)
        lda     _ivm_acc+1      ; hi
        bmi     gskip
        ;; pos: is it > 0 ?
        ora     _ivm_acc
        beq     gskip

        ;; ok, acc > 0 (signed!)
        lda     (_ivm_ipc),y
        clc
        adc     _ivm_ipc
        sta     _ivm_ipc

        bcc     :+
        inc     _ivm_ipc+1
:       
        jmp     xnext
        
gskip:
        jmp     pnext


op_B:
        FIXIPC                  ; Y=0
        sty     savey
        
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


;;;  ops implemented here

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
op_dot: USEY
	jsr     _opputnum

        jmp     ynext

;;; newline
op_N:   USEY
	lda     #10
        jsr     _putchar
	;; TODO: jsr _nl

        jmp     ynext

;;; putchar ACC lo (or PAR?) — opcode 'O'
op_O:   USEY
	lda     _ivm_acc
        jsr     _putchar

        jmp     ynext

;;; " print string at ipc until 0
op_str:
	FIXIPC
strlp:
	ldy     #0		; each loop!
        lda     (_ivm_ipc),y
        beq     :+
        jsr     _putchar	; putchar trashes registers

	;; ipc++
        inc     _ivm_ipc
        bne     strlp
        inc     _ivm_ipc+1
        bne     strlp
:	
	;; Y=0 standing at \0
	jmp     pnext	


;;; ~ bitwise not
op_bnot:
        lda     _ivm_acc
        eor     #$FF
        sta     _ivm_acc
        lda     _ivm_acc+1
        eor     #$FF
        sta     _ivm_acc+1

        jmp     xnext


;;; [  acc <<= PAR   (ASL/ROL on ZP ok)
op_shl: PARAX
        tax                     ; count = par_lo
        beq     shl_dn
shllp:  asl     _ivm_acc
        rol     _ivm_acc+1
        dex
        bne     shllp

shl_dn: jmp     pnext

;;; ]  acc >>= PAR
op_shr: PARAX
        tax
        beq     shr_dn
shrlp:  lsr     _ivm_acc+1
        ror     _ivm_acc
        dex
        bne     shrlp

shr_dn: jmp     pnext

;;; {xx  mem[xx] <<= acc
;;; no ASL (zp),Y — load lo→tmp_lo, hi→A, count in X
op_shlm:
	USEY
	PARY
        sta     tmp_hi              ; store slot for reesult

        lda     (_ivm_memlo),y
        sta     tmp_lo              ; value lo
        lda     (_ivm_memhi),y      ; value hi in A

        ldx     _ivm_acc            ; count
        beq     shlm_st
shlm_lp:
        asl     tmp_lo
        rol     a
        dex
        bne     shlm_lp
shlm_st:
	;; TODO: should it really modify variable or leave in ACC?
        ldy     tmp_hi
        sta     (_ivm_memhi),y
        lda     tmp_lo
        sta     (_ivm_memlo),y

        jmp     pnext

;;; }xx  mem[xx] >>= acc
op_shrm:
	USEY
	PARY
        sta     tmp_hi

        lda     (_ivm_memlo),y
        sta     tmp_lo
        lda     (_ivm_memhi),y
        ldx     _ivm_acc
        beq     shrm_st
shrm_lp:
        lsr     a
        ror     tmp_lo
        dex
        bne     shrm_lp
shrm_st:
	;; TODO: should it really modify variable or leave in ACC?
        ldy     tmp_hi
        sta     (_ivm_memhi),y
        lda     tmp_lo
        sta     (_ivm_memlo),y

        jmp     pnext


;;; unknown: print % + op + NL, halt
;;; op is in X from dispatch (do not use _ivm_op)
op_bad:	FIXIPC
;;; TODO: print address Y ? and byte code value
        lda     #10
        jsr     _putchar

        lda     #'%'
        jsr     _putchar

        txa                     ; opcode still in X
        jsr     _putchar

        lda     #10
        jsr     _putchar

	;; fall-through

;;; C style return ACC in AX
done:   lda     _ivm_acc
        ldx     _ivm_acc+1

        rts
