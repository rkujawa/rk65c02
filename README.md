# rk65c02
65C02 code interpreter/emulator/disassembler.

![rk65c02 logo](https://raw.githubusercontent.com/rkujawa/rk65c02/master/res/rk65c02_small.png)

This project provides a library implementing a farily complete
emulator of WDC 65C02S CPU. It does not aim to be cycle-exact emulator, but
otherwise it tries to mimic behaviour of 65C02S as close as possible. 
Currently, the following features are implemented:
- Emulation of all opcodes, including WDC extensions and BCD mode.
- 16-bit address space.
- Minimal support for interrupts.

The following notable features are missing:
- Ability to execute callbacks in software utilizing this library.
- Just-in-Time translation.

The only external dependencies (besides standard C library) are Boehm GC and
uthash.
On Fedora these can be installed with `gc-devel` and `uthash-devel` packages.

If you want to build tests, `kyua` quality assurance toolkit, `atf` testing
framework and a recent snapshot (1.8f or newer) of `vasm` assembler (6502
with std syntax) are also necessary.

[![Built by neckbeards](https://forthebadge.com/images/badges/built-by-neckbeards.svg)](https://forthebadge.com)

