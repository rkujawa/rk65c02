; 16-bit addition simple example by FMan/Tropyx
; modified by rkujawa

.org 0xC000
.set num1lo, 0x62
.set num1hi, 0x63
.set num2lo, 0x64
.set num2hi, 0x65
.set reslo, 0x66
.set reshi, 0x67

; adds numbers 1 and 2, writes result to separate location

add:	clc				; clear carry
	lda num1lo
	adc num2lo
	sta reslo			; store sum of LSBs
	lda num1hi
	adc num2hi			; add the MSBs using carry from
	sta reshi			; the previous calculation
	stp
