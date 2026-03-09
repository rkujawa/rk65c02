; =============================================================================
; Tiny OS — Per-task code (loaded at physical $11000, $18100, $20100)
; =============================================================================
; Assemble to tinyos_task.rom. Host loads at 0x10000+$1000 for each task.
; Virtual $1000 is mapped to the current task's physical 32KB region.
;
; Guest contract: $FF00 = yield (write next task id 0/1/2), $FF01 = current
; task id (read-only). Console at $DE00. Each task prints its id, yields
; round-robin; every other yield does WAI so host can switch via IRQ.
; =============================================================================

	.org 0x1000

task_entry:
	lda 0xFF01				; current task id
	clc
	adc #48					; ASCII '0'
	sta 0xDE00				; print to console

	inc 0x02				; yield parity (per-task ZP)
	lda 0x02
	and #1
	bne coop_yield
	wai					; every other yield: host switches, asserts IRQ

coop_yield:
	inc 0x0200				; run count (per-task)
	lda 0x0200
	cmp #3
	bcs halt

	lda 0xFF01				; next task = (current + 1) % 3
	clc
	adc #1
	cmp #3
	bcc ok
	lda #0
ok:
	sta 0xFF00				; yield to next task
	jmp task_entry

halt:
	stp
