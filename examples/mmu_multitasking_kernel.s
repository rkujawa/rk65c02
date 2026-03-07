; =============================================================================
; MMU Multitasking — Shared kernel (loaded at physical $8000)
; =============================================================================
; Assemble to mmu_multitasking_kernel.rom. The host loads this at $8000-$8FFF.
; Virtual $8000-$FFFF is identity-mapped (shared); all tasks see this code.
; =============================================================================

	.org 0x8000

kernel:
	lda #0
	sta 0xFF00			; request task 0 (host sets mapping, writes 0 to 0xFF01)
	jmp 0x1000			; run task 0 at virtual 0x1000
