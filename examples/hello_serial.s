; Hello-serial example — guest writes bytes to $DE00, host device prints to stdout.
	.org 0xC000

	ldx #0
loop:
	lda string,x
	beq done
	sta 0xDE00
	inx
	bne loop
done:
	stp

string:
	.byte "Hi!", 0
