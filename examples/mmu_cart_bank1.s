; =============================================================================
; MMU Cart Example — Bank 1 guest code (16KB)
; =============================================================================
;
; Assemble to mmu_cart_bank1.rom and load at physical $14000-$17FFF (bank 1).
; The host's MMU translate callback maps guest $8000-$BFFF to this physical
; range when "current bank" is 1.
;
; Same guest contract as bank 0: $DE00 = bank select, $0200 = result byte.
; =============================================================================

	.org 0x8000

bank1_entry:
	lda #0xB6			; marker: this is bank 1
	sta 0x0200

	stp					; stop; host checks e.stopreason and bus_read_1(0x0200)==0xB6
