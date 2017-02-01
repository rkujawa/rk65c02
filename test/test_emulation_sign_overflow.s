; stolen from: http://www.6502.org/tutorials/vflag.html
; adapted to vasm and std syntax by rkujawa
;
; Demonstrate that the V flag works as described
;
; Returns with ERROR = 0 if the test passes, ERROR = 1 if the test fails
;
; Five (additional) memory locations are used: ERROR, S1, S2, U1, and U2
; which can be located anywhere convenient in RAM
;
.org 0xC000
.set S1,	0x10
.set S2,	0x11
.set U1,	0x12
.set U2, 	0x13
.set ERROR,	0x20

TEST:	cld	; Clear decimal mode (just in case) for test
	lda #1
	sta ERROR ; Store 1 in ERROR until test passes
	lda #0x80
	sta S1    ; Initalize S1 and S2 to -128 ($80)
	sta S2
	lda #0
	sta U1    ; Initialize U1 and U2 to 0
	sta U2
	ldy #1    ; Initialize Y (used to set and clear the carry flag) to 1
LOOP:	jsr ADD   ; Test ADC
	cpx #1
	beq DONE  ; End if V and unsigned result do not agree (X = 1)
	jsr SUB   ; Test SBC
	cpx #1
	beq DONE  ; End if V and unsigned result do not agree (X = 1)
	inc S1
	inc U1
	bne LOOP  ; Loop until all 256 possibilities of S1 and U1 are tested
	inc S2
	inc U2
	bne LOOP  ; Loop until all 256 possibilities of S2 and U2 are tested
	dey
	bpl LOOP  ; Loop until both possiblities of the carry flag are tested
	lda #0
	sta ERROR ; All tests pass, so store 0 in ERROR
DONE:	stp
;
; Test ADC
;
; X is initialized to 0
; X is incremented when V = 1
; X is incremented when the unsigned result predicts an overflow
; Therefore, if the V flag and the unsigned result agree, X will be
; incremented zero or two times (returning X = 0 or X = 2), and if they do
; not agree X will be incremented once (returning X = 1)
;
ADD:	cpy #1   ; Set carry when Y = 1, clear carry when Y = 0
	lda S1   ; Test twos complement addition
	adc S2
	ldx #0   ; Initialize X to 0
	bvc ADD1
	inx ; Increment X if V = 1
ADD1:	cpy #1   ; Set carry when Y = 1, clear carry when Y = 0
	lda U1   ; Test unsigned addition
	adc U2
	bcs ADD3 ; Carry is set if U1 + U2 >= 256
	bmi ADD2 ; U1 + U2 < 256, A >= 128 if U1 + U2 >= 128
	inx ; Increment X if U1 + U2 < 128
ADD2:	rts
ADD3:	bpl ADD4 ; U1 + U2 >= 256, A <= 127 if U1 + U2 <= 383 ($17F)
	inx ; Increment X if U1 + U2 > 383
ADD4:	rts

;
; Test SBC
;
; X is initialized to 0
; X is incremented when V = 1
; X is incremented when the unsigned result predicts an overflow
; Therefore, if the V flag and the unsigned result agree, X will be
; incremented zero or two times (returning X = 0 or X = 2), and if they do
; not agree X will be incremented once (returning X = 1)
;
SUB:	cpy #0x1   ; Set carry when Y = 1, clear carry when Y = 0
	lda S1   ; Test twos complement subtraction
	sbc S2
	ldx #0x0   ; Initialize X to 0
	bvc SUB1
	inx ; Increment X if V = 1
SUB1:	cpy #0x1   ; Set carry when Y = 1, clear carry when Y = 0
	lda U1   ; Test unsigned subtraction
	sbc U2
	pha ; Save the low byte of result on the stack
	lda #0xFF
	sbc #0 ; result = (65280 + U1) - U2, 65280 = $FF00
	cmp #0xFE
	bne SUB4 ; Branch if result >= 65280 ($FF00) or result < 65024 ($FE00)
	pla ; Get the low byte of result
	bmi SUB3 ; result < 65280 ($FF00), A >= 128 if result >= 65152 ($FE80)
SUB2:	inx ; Increment X if result < 65152 ($FE80)
SUB3:	rts
SUB4:	pla ; Get the low byte of result (does not affect the carry flag)
	bcc SUB2 ; The carry flag is clear if result < 65024 ($FE00)
	bpl SUB5 ; result >= 65280 ($FF00), A <= 127 if result <= 65407 ($FF7F)
	inx ; Increment X if result > 65407 ($FF7F)
SUB5:	rts 

