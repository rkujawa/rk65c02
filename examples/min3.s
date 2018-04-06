.org 0xC000
start:	jsr min3
	stp

; min3
; Takes 3 numbers (A, B, C), passed on stack, finds the minimum.
; Result is also passed on stack. Assumes it is being called via jsr.
.set retl,0x10
.set reth,0x11
.set res,0x12
min3:	pla		; pull low byte of return address
	sta retl
	pla
	sta reth    
	pla		; pull C from stack
	sta res		; save C into res
	pla		; pull B from stack
	cmp res		; compare B and C
	bpl bltc	; branch if B > C
	sta res		; if C is smaller, save it to res
bltc:	pla		; pull A from stack
	cmp res		; compare A and whatever is in res
	bpl ret		; branch if A > res
	sta res		; otherwise save A to res
ret:	lda res		; load res into accumulator
	pha		; save to the stack
	lda reth	; restore return address
	pha
	lda retl
	pha
	rts		; return from function

