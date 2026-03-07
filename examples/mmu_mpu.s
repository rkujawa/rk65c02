; =============================================================================
; MMU MPU Example — Guest code (setup MPU, trigger violation, IRQ handler)
; =============================================================================
;
; Assemble to mmu_mpu.rom, load at $C000. Handler at $C000, main at $C010.
; Host sets IRQ vector at $FFFE to $C000.
;
; Guest contract (host-defined):
;   MMIO at $FE00: control, region0 start/end/perms, violation addr/type, clear.
;   Region 0 $0200-$02FF initially R+X only. Store to $0200 faults; host
;   asserts IRQ and stops. On restart CPU takes IRQ. Handler clears IRQ and
;   adds W to region 0, RTI. Faulting store then succeeds.
;
; Pad so file offset = address (host can seek to $C000 and load one chunk).
; =============================================================================

	.org 0
	.space 0xC000
	.org 0xC000

.set MPU_CTRL, 0xFE00
.set MPU_R0_ST, 0xFE01
.set MPU_R0_END, 0xFE03
.set MPU_R0_PER, 0xFE05
.set MPU_CLEAR, 0xFE0D
.set RESULT, 0x0200

; IRQ handler first (vector points here)
irq_handler:
	lda #0
	sta MPU_CLEAR
	lda #7            ; R|W|X
	sta MPU_R0_PER
	lda #0x42         ; restore value for STA RESULT after RTI
	rti

	.org 0xC010
main:
	cli
	lda #0x00
	sta MPU_R0_ST
	lda #0x02
	sta MPU_R0_ST+1
	lda #0xFF
	sta MPU_R0_END
	lda #0x02
	sta MPU_R0_END+1
	lda #5
	sta MPU_R0_PER
	lda #3
	sta MPU_CTRL
	lda #0x42
	sta RESULT
	lda RESULT
	cmp #0x42
	bne fail
	stp
fail:
	brk
