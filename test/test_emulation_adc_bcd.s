; somewhat inspired by http://www.6502.org/tutorials/decimal_mode.html
start:  sed
	sec
	lda #0x58
	adc #0x46
	sta 0x10
	php
	plx
	stx 0x11

	stp

