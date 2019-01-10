.org 0xC000
start:	jsr mul_8bit_to_8bits
	stp

; mul_8bit_to_8bits
; Multiplies 2 numbers (passed on stack) and returns result on stack.

; General 8bit * 8bit = 8bit multiply
; by White Flame 20030207
; adapted as rk65c02 example by rkujawa

; Instead of using a bit counter, this routine early-exits when num2 reaches
; zero, thus saving iterations.

; .X and .Y are preserved
; num1 and num2 get clobbered
.set num1,0x10
.set num2,0x11
.set retl,0x12
.set reth,0x13

mul_8bit_to_8bits:
	pla		; pull return address
	sta retl
	pla
	sta reth
	pla		; pull num1 from stack
	sta num1
	pla		; pull num2 from stack
	sta num2

	lda #0x00
	beq enterl
doAdd:	clc
	adc num1
loop:	asl num1
; For an accumulating multiply (A = A + num1 * num2), 
; set up num1 and num2, then enter here.
enterl:	lsr num2
	bcs doAdd
	bne loop
end:
	pha		; save result to stack
	lda reth	; restore return address
	pha
	lda retl
	pha
	rts		; return from function


