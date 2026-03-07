; Minimal SAVE/LOAD for rk65c02 (no tape/cassette; print message and return)
.segment "CODE"

SAVE:
	lda	#<QT_SAVE_NO
	ldy	#>QT_SAVE_NO
	jsr	STROUT
	rts
LOAD:
	lda	#<QT_LOAD_NO
	ldy	#>QT_LOAD_NO
	jsr	STROUT
	rts

QT_SAVE_NO:
	.byte	"SAVE NOT SUPPORTED", 13, 0
QT_LOAD_NO:
	.byte	"LOAD NOT SUPPORTED", 13, 0
