; Benchmark: hot loop that stores to DATA bytes located on the same page
; as the executing code. No instruction byte is ever modified, so a
; byte-precise SMC tracker must not invalidate anything; page-granular
; tracking treats every store as a self-modifying-code event and demotes
; the page to interpretation.
;
; Run via: ./bench_emulation test_emulation_bench_smcdata.rom
;
.org 0xC000

.set OUTER, 0x10

start:
	cld
	lda #0xFF
	sta OUTER

outer_loop:
	ldx #0xFF
inner_loop:
	; Store to data bytes on this code page (labels below), plus a
	; small amount of native ALU work per iteration.
	txa
	sta data1
	clc
	adc data1
	sta data2
	eor data2
	sta data3
	lda data1
	adc data3
	sta data4
	dex
	bne inner_loop
	dec OUTER
	bpl outer_loop

	stp

; Data bytes on the same page as the loop code above.
data1:	.byte 0
data2:	.byte 0
data3:	.byte 0
data4:	.byte 0
