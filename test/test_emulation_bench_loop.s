; Compute-intensive loop for interpreter vs JIT benchmark.
; Runs a large number of JIT-native instructions (ALU, load/store, branches)
; then stops with STP. Same result regardless of interp/JIT.
;
.org 0xC000

.set COUNTER_LO, 0x10
.set COUNTER_HI, 0x11
.set TMP,        0x12
.set OUTER,      0x13

start:
	cld
	lda #0
	sta COUNTER_LO
	sta COUNTER_HI
	sta TMP
	lda #0xFF
	sta OUTER

outer_loop:
	ldx #0xFF
inner_loop:
	; Mix of JIT-native ops (~24 instructions per inner iteration)
	clc
	lda TMP
	adc #1
	and #0xFF
	sta TMP
	lda COUNTER_LO
	adc #1
	sta COUNTER_LO
	bcc no_carry
	inc COUNTER_HI
no_carry:
	lda COUNTER_LO
	ora COUNTER_HI
	eor TMP
	and #0x7F
	sta TMP
	lda COUNTER_LO
	adc COUNTER_HI
	and #0xFF
	sta TMP
	dex
	bne inner_loop
	dec OUTER
	bpl outer_loop

	stp
