; Small program for stepper example: a few instructions then STP.
	.org 0xC000

	lda #1
	sta 0x0200
	lda #2
	sta 0x0201
	stp
