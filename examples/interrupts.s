; Interrupts example — IRQ vector at $FFFE, minimal handler, host asserts IRQ.
; 0x0200 = IRQ count (handler increments), 0x0201 = WAI wake count.
; Host sets vectors at $FFFC (reset -> start) and $FFFE (IRQ -> irq_handler).

	.org 0xC000

start:
	jmp main

irq_handler:
	inc 0x0200
	rti

main:
	cli
loop:
	wai
	inc 0x0201
	lda 0x0201
	cmp #3
	bne loop
	stp
