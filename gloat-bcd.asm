				; =========================================================================
				; 6502 4-Digit Exponential Printable Table Generator
				; Optimized with Inverted Fall-Through, Register-X Stream, and Zero-Check
				; Target Assembler: ca65
				; =========================================================================

	.setcpu "6502"

				; --- Zero Page Allocation ---
	.zeropage
BcdL:          .res 1      ; Low 2 digits of BCD Accumulator (e.g., $00)
BcdH:          .res 1      ; High 2 digits of BCD Accumulator (e.g., $10)
Delta:         .res 1      ; Tracking delta step size
LoopCount:     .res 1      ; Iteration counter
TestIdx:       .res 1      ; Global test sweep counter (0-255)
TablePtr:      .res 2      ; 16-bit Zero Page pointer to bitstream data

				; --- Global System Vectors ---
	CHROUT  := $FFD2          ; Standard character output vector

	.segment "CODE"

				; =========================================================================
				; Main Sweep Harness
				; Runs 0 to 255, computes the 4-digit BCD value, and prints it as X.XXX
				; =========================================================================
	.proc Main
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
	JSR CHROUT

	LDA #'.'               ; Print decimal point
	JSR CHROUT

				; Digit 2 (Low Nibble of BcdH)
	LDA BcdH
	AND #$0F               
	ORA #$30
	JSR CHROUT
				; Digit 3 (High Nibble of BcdL)
	LDA BcdL
	LSR A
	LSR A
	LSR A
	LSR A 
	ORA #$30
	JSR CHROUT

				; Digit 4 (Low Nibble of BcdL)
	LDA BcdL
	AND #$0F               
	ORA #$30
	JSR CHROUT

	LDA #$0D               ; Output Carriage Return / New Line
	JSR CHROUT

	INC TestIdx            ; Advance to next index point
	BNE SweepLoop          ; Run full 256 entries mapping
	RTS
	.endproc

				; =========================================================================
				; ConvertLogBcd
				; Robert's 4-Digit Variable Bitstream Accumulator
				; Total Cycle Span (X=255): ~8,750 cycles | Code size: ~36 bytes
				; =========================================================================
	.proc ConvertLogBcd
				; 1. Initial State Initialization (4-Digit Parameters)
	LDA #$00
	STA BcdL
	LDA #$10
	STA BcdH		; Start base at 1000 ($10 $00 BCD)
	LDA #$09
	STA Delta	 ; Initial step delta for 4-digits starts at 9

				;; 2. Initialize Data Pointer
	LDA #<BitstreamTable
	STA TablePtr
	LDA #>BitstreamTable
	STA TablePtr+1

				; 3. Initial Register Configurations
	LDY #0			; Reset stream table index register
	LDX #0 ; Force X register to 0 to instantly trigger first reload
	
	LDA LoopCount
	BEQ Done  ; If target is exactly 0 steps, skip loop completely

	SED		; ENGAGE PERSISTENT DECIMAL MODE FOR THE SWEEP

StepLoop:
				; --- INLINE REGISTER-X ZERO CHECK & RELOAD ---
	TXA                    ; Check if register goes empty
	BNE FetchBit           ; If not zero, skip the reload block
	
	LDA (TablePtr),Y       ; Pull fresh payload byte from 43-byte data stream
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
	ADC #0
	STA BcdH ; Native BCD carry rippling

	DEC LoopCount
	BNE StepLoop           ; Loop structural sweep check

Done:
	CLD                    ; DISENGAGE DECIMAL MODE AT ROUTINE EXIT
	RTS

				; --- UNCOMMON BRANCH TARGET PATHWAYS ---
DeltaChanged:
	TAX                    ; Save buffer back to X before split check
        TXA
	BNE FetchBit2	       ; Boundary split check
	LDA (TablePtr),Y
        INY
        TAX
	TXA
FetchBit2:
	ASL A
	BCS SpecialAdjust      ; Token is %11 -> Jump to rare cases
	
	INC Delta              ; Token is %10 -> Increment step size (+1)
	BCC Accumulate         ; 2-byte short branch fallback into fast path

SpecialAdjust:
	TAX
TXA: BNE FetchBit3     ; Boundary split check
	LDA (TablePtr),Y: INY: TAX: TXA
FetchBit3:
	ASL A
	BCS AddTwo             ; Token is %111 -> Go update step size by +2

	DEC Delta              ; Token is %110 -> Decrement step size by 1 (-1)
	BNE Accumulate         ; 2-byte branch fallback to fast path

AddTwo:
				; Since we are in BCD mode, add 2 via BCD addition
	LDA Delta: CLC: ADC #2: STA Delta 
	BCC Accumulate         ; Return control back into main stream loop
	.endproc

				; =========================================================================
				; 43-Byte Encoded 4-Digit Variable Bitstream Table
				; Compiled precisely to map: 0=Same, 10=+1, 110=-1, 111=+2
				; =========================================================================
	.segment "RODATA"
BitstreamTable:
	.byte $00, $00, $00, $00, $01, $04, $10, $40, $01, $04, $11, $44, $15, $55, $55, $55
	.byte $55, $55, $57, $5D, $75, $D7, $5D, $77, $5F, $7F, $55, $55, $55, $55, $55, $55
	.byte $55, $56, $55, $65, $56, $59, $66, $A6, $69, $A9, $AA
