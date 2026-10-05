// TODO: This is gemini generated code,
//   it couldn't implement the other opitmized version,
//   and it keeps failing on this one1 Possibly wrong
//   code as well as data genereated by bcd-*.py
	 
;;; =========================================================================
;;; 6502 4-Digit Exponential Printable Table Generator
;;; Optimized with Inverted Fall-Through, Register-X Stream, and Zero-Check
;;; Target Assembler: ca65
;;; =========================================================================

.setcpu "6502"

				; --- Zero Page Allocation ---
.segment "ZEROPAGE"        ; FIX: Force variables explicitly into ZP segment mapping
BcdL:          .res 1      ; Low 2 digits of BCD Accumulator (e.g., $00)
BcdH:          .res 1      ; High 2 digits of BCD Accumulator (e.g., $10)
Delta:         .res 1      ; Tracking delta step size
LoopCount:     .res 1      ; Iteration counter
TestIdx:       .res 1      ; Global test sweep counter (0-255)
TablePtr:      .res 2      ; 16-bit Zero Page pointer to bitstream data
BitReg:        .res 1      ; FIX: Safe Zero Page storage for bitstream buffer


.segment "CODE"

.import _exit
.import _putchar
	
putchar= _putchar
	
nl:	
	lda #$0a
;	jsr _putchar
;	lda #$0d
	jmp _putchar
	
.macro NL
	jsr nl
.endmacro
	
.macro PUTC char
	lda #char
	jsr _putchar
.endmacro
	

.export _main
_main:   

; 1. Main Entry Point
start:
        sei
        cld
        ldx #$FF
        txs
        
        ;; meat
        PUTC 'a'

	jsr _mainx
	
        PUTC 'z'
	NL

	jmp _exit
halt:
        jmp halt


;;; =========================================================================
;;; Main Sweep Harness
;;; Runs 0 to 255, computes the 4-digit BCD value, and prints it as X.XXX
;;; =========================================================================
;.export _main			
	
.proc _mainx
	;; Minimal existing check
	PUTC 'A'
	NL

	; 1. Initial State Initialization (Moved out of subroutine to run ONCE)
	LDA #$00
	STA BcdL
	LDA #$10
	STA BcdH               ; Start base at 1000 ($10 $00 BCD)
	LDA #$09
	STA Delta              ; Initial step delta for 4-digits starts at 9

	; 2. Initialize Data Pointer
	LDA #<BitstreamTable
	STA TablePtr
	LDA #>BitstreamTable
	STA TablePtr+1

	; 3. Initial Register Configurations
	LDY #0                 ; Reset stream table index register
	LDA #0
	STA BitReg             ; Force bit buffer to 0 to instantly trigger first reload

	; Print the baseline step 0 value manually before entering the loop
	JSR PrintCurrentBCD

	LDA #1                 ; Start iteration from step 1
	STA TestIdx

SweepLoop:
	; Step forward exactly 1 step through the bitstream data dynamically
	JSR ConvertLogBcdStep  

				; --- PRINTING PHASE (Formats BCD as X.XXX) ---
	JSR PrintCurrentBCD

	INC TestIdx            ; Advance to next index point
	BNE SweepLoop          ; Run full 256 entries mapping
	RTS

	;; Minimal existing check
	PUTC 'Z'
	NL
.endproc

; =========================================================================
; Helper Subroutine to output BcdH/BcdL
; =========================================================================
.proc PrintCurrentBCD
	PUTC ' '

	; Digit 1 (High Nibble of BcdH)
	LDA BcdH
	LSR A
	LSR A
	LSR A
	LSR A 
	ORA #$30
	JSR putchar

	; Digit 2 (Low Nibble of BcdH)
	LDA BcdH
	AND #$0F               
	ORA #$30
	JSR putchar

	; Digit 3 (High Nibble of BcdL)
	LDA BcdL
	LSR A
	LSR A
	LSR A
	LSR A 
	ORA #$30
	JSR putchar

	; Digit 4 (Low Nibble of BcdL)
	LDA BcdL
	AND #$0F               
	ORA #$30
	JSR putchar
	RTS
.endproc

;;; =========================================================================
;;; ConvertLogBcdStep
;;; Processes exactly 1 step forward in the 2-bit fixed stream
;;; =========================================================================
	
.proc ConvertLogBcdStep
	SED                    ; ENGAGE PERSISTENT DECIMAL MODE FOR THE STEP

				; --- INLINE ZERO CHECK & RELOAD ---
	LDA BitReg             ; Check if bit register goes empty
	BNE FetchBits          ; If not zero, skip the reload block
	
	LDA (TablePtr),Y       ; Pull fresh payload byte from 64-byte data stream
	INY                    ; Increment data table array index
	STA BitReg             ; Save configuration state directly into safe ZP buffer

FetchBits:
	LDA BitReg             ; Load active bitstream state
	; Extract exactly 2 bits from the current stream byte in A
	ASL A                  ; Bit A into Carry
	BCS FirstBitSet
	ASL A                  ; Bit B into Carry
	BCS DoIncrement        ; %01 -> +1
	STA BitReg             ; FIX: Save shifted state here before fast path fall-through
	JMP Accumulate         ; %00 -> Unchanged

FirstBitSet:
	ASL A                  ; Bit B into Carry
	BCS DoAddTwo           ; %11 -> +2

DoDecrement:
	; %10 -> BCD-compliant decrement (-1)
	STA BitReg             ; FIX: Save shifted state here before clobbering A
	LDA Delta
	SEC
	SBC #1
	STA Delta
	JMP Accumulate

DoIncrement:
	; %01 -> BCD-compliant increment (+1)
	STA BitReg             ; FIX: Save shifted state here before clobbering A
	LDA Delta
	CLC
	ADC #1
	STA Delta
	JMP Accumulate

DoAddTwo:
	; %11 -> BCD-compliant increment (+2)
	STA BitReg             ; FIX: Save shifted state here before clobbering A
	LDA Delta
	CLC
	ADC #2
	STA Delta

Accumulate:
	CLC
	LDA BcdL
	ADC Delta
	STA BcdL
	LDA BcdH
	ADC #0                 ; Native BCD carry rippling
	STA BcdH 

Done:
	CLD                    ; DISENGAGE DECIMAL MODE AT ROUTINE EXIT
	RTS
.endproc

;;; =========================================================================
;;; Encoded 4-Digit Variable Bitstream Table From bcd-delta3.py
;;; =========================================================================
.segment "RODATA"

BitstreamTable:
	.byte $01, $92, $10, $01, $99, $00, $06, $40, $00, $64, $01, $84, $00, $19, $00, $01
	.byte $00, $19, $00, $19, $00, $10, $01, $90, $06, $40, $06, $40, $06, $40, $19, $01
	.byte $90, $19, $00, $64, $06, $40, $10, $06, $41, $90, $19, $00, $40, $10, $04, $01
	.byte $01, $90, $40, $10, $40, $10, $10, $04, $04, $04, $04, $10, $19, $10, $40, $41
	.byte $01, $04, $10, $11, $01, $04, $10, $11, $01, $10
