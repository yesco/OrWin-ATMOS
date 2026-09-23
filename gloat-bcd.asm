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

	LDA #0
	STA TestIdx

SweepLoop:
	LDA TestIdx
	STA LoopCount          ; Set math iteration target steps
	JSR ConvertLogBcd      ; Compute the 4-digit value

				; --- PRINTING PHASE (Formats BCD as X.XXX) ---
				; Digit 1 (High Nibble of BcdH)
	LDA BcdH
	LSR A
	LSR A
	LSR A
	LSR A 
	ORA #$30
	JSR putchar

	PUTC ' '
;	PUTC '.'		; Print decimal point

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

;	NL

	INC TestIdx            ; Advance to next index point
	BNE SweepLoop          ; Run full 256 entries mapping
	RTS

	;; Minimal existing check
	PUTC 'Z'
	NL
.endproc

;;; =========================================================================
;;; ConvertLogBcd
;;; 4-Digit Variable Bitstream Accumulator
;;; Total Cycle Span (X=255): ~8,751 cycles | Code size: ~36 bytes
;;; =========================================================================
	
.proc ConvertLogBcd
				; 1. Initial State Initialization (4-Digit Parameters)
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
	LDX #0                 ; Force X register to 0 to instantly trigger first reload
	
	LDA LoopCount
	BEQ Done               ; If target is exactly 0 steps, skip loop completely

	SED                    ; ENGAGE PERSISTENT DECIMAL MODE FOR THE SWEEP

StepLoop:
				; --- INLINE REGISTER-X ZERO CHECK & RELOAD ---
	TXA                    ; Check if register goes empty
	BNE FetchBit           ; If not zero, skip the reload block
	
	LDA (TablePtr),Y       ; Pull fresh payload byte from 51-byte data stream
	INY                    ; Increment data table array index
	TAX                    ; Move configuration state directly into register X buffer
	TXA

FetchBit:
	ASL A                  ; Push next bit pattern directly into Carry status
	BCS DeltaChanged       ; IF BIT IS 1: Jump out to processing lanes

Accumulate:
	TAX                    ; Fast-save remaining shift states back into register X
	CLC
	LDA BcdL
	ADC Delta
	STA BcdL
	LDA BcdH
	ADC #0                 ; Native BCD carry rippling
	STA BcdH 

	DEC LoopCount
	BNE StepLoop           ; Loop structural sweep check

Done:
	CLD                    ; DISENGAGE DECIMAL MODE AT ROUTINE EXIT
	RTS

				; --- UNCOMMON BRANCH TARGET PATHWAYS ---
DeltaChanged:
				; Bit 1 was '1'. We already have the remaining bits in A. Check Bit 2.
	BNE FetchBit2          ; If A is not empty, skip reload
	LDA (TablePtr),Y       ; Reload byte if it split exactly on a byte boundary
	INY
FetchBit2:
	ASL A
	BCS SpecialAdjust      ; Token is %11 -> Jump to rare cases
	
	PHA                    ; Save stream bits
	LDA Delta
	CLC
	ADC #1                 ; BCD-compliant increment (+1)
	STA Delta
	PLA                    ; Restore stream bits
	BCC Accumulate         ; 2-byte short branch fallback into fast path

SpecialAdjust:
				; Bits were %11. Check Bit 3.
	BNE FetchBit3          ; If A is not empty, skip reload
	LDA (TablePtr),Y
	INY
FetchBit3:
	ASL A
	BCS AddTwo             ; Token is %111 -> Go update step size by +2

	PHA                    ; Save stream bits
	LDA Delta
	SEC
	SBC #1                 ; BCD-compliant decrement (-1)
	STA Delta
	PLA                    ; Restore stream bits
	BNE Accumulate         ; 2-byte branch fallback to fast path

AddTwo:
	PHA                    ; Temporarily save our active bitstream register
	LDA Delta
	CLC
	ADC #2
	STA Delta              ; Update step size natively by +2 via BCD
	PLA                    ; Restore our active bitstream bits to A
	BCC Accumulate         ; Return control back into main stream loop
.endproc

;;; =========================================================================
;;; 51-Byte Encoded 4-Digit Variable Bitstream Table
;;; Compiled precisely to map: 0=Same, 10=+1, 110=-1, 111=+2
;;; =========================================================================
.segment "RODATA"

BitstreamTable:
	.byte $16, $4D, $02, $D6, $80, $B4, $02, $D0, $59, $02, $D0, $26, $96, $41, $68, $5A
	.byte $08, $2D, $16, $8B, $4B, $41, $16, $96, $96, $88, $42, $21, $10, $96, $FA, $25
	.byte $A8, $88, $91, $22, $48, $92, $49, $14, $8F, $B9, $29, $29, $29, $2A, $4A, $93
	.byte $A7, $4E, $80
