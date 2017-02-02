; 16-bit subtraction simple example by FMan/Tropyx
; modified by rkujawa

.org 0xC000
.set num1lo, 0x62
.set num1hi, 0x63
.set num2lo, 0x64
.set num2hi, 0x65
.set reslo, 0x66
.set reshi, 0x67

; subtracts number 2 from number 1 and writes result out

sub:	sec				; set carry for borrow purpose
	lda num1lo
	sbc num2lo			; perform subtraction on the LSBs
	sta reslo
	lda num1hi			; do the same for the MSBs, with carry
	sbc num2hi			; set according to the previous result
	sta reshi
	stp
