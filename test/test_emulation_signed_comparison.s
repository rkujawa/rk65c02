; stolen from: http://www.6502.org/tutorials/compare_beyond.html
; adapted to vasm and std syntax by rkujawa
;
; Test the signed compare routine
;
; Returns with ERROR = 0 if the test passes, ERROR = 1 if the test fails
;
; Three (additional) memory locations are used: ERROR, N1, and N2
; These may be located anywhere convenient in RAM
;
.org 0xC000
.set N1,	0x10
.set N2,	0x11
.set ERROR,	0x12
TEST:	cld		; Clear decimal mode for test
	lda #1
	sta ERROR	; Store 1 in ERROR until test passes
	tsx		; Save stack pointer so subroutines can exit with ERROR = 1
;
; Test N1 positive, N2 positive
;
        lda #0		; 0
        sta N1
PP1:	lda #0		; 0
	sta N2
PP2:	jsr SUCMP	; Verify that the signed and unsigned comparison agree
	inc N2
	bpl PP2
	inc N1
	bpl PP1
;
; Test N1 positive, N2 negative
;
	lda #0		; 0
	sta N1
PN1:	lda #0x80	; -128
	sta N2
PN2:	jsr SCMP	; Signed comparison
	bmi TEST1	; if N1 (positive) < N2 (negative) exit with ERROR = 1
	inc N2
	bmi PN2
	inc N1
	bpl PN1
;
; Test N1 negative, N2 positive
;
	lda #0x80	; -128
	sta N1
NP1:	lda #0		; 0
	sta N2
NP2:	jsr SCMP	; Signed comparison
	bpl TEST1	; if N1 (negative) >= N2 (positive) exit with ERROR = 1
	inc N2
	bpl NP2
	inc N1
	bmi NP1
;
; Test N1 negative, N2 negative
;
	lda #0x80	; -128
	sta N1
NN1:	lda #0x80	; -128
	sta N2
NN2:	jsr SUCMP	; Verify that the signed and unsigned comparisons agree
	inc N2
	bmi NN2
	inc N1
	bmi NN1

	lda #0
	sta ERROR	; All tests pass, so store 0 in ERROR
TEST1:  stp	

; Signed comparison
;
; Returns with:
;   N=0 (BPL branches) if N1 >= N2 (signed)
;   N=1 (BMI branches) if N1 < N2 (signed)
;
; The unsigned comparison result is returned in the C flag (for free)
;
SCMP:	sec
	lda N1		; Compare N1 and N2
	sbc N2
	bvc SCMP1	; Branch if V = 0
	eor #0x80	; Invert Accumulator bit 7 (which also inverts the N flag)
SCMP1:	rts	

; Test the signed and unsigned comparisons to confirm that they agree
;
SUCMP:	jsr SCMP	; Signed (and unsigned) comparison
	bcc SUCMP2	; Branch if N1 < N2 (unsigned)
	bpl SUCMP1	; N1 >= N2 (unsigned), branch if N1 >= N2 (signed)
        tsx		; reset stack and exit with ERROR = 1
SUCMP1: rts 
SUCMP2: bmi SUCMP3	; N1 < N2 (unsigned), branch if N1 < N2 (signed)
	tsx		; reset stack and exit with ERROR = 1
SUCMP3: rts


