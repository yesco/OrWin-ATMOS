	.export _run_ivm
	.import _main_entry, _frame_base, _vptr, _mem, _putchar, _printf

	.zeropage
vm_ipc:     .res 2      ; Virtual Instruction Pointer (16-bit ZP pointer)
vm_acc:     .res 2      ; 16-bit Virtual Accumulator (Low: vm_acc, High: vm_acc+1)
vm_fp:      .res 1      ; Virtual Frame Pointer (8-bit index)
tmp:        .res 2      ; Scratchpad for jump tables / expressions

	FP_MAIN     = $00
	FR_VAL      = 0
	FR_RET      = 1
	FR_PREV     = 3

	.segment "CODE"

;;; -----------------------------------------------------------------------------
;;; unsigned int run_ivm(void);
;;; -----------------------------------------------------------------------------
_run_ivm:
	lda _main_entry
	sta vm_ipc
	lda _main_entry+1
	sta vm_ipc+1
	
	lda #0
	sta vm_acc
	sta vm_acc+1
	sta vm_fp
	jmp dispatch_xnext

;;; -----------------------------------------------------------------------------
;;; Dispatch Entry Points (Fall-through architecture)
;;; -----------------------------------------------------------------------------

dispatch_pnext:
;;; 5c  1 B      ; pnext: Advance IPC past the 1-byte parameter
	inc vm_ipc
	bne dispatch_xnext
	inc vm_ipc+1

dispatch_xnext:
;;; 5c  1 B      ; xnext: Fetch next opcode
	ldy #0
	lda (vm_ipc),y
	inc vm_ipc
	bne @no_carry
	inc vm_ipc+1
	@no_carry:

	;; Check high bit ($80-$FF) immediately
	bmi handle_high_bit
	
	;; Dispatch remaining 0-127 opcodes via 128-slot jump table
	tax
	lda dispatch_table_lo,x
	sta tmp
	lda dispatch_table_hi,x
	sta tmp+1
	jmp (tmp)

	;; --- High Bit Router ---
handle_high_bit:
				;; If (op & $C0) == $C0, it's a vector call. Otherwise it's a ZJMP ($80-$BF)
	cmp #$C0
	bcs do_vector_call

op_zjmp:
;;; 12c 0 B      ; ZJMP placeholder for $80-$BF
	jmp dispatch_xnext


;;; -----------------------------------------------------------------------------
;;; Opcode Execution Blocks
;;; -----------------------------------------------------------------------------

;;; --- Vectorized Call ($C0 - $FF) ---
do_vector_call:
;;; 45c 0 B      ; Default high bit vectorized JSR
	and #$3F
	tay
	
	lda _frame_base,y
	tax
	
	lda vm_fp
	sta _mem+FR_PREV,x
	
	lda vm_ipc
	sta _mem+FR_RET,x
	lda vm_ipc+1
	sta _mem+FR_RET+1,x
	
	stx vm_fp
	
	tya
	asl
	tay
	lda _vptr,y
	sta vm_ipc
	lda _vptr+1,y
	sta vm_ipc+1
	jmp dispatch_xnext


op_q:
;;; 3c  0 B      ; 'q': Quit VM loop
	jmp done

op_semi:
;;; 23c 0 B      ; ';': Return with value if not main
	lda vm_fp

	;; TODO: remove, just make fun call to enter,
	;;   or require 'q'uit
	cmp #FP_MAIN
	bne :+
	jmp done
:	
	;; TODO: Y=0, why adding 
	clc
	adc #FR_VAL
	tay

	;; ACC = return value
	lda _mem,y
	sta vm_acc
	lda _mem+1,y
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
	ldy vm_fp
	lda _mem+FR_RET,y
	sta vm_ipc
	lda _mem+FR_RET+1,y
	sta vm_ipc+1
	
	lda _mem+FR_PREV,y
	sta vm_fp
	jmp dispatch_xnext

op_plus:
;;; 26c 1 B      ; '+': acc += PAR
	ldy #0
	lda (vm_ipc),y      ; Fetch parameter slot index directly
	tay
	lda _mem,y          ; Load low byte of PAR
	ldx _mem+1,y        ; Load high byte of PAR
	
	clc
	adc vm_acc
	sta vm_acc
	txa
	adc vm_acc+1
	sta vm_acc+1
	jmp dispatch_pnext

op_minus:
;;; 28c 1 B      ; '-': acc -= PAR
	ldy #0
	lda (vm_ipc),y
	tay
	
	lda vm_acc
	sec
	sbc _mem,y
	sta vm_acc
	lda vm_acc+1
	sbc _mem+1,y
	sta vm_acc+1
	jmp dispatch_pnext

op_L:
;;; 19c 1 B      ; 'L': acc = PAR
	ldy #0
	lda (vm_ipc),y
	tay
	lda _mem,y
	sta vm_acc
	lda _mem+1,y
	sta vm_acc+1
	jmp dispatch_pnext

op_clear:
;;; 9c  0 B      ; '0': Clear accumulator
	lda #0
	sta vm_acc
	sta vm_acc+1
	jmp dispatch_xnext

op_max:
;;; 9c  0 B      ; '9': Set accumulator to 0xFFFF
	lda #$FF
	sta vm_acc
	sta vm_acc+1
	jmp dispatch_xnext

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

op_B:
;;; 24c 1 B      ; 'B': Branch backward relative
	ldy #0
	lda (vm_ipc),y
	sta tmp
	
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
	jmp dispatch_xnext

;;; -----------------------------------------------------------------------------
;;; Shutdown
;;; -----------------------------------------------------------------------------
done:
	lda vm_acc
	ldx vm_acc+1
	rts



.segment "RODATA"

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
            .byte <op_L
        .elseif I = '0'
            .byte <op_clear
        .elseif I = '9'
            .byte <op_max
        .elseif I = 'B'
            .byte <op_B
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
            .byte >op_L
        .elseif I = '0'
            .byte >op_clear
        .elseif I = '9'
            .byte >op_max
        .elseif I = 'B'
            .byte >op_B
        .elseif I = 'N'
            .byte >op_newline
        .elseif I = 'E'
            .byte >op_echo
        .else
            .byte >op_unknown
        .endif
    .endrepeat
