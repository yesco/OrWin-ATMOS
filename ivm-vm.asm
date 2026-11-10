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
        
;; ALTCODE: 1.5 % faster...
;ALTCODE=0
ALTCODE=1

OPFOR=3 ;; new one works i,n only
;OPFOR=0
;OPFOR=1 ;; hand coded maybve works for ALTCODE

FASTDISPATCH=1


.export         _run_ivm_asm

.importzp       _ivm_ipc, _ivm_fp, _ivm_acc, _ivm_op
.importzp       _ivm_memlo, _ivm_memhi, _ivm_par
.importzp       _ivm_fptab, _ivm_callv

.import         _opmul, _opdiv, _opmod, _opputnum
.import         _putchar, _getchar
;.import         _malloc, _free
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

savey:  .res 1
        

;;; CODESIZE: (- _IVM_ASM_END _IVM_ASM_START)
;;; =========================================
;;;    (- #x7ee #x24b) = 1443 with DATALIGN and FASTDISPATCH

.code
.export _IVM_ASM_START
_IVM_ASM_START:	

;;  TODO: move the most common/expensive OP that tkaes a parameter here!


.if ALTCODE
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
        
;;; TODO: one case PARY without leading USEY, lol
.macro PARY
        lda     (_ivm_ipc),y
        tay
.endmacro
        

FIXINLINE=1

.ifdef FIXINLINE
	
;;; leaves Y=0
.macro FIXIPC
;;; 12 b - 15c
        tya
        clc
        adc     _ivm_ipc
        sta     _ivm_ipc
        bcc     :+
        inc     _ivm_ipc+1
:       
        ;; new ipc
        ldy     #0
.endmacro       

.else

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

.endif ; FIXINLINE
	
	

_run_ivm_asm:   
        ;; new ipc
        ldy     #0
        beq     xnext           ; always

anext:
;;; 10c (17c 19c)
	clc
	adc     savey
	tay
	bcc     pfixnext
	;; a bit clumsy
	inc     _ivm_ipc+1
	bpl     pfixnext
	bne     xnext
	

;;; xx11 b  11c (18c)
;;; Y stays same! (safe)
	clc
	adc     _ivm_ipc
	sta     _ivm_ipc
	bcc     xnext
	inc     _ivm_ipc+1
	bne     xnext

;;; ----- consume 1-byte operand -----
;;; if used parameter always need to savey
pnext:
        ldy     savey
        iny
        ;; if Y>=128 fold into ipc (matches comment above); else keep Y offset
;;; TODO: 5-10% (2c spent testing here, maybe inject an fixop)
pfixnext:
        bpl     xnext
        FIXIPC                  ; ipc += Y, Y := 0
        jmp     xnext
        
;;; ----- main loop -----
ynext:  
        ldy     savey

xnext:
;;; FAST = (+ 5 2 2 2 11) = 22c (can remove 2c)
        lda     (_ivm_ipc),y

	        .if TRACE
	        sta     _ivm_op
	        sty     savey

		lda     #'.'
	        jsr     _putchar

		lda     _ivm_op
	        jsr     _putchar

	        ldy     savey
	        lda     _ivm_op
	        .endif ; TRACE

        iny

        ; X contains op-code when op is called
        tax

        ;; lobit - 0..127
        bmi     HIBIT

.if FASTDISPATCH
;;; 7 B  11c
	asl
	sta     pc+1
pc:	jmp     (dispvec)

.else
;;; 
        lda     disphi,x
        sta     jmpvec+1

        lda     displo,x
        sta     jmpvec
        jmp     (jmpvec)
.endif

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

anext:
;;; 11 b  11c (18c)
	clc
	adc     _ivm_ipc
	sta     _ivm_ipc
	bcc     xnext
	inc     _ivm_ipc+1
	bne     xnext
	
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
        
;;; FAST = (+ 7 8 4 11 3) = 33 c
        ;; 7c
        ldy     #0
        lda     (_ivm_ipc),y

	        .if TRACE
		sty     savey
	        sta     _ivm_op

		lda     #' '
		jsr     _putchar

		lda     _ivm_op
	        jsr     _putchar
	

	        lda     _ivm_op
		ldy     savey
	        .endif ; TRACE

        ;; 8c (12c)
        inc     _ivm_ipc
        bne     :+
        inc     _ivm_ipc+1
:

        ;; lobit - OPCODE DISPATCH (0..127)
        ;; 4c
;;; TODO: not needed do ASL / BCS instead, save 2 cycles
;;;   BUT: then OP is in A<<2 and not in X
	tax

        bmi     HIBIT


.if FASTDISPATCH
;;; 7 B  11c
	asl
	sta     pc+1
pc:	jmp     (dispvec)

.else
;;; 15 B  19c
        ;; +18c
        lda     disphi,x
        sta     jmpvec+1

        lda     displo,x
        sta     jmpvec

        jmp     (jmpvec)
.endif ; FASTDISPATCH

.endif ; ALTCODE=2
        
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

;        .if TRACE
;        lda     #10     
;        jsr     _putchar
;        .endif ; TRACE
        
        ;; v = op & $3F
        ;; nfp = fptab[v]
        ;; mem[nfp+FR_PREV] = fp
        ;; mem[nfp+FR_RET]  = ipc
        ;; mem[nfp+FR_ACC]  = acc   (last param)
        ;; fp  = nfp
        ;; ipc = callv[v]
        ;; (inverse of op_X / op_semi)

        txa                     ; original op (kept across ASL)

	;; get new frame pointer
        and     #$3F
        tay                     ; Y = v
        lda     (_ivm_fptab),y
        sta     tmp_lo          ; nfp

	;; -- Linear copying
	;; (/ (- 2752774 2732460) 2752774.0) = 0.74% faster
	
	;; FR_RET:0 = ipc (return address)
	tay
        lda     _ivm_ipc
        sta     (_ivm_memlo),y

        lda     _ivm_ipc+1
        sta     (_ivm_memhi),y

	;; skip FR_VAL:1 (actually only there for pascal?)
	iny
	
        ;; FR_PREV:2 = old fp
	iny
        lda     _ivm_fp
        sta     (_ivm_memlo),y

        lda     #0
        sta     (_ivm_memhi),y

        ;; FR_ACC:3 = acc (last parameter)
	iny
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
	
	jmp     op_X

;;; return FUNNAME ('; val; fall into op_X)
op_semi:
        USEY
        ;; Loads FR_VAL index
        lda     _ivm_fp
        clc
        adc     #FR_VAL
        tay
        LOAD_SLOT_Y

        ldy     savey           ; for Y pc
        
	;; fall-through (0.1% faster than having op_Q fallthrough)
	;; (/ (- 2732460 2729364) 2732460.0)
	

;;; return acc — FR_RET is always valid (_start ends with q after Vmain)
op_X:   FIXIPC

	;; -- linear load
	;; (/ (- 2752774 2713089) 2752774.0) 1.4% ;; ALTCODE= 0 TOTAL

        ;; restore IPC from FR_RET:0
        ldy     _ivm_fp
        lda     (_ivm_memlo),y
        sta     _ivm_ipc

        lda     (_ivm_memhi),y
        sta     _ivm_ipc+1

        ;; restore FP from FR_PREV
        iny
        iny
        lda     (_ivm_memlo),y
        sta     _ivm_fp

	;; new IPC
        ldy     #0

        jmp     xnext



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

;;; C dest src — mem[dest]=mem[src]; ACC=value
op_C:   lda     (_ivm_ipc),y
        sta     tmp_hi              ; dest
        iny
	USEY
        lda     (_ivm_ipc),y
        tay                         ; src

        lda     (_ivm_memlo),y
        sta     tmp_lo
        lda     (_ivm_memhi),y
        tax                         ; hi

        ldy     tmp_hi              ; dest
        lda     tmp_lo
        sta     (_ivm_memlo),y
        sta     _ivm_acc
        txa
        sta     (_ivm_memhi),y
        sta     _ivm_acc+1

.if .not(ALTCODE)
        inc     _ivm_ipc
        bne     :+
        inc     _ivm_ipc+1
:
.endif
        jmp     pnext


;;; ============================================================
;;; Byte / pointer / indexed memory ops (added for C parity)
;;; ============================================================

;;; Mvv - malloc - (TODO: zero clean" == calloc)
op_M:   USEY

	;; get and save var index
	lda (_ivm_ipc),y
	sta tmp_lo

	;; load size
	lda _ivm_acc
	ldx _ivm_acc+1
	
	jsr _ivm_zalloc
	
	sta _ivm_acc
	stx _ivm_acc+1
	
	ldy tmp_lo
	sta (_ivm_memlo),y
	txa
	sta (_ivm_memhi),y
	
	jmp pnext

;;; Fvv - free
op_F:   USEY

	;; get and save var index
	lda (_ivm_ipc),y
	tay

	;; load pointer
	lda (_ivm_memhi),y
	tax
	lda (_ivm_memlo),y
	
	jsr _ivm_free
	
	lda #0
	sta (_ivm_memlo),y
	sta (_ivm_memhi),y
	
	jmp pnext

	

;;; lxx  ACC = low byte of slot xx  (hi := 0)
op_l:   USEY
        PARY
        lda     (_ivm_memlo),y
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1

        jmp     pnext

;;; sxx  slot xx := ACC & 0xff  (hi := 0)
op_s:   USEY
        PARY
        lda     _ivm_acc
        sta     (_ivm_memlo),y
        lda     #0
        sta     (_ivm_memhi),y

        jmp     pnext

;;; rxx  ACC = *(uchar*)PAR
op_r:   PARAX                       ; A=addr_lo, X=addr_hi
        sta     tmp_lo
        stx     tmp_hi
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1

        jmp     pnext

;;; wxx  *(uchar*)PAR = ACC & 0xff
op_w:   PARAX
        sta     tmp_lo
        stx     tmp_hi
        ldy     #0
        lda     _ivm_acc
        sta     (tmp_lo),y

        jmp     pnext

;;; tick - 'xx  ACC = *(uchar*)ptr;  ptr++   (ptr lives in slot xx)
op_ri:  USEY
        PARY                        ; Y = slot
        tya
        tax                         ; X = slot
        lda     (_ivm_memlo),y
        sta     tmp_lo
        lda     (_ivm_memhi),y
        sta     tmp_hi              ; tmp = current ptr
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        lda     #0
        sta     _ivm_acc+1
        ;; ptr++
        inc     tmp_lo
        bne     :+
        inc     tmp_hi
:
        ;; write ptr back
        txa
        tay
        lda     tmp_lo
        sta     (_ivm_memlo),y
        lda     tmp_hi
        sta     (_ivm_memhi),y

        jmp     pnext

;;; comma - ,xx  *(uchar*)ptr = ACC;  ptr++
op_wi:  USEY
        PARY
        tya
        tax                         ; X = slot
        lda     (_ivm_memlo),y
        sta     tmp_lo
        lda     (_ivm_memhi),y
        sta     tmp_hi
        ldy     #0
        lda     _ivm_acc
        sta     (tmp_lo),y
        ;; ptr++
        inc     tmp_lo
        bne     :+
        inc     tmp_hi
:
        txa
        tay
        lda     tmp_lo
        sta     (_ivm_memlo),y
        lda     tmp_hi
        sta     (_ivm_memhi),y

        jmp     pnext

;;; @xx  ACC = *(ivmvar*)PAR     (word load via pointer in slot)
op_at:  PARAX
        sta     tmp_lo
        stx     tmp_hi
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        iny
        lda     (tmp_lo),y
        sta     _ivm_acc+1

        jmp     pnext

;;; !xx  *(ivmvar*)PAR = ACC
op_bang: PARAX
        sta     tmp_lo
        stx     tmp_hi
        ldy     #0
        lda     _ivm_acc
        sta     (tmp_lo),y
        iny
        lda     _ivm_acc+1
        sta     (tmp_lo),y

        jmp     pnext

;;; (aa ii   ACC = base[index]   (word elements, index*=2)
op_idxl:
        USEY
        ; ipc[0]=aa (base slot), ipc[1]=ii (index slot)
        ldy     #0
        lda     (_ivm_ipc),y        ; aa
        tax
        iny
        lda     (_ivm_ipc),y        ; ii
        tay
        ; index *= 2
        lda     (_ivm_memlo),y
        asl     a
        sta     tmp_lo
        lda     (_ivm_memhi),y
        rol     a
        sta     tmp_hi
        ; base + (index*2)
        txa
        tay
        clc
        lda     (_ivm_memlo),y
        adc     tmp_lo
        sta     tmp_lo
        lda     (_ivm_memhi),y
        adc     tmp_hi
        sta     tmp_hi
        ; load word
        ldy     #0
        lda     (tmp_lo),y
        sta     _ivm_acc
        iny
        lda     (tmp_lo),y
        sta     _ivm_acc+1
        ; consume 2 operands
.if ALTCODE
        ldy     savey
        iny
        iny

        jmp     xnext
.else
        inc     _ivm_ipc
        bne     :+
        inc     _ivm_ipc+1
:
        jmp     pnext               ; pnext does the second ++
.endif

;;; )aa ii   base[index] = ACC
op_idxs:
        USEY
        ldy     #0
        lda     (_ivm_ipc),y        ; aa
        tax
        iny
        lda     (_ivm_ipc),y        ; ii
        tay
        ; index *= 2
        lda     (_ivm_memlo),y
        asl     a
        sta     tmp_lo
        lda     (_ivm_memhi),y
        rol     a
        sta     tmp_hi
        ; base + (index*2)
        txa
        tay
        clc
        lda     (_ivm_memlo),y
        adc     tmp_lo
        sta     tmp_lo
        lda     (_ivm_memhi),y
        adc     tmp_hi
        sta     tmp_hi
        ; store ACC
        ldy     #0
        lda     _ivm_acc
        sta     (tmp_lo),y
        iny
        lda     _ivm_acc+1
        sta     (tmp_lo),y
        ; consume 2 operands
.if ALTCODE
        ldy     savey
        iny
        iny

        jmp     xnext
.else
        inc     _ivm_ipc
        bne     :+
        inc     _ivm_ipc+1
:
        jmp     pnext
.endif





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
	;; (there is no inc (),y)

        ;; lo += 1
        clc
        lda     (_ivm_memlo),y
        adc     #1
        sta     (_ivm_memlo),y
        sta     _ivm_acc

        ;;  hi += Carry
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
	;; read first parameter
        lda     (_ivm_ipc),y

        sta     tmp_hi              ; a
        iny
	sty     savey

        lda     (_ivm_ipc),y
        tay                         ; b
        
        lda     (_ivm_memlo),y
        sta     tmp_lo              ; B.lo

        lda     (_ivm_memhi),y
        tax                         ; B.hi

        ldy     tmp_hi		; a
        clc
        lda     (_ivm_memlo),y	; A
        adc     tmp_lo		; +B.lo
        sta     (_ivm_memlo),y	; A.lo   := A.lo + B.lo
        sta     _ivm_acc	; ACC.lo := A.lo

        txa			; B.hi
        adc     (_ivm_memhi),y
        sta     (_ivm_memhi),y	; A.hi   := B.hi + A.hi
        sta     _ivm_acc+1	; ACC.hi := A.hi

.if .not(ALTCODE)
        ; consume 1 operand bytes
        inc     _ivm_ipc
        bne     :+
        inc     _ivm_ipc+1
:
.endif
        jmp     pnext 		; this does the second



;;; conditional Goto: if (acc > 0) ipc += delta; ALWAYS pnext to skip delta
;;; (matches C: if (acc>0) ipc+=*ipc; goto pnext;)
;;; FIXIPC → ipc at delta, Y=0. sty savey so pnext's ldy savey/iny works.
;;; 
;;; TODO: replace usage by rjmp, or make it an FUSED LOOP:
;;; TODO: Giiuuee (ii=index var, uu=upper limit pascal, ee== rjmp end)

.if ALTCODE

op_G:
        ;; test if acc > 0 (signed: not neg and not zero)
        lda     _ivm_acc+1      ; hi
        bmi     gskip
        ;; pos: is it > 0 ?
        ora     _ivm_acc
        beq     gskip
	
gyes:	
        ;; ok: acc > 0: ipc += delta (ipc still points at delta)
	FIXIPC
	PARY

gskip:	
	;; skip parameter (or 0th jump byte)
	iny
        jmp     xnext           ; both paths: consume delta

.else ; !ALT_CODE
	
op_G:
        FIXIPC                  ; ipc at delta, Y=0
        sty     savey           ; savey=0
        
;        .if TRACE
;        lda     #10     
;        jsr     _putchar
;        ldy     savey
;        .endif ; TRACE
        
        ;; test if acc > 0 (signed: not neg and not zero)
        lda     _ivm_acc+1      ; hi
        bmi     gskip
        ;; pos: is it > 0 ?
        ora     _ivm_acc
        beq     gskip

        ;; ok: acc > 0: ipc += delta (ipc still points at delta)
        lda     (_ivm_ipc),y
        clc
        adc     _ivm_ipc
        sta     _ivm_ipc
        bcc     gskip
        inc     _ivm_ipc+1
gskip:
        jmp     pnext           ; both paths: consume delta


.endif ; !ALT_CODE
	
	
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

	

;;; ------------------------------------------------------------
;;; i v u oo   — counting-up for test (at top)
;;;   acc = mem[v]
;;;   if (acc <= mem[u])  ipc += 3;          // enter body
;;;   else                ipc += 2 + oo;     // exit (G-style)
;;; ------------------------------------------------------------

;;; ------------------------------------------------------------
;;; n back  — next of counting-up for
;;;   tpc = ipc - back + 1     ; → address of the 'i'
;;;   acc = ++mem[tpc[1]]       ; v
;;;   if (acc > mem[tpc[2]]) fall out
;;;   else ipc = tpc + 4        ; body
;;; ------------------------------------------------------------

;;; (- 9345 6577) = 2768 enabled OPFOR=2
;;; (- 9201 6577) = 2624 OPFOR=0
;;; (- 2768 2624) = 144 bytes (x 2 for j m)



;;; ONLY FOR ALTCODE
.if (OPFOR=3) .and (ALTCODE=1)

.if 1

op_i:
        ;; Entry: Y points to arg 1 ('v'). No FIXIPC needed!
        sty     savey
        lda     (_ivm_ipc),y
        tay
        LOAD_SLOT_Y             ; _ivm_acc = mem[v]
        ldy     savey           ; Y = offset of 'v'
        jmp     i_skip_load

op_i_step:
        ;; Entry from op_n: _ivm_ipc points to 'v', so Y = 0!
	;; Y=0 points to 'v'

	;; AX = mem[v];
        ldy     #0            
        lda     (_ivm_ipc),y
        tay
        lda     (_ivm_memhi),y
        tax
        lda     (_ivm_memlo),y

	;; AX++;
        clc
        adc     #1
        bcc     :+
        inx
:
	;; acc = AX;
        sta     _ivm_acc
        stx     _ivm_acc+1

	;; mem[v] = AX
        sta     (_ivm_memlo),y
        txa
        sta     (_ivm_memhi),y

	;; "restore Y"
	ldy     #0

i_skip_load:
	;; get u parameter memory location
        iny
	sty     savey
        lda     (_ivm_ipc),y
        tay

	;; acc <= mem[u]
        lda     (_ivm_memlo),y
        cmp     _ivm_acc
        lda     (_ivm_memhi),y
        sbc     _ivm_acc+1
        bcc     i_exit

i_enter:
	;; go to next op (skip jump offset)
        ldy     savey
	iny
        jmp     xnext

i_exit:
	;; fail, exit loop read and add offset
        ldy     savey
        iny
        lda     (_ivm_ipc),y
	
        jmp     anext


op_n:
;;; 10c - FAST JUMP if started at Y=0 !
        tya
        sec
	;; delta = (Y - offset arg)
        sbc     (_ivm_ipc),y 
	;; if equal go next step where Y will be RESET to 0
        beq     op_i_step

op_n_do_the_math:
	;; Y = delta, don't need update ipc low
        bcs     :+
        dec     _ivm_ipc+1
:
	tay
	FIXIPC

        jmp     op_i_step

.else

op_i:
;;; This entry point is only used the first time.
;;; During the loop it enters from op_n in op_i_step

;;; 74 B 

	;; load acc = mem[v]
	;; We normalize here so that Y == 0 meaning
	;; next value back jump == Y at that time, and
	;; can super-fast jump back without math
	;; PARY - LDY #0
	USEY
	lda (_ivm_ipc),y
	tay
        LOAD_SLOT_Y

	ldy savey
;;; TODO: jump back shortcut wrong (when Y=0 here)
;        FIXIPC                  ; Y=0 ; 
	jmp     i_skip_load

;;; Entry point from op_n! saves bytes
op_i_step:
;;; --- 25 b 46c
	;; PARY ?

	;; 7 i var
	ldy #2
	lda (_ivm_ipc),y
	tay

	;; 
	lda (_ivm_memhi),y
	tax
	lda (_ivm_memlo),y

	;; +1
	clc
	adc #1
	bcc :+
	inx
:	

	sta _ivm_acc
	stx _ivm_acc+1

	sta (_ivm_memlo),y
	txa
	sta (_ivm_memhi),y

	ldy #2

;;; We want to compare acc <= mem[u]
op_i_cmp:

i_skip_load:	
	iny
        lda     (_ivm_ipc),y
        tay

	;; acc <= mem[u] ?
	;; NAH: NOT( mem[u] < acc )
        lda     (_ivm_memlo),y
        cmp     _ivm_acc
        lda     (_ivm_memhi),y
        sbc     _ivm_acc+1
        bcc     i_exit

i_enter:
	iny
	jmp pnext

.if 0
i_exit:
        ldy     savey
        iny                     ; Y = offset 3 (exit_offset byte)
        sty     savey           ; savey = offset 3
        lda     (_ivm_ipc),y    ; A = exit_offset ($02)
        clc
        adc     #1              ; +1 skips exit_offset byte -> 3 + 1 + 2 = 6 (Le8)!
        jmp     anext
.else
i_exit:
	ldy     savey
	iny
        lda     (_ivm_ipc),y        ; oo
	sec
	adc     savey
	FIXIPC

	jmp xnext
.endif
	


;;; (for) next - dispatches to a looping-step-compare op
;;; like i<=N !j k<N
op_n:
;;; 10c  5 B
	tya
	;; add offset 2 for this instruction
	clc
	adc #2

	sec
	sbc (_ivm_ipc),y
	;; We're where we can just set Y=0 (== go back beginning loop!)
	beq op_i_step  		; always (and at dest set Y=0)

op_n_do_the_math:	
	;; we're not the inner loop - pay the cost
	;; ipc = ipc + y - back + 2

	;; let's assume is safe? LOL
	bcs :+
	dec _ivm_ipc+1
:	
;;; TODO: would be good if these two were part of the displacement!
	;; add 2 (length of instruction)
	clc
	adc #2
	bcc :+
	inc _ivm_ipc+1
:	
	clc
	adc _ivm_ipc
	bcc op_i_step
	inc _ivm_ipc+1
;;; TODO: specialized dispatch depending on receiver op!
	jmp op_i_step

.endif

.endif ; OPFOR=3



.if (OPFOR=2) .and (ALTCODE=0)
;;; 34 Bytes saved compared to OPFOR=1


;;; TODO: only works if NOT ALTCODE

;;; ALL CHANGES:
;;;   saves 24 bytes, lol
;;;  (- 1 (/ 4866569 4926832.0)) = 1.2% faster !
;;;  (- 1 (/ 4806464 4926832.0)) = 2.44% (upfront load ax)
op_i:
;;; This entry point is only used the first time.
;;; During the loop it enters from op_n in op_i_step

;;; 74 B 

	;; load acc = mem[v]
.if ALTCODE
	;; We normalize here so that Y == 0 meaning
	;; next value back jump == Y at that time, and
	;; can super-fast jump back without math
	iny
        FIXIPC                  ; Y=0 ; 
	;; PARY - LDY #0
	lda (_ivm_ipc),y
	tay
.else
	USEY
	PARY			
.endif
        LOAD_SLOT_Y

	jmp     i_skip_load

;;; Entry point from op_n! saves bytes
op_i_step:
;;; --- 25 b 46c

	PARAX			; 10 B  21c

	;; +1
	clc
	adc     #1
	bcc     :+
	inx
:	

	sta     _ivm_acc
	stx     _ivm_acc+1

	sta     (_ivm_memlo),y
	txa
	sta     (_ivm_memhi),y

;;; We want to compare acc <= mem[u]
op_i_cmp:

i_skip_load:	
        ldy     #1
        lda     (_ivm_ipc),y
        tay

	;; acc <= mem[u] ?
	;; NAH: NOT( mem[u] < acc )
        lda     (_ivm_memlo),y
        cmp     _ivm_acc
        lda     (_ivm_memhi),y
        sbc     _ivm_acc+1
        bcc     i_exit

i_enter:
	lda     #3
	jmp     anext

i_exit:
        ldy     #2
        lda     (_ivm_ipc),y        ; oo

;;; TODO: can forward jumps be made to take this into account!
;;; 7 B  7c (11c) - but only save during exit loop
        clc
	adc     #3
	bcc     :+
	inc     _ivm_ipc+1
:	
	jmp     anext


;;; (for) next - dispatches to a looping-step-compare op
;;; like i<=N !j k<N
op_n:
.if ALTCODE=1
;or... 12c ... 
;;; On entry: X holds the opcode ($C0..$FF), Y holds current loop offset
op_n_short:
;;; 10c  5 B
	tya
	cmp (_ivm_ipc),y
	;; We're where we can just 
	beq op_i_step  		; always (and at dest set Y=0)
.endif

op_n_compute:	
	;; we're not the inner loop - pay the cost
;;; 23 B  27c f+ this: (FIXIPC + USEY: 12 b  15c +  2 b  3c)
	;; basically op_B except the jmp next
	;; is shortcut to inside op_i
        FIXIPC                  ; Y=0 ; 
	USEY

	ldy     #0
        lda     _ivm_ipc
        sec
        sbc     (_ivm_ipc),y
        bcs     :+
        dec     _ivm_ipc+1
:	
;;; TODO: look at adjusting \m .. B so 
;;;   no need add 2
;;; 7 B  7c (11c) TODO: used for EVERY LOOP!
	clc
	adc     #2 		; our own length!
	bcc     :+
	inc     _ivm_ipc+1
:	

        sta     _ivm_ipc

;;; TODO: specialized dispatch depending on receiver op!
	jmp     op_i_step


.endif ; OPFOR=2
	


.if OPFOR=1
;;;  TODO: does it work?
;;; 	- no, it was mean to work for ALTCODE=1
	
;;; 70 B to much!
op_i:
        FIXIPC                  ; Y = 0, ipc at first operand
        sty     savey

;PARAX ???
        ;; load v → acc  (and keep slot in tmp_lo)
        lda     (_ivm_ipc),y    ; v
        sta     tmp_lo
        tay
        LOAD_SLOT_Y             ; acc = mem[v]

        ;; get Y = u
        ldy     #1
        lda     (_ivm_ipc),y    ; u slot
        tay

	;; acc <= mem[u]
        lda     _ivm_acc
        cmp     (_ivm_memlo),y
        lda     _ivm_acc+1
        sbc     (_ivm_memhi),y

        bcc     i_enter         ; acc <  mem[u]
        bne     i_exit          ; acc >  mem[u]
        ;; equal → also enter

.if ALTCODE
i_enter:
        ;; ipc += 3  (skip v u oo) → body
	iny
	iny
	iny

	jmp     xnext

i_exit:
	;; ipc += 2 + oo   (from current pos = v)
	iny
	;; pretend we're executing 'G' (or jr) !
	jmp     gyes

.else
i_enter:
        ;; ipc += 3  (skip v u oo) → body
        clc

        lda     _ivm_ipc
        adc     #3
        sta     _ivm_ipc
        bcc     :+
        inc     _ivm_ipc+1
:
        jmp     xnext

i_exit:
        ;; ipc += 2 + oo   (from current pos = v)
        clc
        ldy     #2
        lda     (_ivm_ipc),y    ; oo

        adc     #2
        adc     _ivm_ipc
        sta     _ivm_ipc
        bcc     :+
        inc     _ivm_ipc+1
:
        jmp     pnext           ; consume the last (already added) byte? 
                                ; actually the +2 already skipped v+u,
                                ; pnext will skip one more — adjust if needed

.endif ; ALTCODE
	
	
;;;  88 B !!! too much!
op_n:
        FIXIPC
        sty     savey

        ;; tpc = ipc - *ipc + 1
        lda     _ivm_ipc
	sec
        sbc     (_ivm_ipc),y    ; - delta
        sta     tmp_lo
        lda     _ivm_ipc+1
        sbc     #0
        sta     tmp_hi
        ;; +1
        inc     tmp_lo
        bne     :+
        inc     tmp_hi
:

	;; acc = (tmp_lo) Y=0 still
	PARY

	lda     (tmp_lo),y	; lo
	clc
	adc     #1
	sta     _ivm_acc
	tax
	
	iny
	lda     (tmp_lo),y	; hi
	adc     #0
	sta     _ivm_acc

	;;  Y=1
	PARY

	;; acc <= mem[u]
	txa
	cmp     (_ivm_memlo),y
	lda     _ivm_acc+1
	sbc     (_ivm_memhi),y
	
	bcc     n_enter
	bne     n_exit
	
        ;; equal → also enter

.if ALTCODE
n_enter:
	lda     tmp_lo
	sta     _ivm_ipc
	lda     tmp_hi
	sta     _ivm_ipc+1

	;; ipc += 2 + oo   (from current pos = v)
	ldy     #1
	;; pretend we're executing 'B' (or jr) !
	jmp     op_B

n_exit:
	jmp     pnext
	
.else
n_enter:
        ;; ipc += 3  (skip v u oo) → body
        clc

        lda     _ivm_ipc
        adc     #3
        sta     _ivm_ipc
        bcc     :+
        inc     _ivm_ipc+1
:
        jmp     xnext

n_exit:
        ;; ipc += 2 + oo   (from current pos = v)
        clc
        ldy     #2
        lda     (_ivm_ipc),y    ; oo

        adc     #2
        adc     _ivm_ipc
        sta     _ivm_ipc
        bcc     :+
        inc     _ivm_ipc+1
:
        jmp     pnext           ; consume the last (already added) byte? 
                                ; actually the +2 already skipped v+u,
                                ; pnext will skip one more — adjust if needed
.endif ; ALTCODE

.endif ; OPFOR=1
	

;;; TODO:
	
;;; j v oo   — zero-test form
;;; m delta  — next (does --)
;;; (same structure, lengths 3 / 1)
;;; ------------------------------------------------------------
;op_j:
        ; analogous to op_i but only one limit test (acc != 0)
        ; enter: ipc += 2
        ; exit:  ipc += 1 + oo

;op_m:
        ; analogous to op_n but -- and length 3
	
	


;;; NOP - SPACE
op_sp:
;	lda     '.'
;	jsr     _putchar
	jmp     xnext


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

;;; getchar(ACC) - opcode 'K'
op_K:	USEY
	jsr    	_getchar
	sta     _ivm_acc
	lda     #0
	sta     _ivm_acc+1
	
	jmp     ynext
	
;;; newline
op_N:   USEY
	lda     #10
        jsr     _putchar
	;; TODO: jsr _nl

        jmp     ynext

;;; putchar(ACC) — opcode 'E'
op_E:   USEY
        lda     _ivm_acc
        jsr     _putchar

        jmp     ynext

;;; 'O:' - inline putchar
op_O:   USEY
	lda     (_ivm_ipc),y
        jsr     _putchar

        jmp     pnext


;;; TOOD: P print string from ACC
op_P:
	USEY
:	
	ldy     #0		; each loop!
        lda     (_ivm_acc),y
        beq     :+
        jsr     _putchar	; putchar trashes registers

	;; ipc++
        inc     _ivm_acc
        bne     :-
        inc     _ivm_acc+1
        bne     :-
:	
	jmp 	ynext
	

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
.if ALTCODE
	iny

	jmp     xnext
.else
	jmp     pnext
.endif	

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
;;; undefined opcodes — named aliases of op_bad
op_hash:
op_dollar:
op_colon:

op_A:
op_D:

op_H:


op_R:
op_T:

op_V:
op_W:

op_Y:

op_bslash:
op__:
op_tick:

;;; TODO: single opcode save local
op_a:
op_b:
op_c:
op_d:
op_e:
op_f:
op_g:
op_h:

op_j:
op_k:

.if (OPFOR=0)
op_i:	
op_n:	
.endif
	
op_m:
op_o:
op_p:

op_t:
op_u:
op_v:

;;; TODO: call library routine (see ivm.c)
op_x:
op_y:
op_z:

op_del:

op_bad:
	stx     tmp_lo      	; save op code
	
	FIXIPC
;;; TODO: print address, and byte code value
        lda     #10
        jsr     _putchar

        lda     #'%'
        jsr     _putchar

        lda     tmp_lo          ; op code
        jsr     _putchar

        lda     #10
        jsr     _putchar

	;; fall-through

;;; C style return ACC in AX
done:   lda     _ivm_acc
        ldx     _ivm_acc+1

        rts

.export _IVM_ASM_END
_IVM_ASM_END:	
	
	
	
	
	
;;  (/ 199954374 198149017.0) 0.91% faster w pagealign table
.segment "ALIGNDATA"
;.data
	
.align 256, $42
	
;;; static dispatch tables (256 bytes) — no runtime init_disp

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
  op_X, op_Y, op_Z, op_shl, op_bslash, op_shr, op_xor, op__, \
  op_tick, op_a, op_b, op_c, op_d, op_e, op_f, op_g, \
  op_h, op_i, op_j, op_k, op_l, op_m, op_n, op_o, \
  op_p, op_q, op_r, op_s, op_t, op_u, op_v, op_w, \
  op_x, op_y, op_z, op_shlm, op_or, op_shrm, op_bnot, op_del

.if FASTDISPATCH
	
  .export _dispvec
  _dispvec:	

  dispvec:
    .word JumpTable

.else
	
  .export _displo
  _displo:	

  displo:
    .lobytes JumpTable
  disphi:	
    .hibytes JumpTable

.endif
