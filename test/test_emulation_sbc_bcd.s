; somewhat inspired by http://www.6502.org/tutorials/decimal_mode.html
start:  sed
	sec
	lda #0x46
	sbc #0x12
	sta 0x10
	php
	plx
	stx 0x11

	sed
	sec
	lda #0x40
	sbc #0x13
	sta 0x20
	php
	plx
	stx 0x21

	sed
	clc
	lda #0x32
	sbc #0x02
	sta 0x30
	php
	plx
	stx 0x31

	stp

