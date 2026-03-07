; rk65c02 platform defines for Microsoft BASIC (mist64/msbasic)
; I/O: MONCOUT/MONRDKEY/MONRDKEY2 in stub at $F002; data port at $F000, status at $F001

; configuration (AIM65-style layout, KIM-style MONCOUT/MONRDKEY)
CONFIG_2A := 1
CONFIG_MONCOUT_DESTROYS_Y := 1
CONFIG_NO_LINE_EDITING := 1
CONFIG_NULL := 1
CONFIG_PRINT_CR := 1
CONFIG_SAFE_NAMENOTFOUND := 1
CONFIG_SCRTCH_ORDER := 1
CONFIG_PEEK_SAVE_LINNUM := 1
CONFIG_SMALL_ERROR := 1

; zero page
ZP_START1 = $00
ZP_START2 = $10
ZP_START3 = $06
ZP_START4 = $5E

; extra ZP variables
USR := $03
TXPSV := LASTOP
; Do not define NULL here; flow1.s has label NULL when AIM65 is not defined

; input buffer
INPUTBUFFER := $0016

; extra stack
STACK2 := $0200

; constants
STACK_TOP := $FD
SPACE_FOR_GOSUB := $44
NULL_MAX := $F2
CRLF_1 := CR
CRLF_2 := LF
WIDTH := 72
WIDTH2 := 56

; memory layout
RAMSTART2 := $0211

; I/O stub addresses (code at $F002; data port $F000, status $F001)
; Stub layout: MONCOUT 4 bytes (STA+RTS), MONRDKEY 4 bytes, MONRDKEY2 4 bytes
MONCOUT   := $F002
MONRDKEY  := $F006
MONRDKEY2 := $F00A
