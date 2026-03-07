; =============================================================================
; MMU Cart Example — Bank 0 guest code (16KB)
; =============================================================================
;
; Assemble to mmu_cart_bank0.rom and load at physical $10000-$13FFF (bank 0).
; The host's MMU translate callback maps guest addresses $8000-$BFFF to this
; physical range when "current bank" is 0.
;
; GUEST-VISIBLE CONTRACT (defined by the host program, not rk65c02):
;   - Cart window: $8000-$BFFF. Content depends on selected bank. This is
;     the ONLY way to access the cart (C64-style); $C000-$FFFF is not the cart.
;   - Write 0 or 1 to $DE00 to select bank 0 or 1. Host polls the physical
;     address that guest $DE00 maps to and updates the mapping.
;   - $0200 is a result byte the host reads to verify which bank ran.
; =============================================================================

	.org 0x8000

bank0_entry:
	lda #0xA5			; marker: this is bank 0
	sta 0x0200			; host will read 0x0200 from bus to verify

	lda #1				; select bank 1: write bank id to 0xDE00
	sta 0xDE00

	; Next instruction fetch will be from 0x8000. By then the host (in its
	; tick callback) will have seen 0xDE00=1, updated the mapping, so the
	; CPU will fetch from bank 1's code.
	jmp 0x8000
