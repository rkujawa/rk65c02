; =============================================================================
; Tiny OS — Shared kernel (loaded at physical $8000, system RAM)
; =============================================================================
; Virtual $8000–$FFFF is identity-mapped (shared). Kernel sets initial task 0,
; then JMPs to task entry at $1000 (MMU maps to current task's physical region).
; IRQ handler at $8010: JMP $1000 so that after WAI the host-selected task runs.
; =============================================================================

	.org 0x8000

kernel:
	lda #0
	sta 0xFF00			; yield reg: request task 0
	sta 0xFF01			; current task id (host mirror)
	jmp 0x1000			; run task 0 at virtual $1000

	.org 0x8010

irq_handler:
	jmp 0x1000			; switch to task in $FF00 (set by host in idle_wait)
