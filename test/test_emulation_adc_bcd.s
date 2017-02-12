; somewhat inspired by http://www.6502.org/tutorials/decimal_mode.html
start:  sed
	sec
	lda #0x58
	adc #0x46
	sta 0x10
	php
	plx
	stx 0x11

	sed
	clc
	lda #0x12
	adc #0x34
	sta 0x20
	php
	plx
	stx 0x21

	sed
	clc
	lda #0x15
	adc #0x26
	sta 0x30
	php
	plx
	stx 0x31

	sed
	clc
	lda #0x81
	adc #0x92
	sta 0x40
	php
	plx
	stx 0x41

	stp

