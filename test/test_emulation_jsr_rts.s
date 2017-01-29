.org 0xC000

start:	nop
	jsr foo
	nop
	stp

foo:	lda #0xAA
	rts

