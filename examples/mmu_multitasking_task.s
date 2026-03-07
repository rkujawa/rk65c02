; =============================================================================
; MMU Multitasking — Per-task code (loaded at physical $1000 and $5000)
; =============================================================================
; Assemble to mmu_multitasking_task.rom. The host loads this at physical
; $1000 (task 0) and $5000 (task 1). Virtual $1000 is mapped to either
; physical $1000 or $5000 depending on current task.
;
; Guest contract: $FF00 = yield (write next task id), $FF01 = current task id (read-only, set by host).
; =============================================================================

	.org 0x1000

task_entry:
	inc 0x0200				; per-task counter (each task has its own 0x0200)

	lda 0x0200
	cmp #3
	bcs halt

	lda 0xFF01				; current task id (host wrote it when switching)
	eor #1
	sta 0xFF00				; yield to the other task
	jmp task_entry

halt:
	stp
