; ISCNTC: check for Ctrl-C via status port $F001 (bit 0 = break requested)
.segment "CODE"
ISCNTC:
	lda	$F001
	and	#$01
	beq	RET2
	; fall through to STOP
;!!! runs into "STOP" (RET2 is in flow1.s)
