;;; 1) Don't change any line if not needed.
;;; 2) Don't remove comments.
	
.export _run_ivm
.import _main_entry, _frame_base, _vptr, _mem, _putchar, _printf

	.zeropage
vm_ipc:     .res 2      ; Virtual Instruction Pointer (16-bit ZP pointer)
vm_acc:     .res 2      ; 16-bit Virtual Accumulator (Low: vm_acc, High: vm_acc+1)
vm_fp:      .res 1      ; Virtual Frame Pointer (8-bit index)
tmp:        .res 2      ; Scratchpad for jump tables / expressions
vm_mem:     .res 2      ; Pointer to active process memory space (16-bit ZP pointer)

	FP_MAIN     = $00
	FR_VAL      = 0
	FR_RET      = 1
	FR_PREV     = 3

	.segment "CODE"

;;; -----------------------------------------------------------------------------
;;; unsigned int run_ivm(void);
;;; -----------------------------------------------------------------------------
_run_ivm:
	;; Enter main as if it was a function
	lda _main_entry
	lda _main_entry+1
	sta vm_ipc
	sta vm_ipc+1
	
	;; Initialize our indirect process memory base address
	lda _mem
	sta vm_mem
	lda _mem+1
	sta vm_mem+1

	lda #0
	sta vm_acc
	sta vm_acc+1
	sta vm_fp

	jmp dispatch_xnext

;;; -----------------------------------------------------------------------------
;;; Dispatch Entry Points (Fall-through architecture)
;;; -----------------------------------------------------------------------------

dispatch_pnext:
;;; 10-12c  6 B  ; pnext: Advance IPC past the 1-byte parameter
	inc vm_ipc
	bne :+
	inc vm_ipc+1
:	
dispatch_xnext:
;;; 10-12c  6 B      ; xnext: Fetch next opcode
	;; step first!
	inc vm_ipc
	bne :+
	inc vm_ipc+1
:	

;;; dispatch without advancing ipc
;;; TODO: like from call/ret/jmp?
dispatch_rnext:
;;; 32c (+ 2 5 2 23)
	ldy #0
	lda (vm_ipc),y

	;; Check high bit ($80-$FF)
	bmi handle_high_bit
	
	;; Dispatch ASCII 0-127 opcodes
	;; 23c
	tax
	lda dispatch_table_lo,x
	sta tmp
	lda dispatch_table_hi,x
	sta tmp+1
	
	jmp (tmp)

	;; ALT-selfmodify
	;; TODO: use when only patch last byte
	;;  requires page-aligned table

	;; --- High Bit Router ---
handle_high_bit:
;;; 5c
	;; If (op & $C0) == $C0, it's a vector call. Otherwise it's a ZJMP ($80-$BF)
	cmp #$C0
	bcc do_zjmp
	;; fall-througn

;;; -----------------------------------------------------------------------------
;;; Opcode Execution Blocks
;;; -----------------------------------------------------------------------------

;;; fall-througn
;;; NOTE: Don't put anything before do_vector_call!

;;; --- Vectorized Call ($C0 - $FF) ---
do_vector_call:
;;; 45c 0 B      ; Default high bit vectorized JSR
	;; from op extract index 0-31
	and #$3F
	tay
	
	;; x= load new fp
	ldx _frame_base,y

	;; save subroutine lookup index
	asl
	pha
	
	;; TODO: make these adjacent so just "copy 3 bytes"
	;; 36c

	;; store fp in callee slot
	lda vm_fp
	ldy #FR_PREV
	sta (vm_mem),y
	
	;; store ipc in callee slot
	lda vm_ipc
	ldy #FR_RET
	sta (vm_mem),y
	lda vm_ipc+1
	iny
	sta (vm_mem),y
	
	;; use new fp
	stx vm_fp
	

	;; lookup subroutine address
	;; TODO: all vectors should be hi/lo tables?
	pla
	tay
	
	lda _vptr,y
	sta vm_ipc
	lda _vptr+1,y
	sta vm_ipc+1

	jmp dispatch_rnext


op_zjmp:
;;; 12c 0 B      ; ZJMP placeholder for $80-$BF

;;; TODO: need to think about what calls 
;;;   make sense, I think many are IF THEN... ELSE not as much
;;;   loop, not as much, and maybe need longer thus 'B'
	
	jmp dispatch_rnext


op_q:
;;; 3c  0 B      ; 'q': Quit VM loop
	jmp done

op_semi:
;;; 23c 0 B      ; ';': Return with value if not main
	lda vm_fp

	;; TODO: remove, just make fun call to enter,
	;;   or require 'q'uit
	;; 5c Added at every function call for uncommon top  call!
	;; TODO: and shouln't it just be zero  after restore, so
	;; TODO: mvoe test to later!
fixfix
	;;   better inject call to done as reutrn address!!
	;;   when starting
	cmp #FP_MAIN
	bne :+
	jmp done
:	

	;; TODO: Y=1???, why adding? - this seems all wrong!
fixfixfix
	clc
	adc #FR_VAL
	tay

	;; ACC = return value
	lda (vm_mem),y
	sta vm_acc
	iny
	lda (vm_mem),y
	sta vm_acc+1
	;; fall-through directly into op_zero, matching C loop behavior

op_zero:
;;; 32c 0 B      ; 0: Return from sub-frame, keep ACC
	lda vm_fp

	;; TODO: remove, just make fun call to enter,
	;;   or require 'q'uit
	cmp #FP_MAIN
	bne :+
	jmp done
:	

	;; more...?
	ldy vm_fp
	tya
	clc
	adc #FR_RET
	tay
	lda (vm_mem),y
	sta vm_ipc
	iny
	lda (vm_mem),y
	sta vm_ipc+1
	
	ldy vm_fp
	tya
	clc
	adc #FR_PREV
	tay
	lda (vm_mem),y
	sta vm_fp
	jmp dispatch_xnext


op_plus:
;;; 26c 1 B      ; '+': acc += PAR
	lda (vm_ipc),y      ; Fetch parameter slot index directly
	tay
	lda (vm_mem),y      ; Load low byte of PAR via ZP pointer
	iny
	ldx (vm_mem),y      ; Load high byte of PAR via ZP pointer
	
fixfix, why are we not jjust adding w adc (vm_mem),y???

	clc
	adc vm_acc
	sta vm_acc
	txa
	adc vm_acc+1
	sta vm_acc+1
	jmp dispatch_pnext

op_minus:
;;; 28c 1 B      ; '-': acc -= PAR
	lda (vm_ipc),y

	tay
	lda vm_acc

	sec
	sbc (vm_mem),y
	sta vm_acc
	iny
	lda vm_acc+1
	sbc (vm_mem),y
	sta vm_acc+1

	jmp dispatch_pnext

op_load:
;;; 19c 1 B      ; 'L': acc = PAR
	lda (vm_ipc),y

	tay
	lda (vm_mem),y
	sta vm_acc
	iny
	lda (vm_mem),y
	sta vm_acc+1
	
	jmp dispatch_pnext

op_clear:
;;; 9c  0 B      ; '0': Clear accumulator
	lda #0
	sta vm_acc
	sta vm_acc+1

	jmp dispatch_xnext

op_true:
;;; 9c  0 B      ; '9': Set accumulator to 0xFFFF
	lda #$FF
	sta vm_acc
	sta vm_acc+1

	jmp dispatch_xnext


;;; TODO: do the clever woz RTS loop, so we can tail call!

op_newline:
;;; 12c 0 B      ; 'N': Print newline character
	lda #10
	jsr _putchar

	jmp dispatch_xnext

op_echo:
;;; 10c 0 B      ; 'E': Output low byte of ACC
	lda vm_acc
	jsr _putchar

	jmp dispatch_xnext

op_back:
;;; 24c 1 B      ; 'B': Branch backward relative
	lda (vm_ipc),y
	sta tmp
	
	;; just do xor a ; adc ? no need store tmp
fixfix
	lda vm_ipc
	sec
	sbc tmp
	sta vm_ipc
	lda vm_ipc+1
	sbc #0
	sta vm_ipc+1

	jmp dispatch_pnext

op_unknown:
;;; 8c  0 B      ; Unknown lower ASCII op fallback

	// TODO: how to handle errors?
	
	jmp dispatch_xnext

;;; -----------------------------------------------------------------------------
;;; Shutdown
;;; -----------------------------------------------------------------------------
done:
;;; 'q'uit, return ACC in A/X for cc65
	lda vm_acc
	ldx vm_acc+1
	;; TODO: if do clever woz-rts-loop, need pla;pal;rts
	rts

; -----------------------------------------------------------------------------
; 128-Slot Jump Table (Covers ASCII $00 - $7F)
; -----------------------------------------------------------------------------
.segment "RODATA"
dispatch_table_lo:
	.repeat 128, I
		.if I = 'q'
			.byte <op_q
		.elseif I = ';'
			.byte <op_semi
		.elseif I = 0
			.byte <op_zero
		.elseif I = '+'
			.byte <op_plus
		.elseif I = '-'
			.byte <op_minus
		.elseif I = 'L'
			.byte <op_load
		.elseif I = '0'
			.byte <op_clear
		.elseif I = '9'
			.byte <op_true
		.elseif I = 'B'
			.byte <op_back
		.elseif I = 'N'
			.byte <op_newline
		.elseif I = 'E'
			.byte <op_echo
		.else
			.byte <op_unknown
		.endif
	.endrepeat

dispatch_table_hi:
	.repeat 128, I
		.if I = 'q'
			.byte >op_q
		.elseif I = ';'
			.byte >op_semi
		.elseif I = 0
			.byte >op_zero
		.elseif I = '+'
			.byte >op_plus
		.elseif I = '-'
			.byte >op_minus
		.elseif I = 'L'
			.byte >op_load
		.elseif I = '0'
			.byte >op_clear
		.elseif I = '9'
			.byte >op_true
		.elseif I = 'B'
			.byte >op_back
		.elseif I = 'N'
			.byte >op_newline
		.elseif I = 'E'
			.byte >op_echo
		.else
			.byte >op_unknown
		.endif
	.endrepeat
