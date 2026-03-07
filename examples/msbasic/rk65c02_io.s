; I/O stub for rk65c02: MONCOUT, MONRDKEY, MONRDKEY2 at $F002.
; Data port $F000 (write=putchar, read=getchar), status $F001 (bit0=Ctrl-C).
.setcpu "6502"
.segment "IOSTUB"
.export MONCOUT, MONRDKEY, MONRDKEY2

MONCOUT:
	sta	$F000
	rts
MONRDKEY:
	lda	$F000
	rts
MONRDKEY2:
	lda	$F000
	rts
