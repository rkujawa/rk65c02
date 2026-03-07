# Real 65C02 JIT Design

## Overview

The JIT backend compiles sequences of 65C02 instructions (basic blocks) into native code using GNU Lightning. The interpreter remains the reference implementation; the JIT replicates behaviour for performance. When debugging features are active (breakpoints, trace, runtime disassembly), execution uses the interpreter.

## Block Definition

- **Block**: A contiguous sequence of 65C02 instructions from a single entry PC until (and including) the first instruction that **modifies PC** (branch, JMP, JSR, RTS, RTI, BRK), or until a maximum length (64 instructions).
- **Block key**: Start PC; the cache is direct-mapped as `blocks[pc]`.
- **Compiled function**: One Lightning-generated function per block with signature `void block_fn(rk65c02emu_t *e)`. On return, `e->regs.PC` is the next PC. The C dispatcher looks up or compiles the block for that PC and calls it again.

## State and Offsets

All emulator state lives in `e->regs` and `e->bus`. Offsets are computed with `offsetof(struct rk65c02emu, regs)` and `offsetof(struct reg_state, A)` (and similarly for PC, P, SP, etc.). No raw numeric offsets are used.

## Register Allocation

- **JIT_V0**: Holds `e` (callee-saved, survives C calls). Loaded once at function entry via `jit_getarg`.
- **JIT_R0**: Working copy of `e`. Reloaded from JIT_V0 (`jit_movr(JIT_R0, JIT_V0)`) after any C function call (bus_read_1, bus_write_1, rk65c02_exec).
- **JIT_R1, JIT_R2**: Temporaries (caller-saved, clobbered by C calls).
- **JIT_V1, JIT_V2**: Callee-saved scratch for values that must survive C calls (effective addresses, modified values in read-modify-write sequences, flag accumulators in CMP/BIT).

## Memory Access

Memory is accessed via `bus_read_1(e->bus, addr)` and `bus_write_1(e->bus, addr, val)`. The JIT calls these from generated code using Lightning's `jit_prepare`, `jit_pushargr`, `jit_finishi`. Effective addresses for all modes are computed inline: ZP, ZPX, ZPY, ABSOLUTE, ABSOLUTEX, ABSOLUTEY use direct computation; IZP, IZPX, IZPY issue bus reads to resolve the indirect pointer.

## Bail-Out After Fallback

When the fallback path calls `rk65c02_exec` (for opcodes not yet native), the generated code checks `e->state` afterward. If the emulator is no longer RUNNING (e.g. STP, WAI, BRK changed the state), a forward branch skips the remaining block instructions and returns. All bail-out branches are patched to the block's shared return point.

## C Helpers

- **Bus**: `bus_read_1`, `bus_write_1` (from `bus.h`).
- **Fallback**: Any opcode not yet implemented natively triggers a call to `rk65c02_exec(e)` (single instruction) so behaviour stays correct while the opcode table is filled incrementally.

## GNU Lightning Lifecycle

- `init_jit(NULL)` is called once before any JIT state is created.
- One `jit_state_t *` per block so that `jit_emit()` returns a stable entry point.
- For each block: `jit_prolog()`, `jit_getarg(JIT_V0, arg)`, emit body, patch bail-outs, `jit_ret()`, `jit_epilog()`, `jit_emit()`.

## Coding Style

- K&R style consistent with the rest of the project.
- Use `offsetof` for all reg/state offsets; no magic numbers.
- Reuse `instruction_fetch`, `instruction_decode`, `instruction_modify_pc` for block formation so decode stays consistent with the interpreter.

## Opcode Implementation Matrix

| Category | Native | Fallback (rk65c02_exec) |
|----------|--------|------------------------|
| Flag modifiers | CLC, SEC, CLI, SEI, CLV, CLD, SED | |
| Transfer | TAX, TAY, TXA, TYA, TSX, TXS | |
| Inc/Dec reg | INX, DEX, INY, DEY | |
| Inc/Dec acc | INC A, DEC A | |
| Load imm | LDA, LDX, LDY (immediate) | |
| Load mem | LDA, LDX, LDY (ZP, ZPX/ZPY, ABS, ABSX/ABSY, IZP, IZPX, IZPY) | |
| Store | STA, STX, STY, STZ (all modes) | |
| Logic | AND, ORA, EOR (all modes) | |
| Compare | CMP, CPX, CPY (all modes) | |
| Inc/Dec mem | INC, DEC (ZP, ZPX, ABS, ABSX) | |
| Shift/Rotate acc | ASL, LSR, ROL, ROR (accumulator) | |
| Shift/Rotate mem | ASL, LSR, ROL, ROR (ZP, ZPX, ABS, ABSX) | |
| BIT | BIT (IMM, ZP, ZPX, ABS, ABSX) | |
| Stack | PHA, PLA, PHX, PLX, PHY, PLY, PHP, PLP | |
| Branch | BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS, BRA | |
| Jump | JMP absolute, JMP indirect (0x6C), JSR, RTS | RTI, BRK |
| Arithmetic | ADC, SBC (all modes; BCD via helper when P_DECIMAL) | |
| Misc | NOP | STP, WAI, BBR, BBS, RMB, SMB, TRB, TSB |

## ADC/SBC JIT

- **Binary mode**: Full inline: load A and operand, add with carry, compute overflow (A^res)&(M^res)&0x80, set C if sum > 0xFF, update N/Z from result. SBC uses A + (~M) + C with the same flag logic.
- **Decimal (BCD) mode**: When P_DECIMAL is set, the JIT calls `rk65c02_do_adc_bcd(e, operand)` or `rk65c02_do_sbc_bcd(e, operand)` so BCD semantics (from_bcd/to_bcd, carry/borrow rules) stay in C and match the interpreter. PC advance is done in the JIT after the helper returns.

## Resolved Issues

- **`stxi` argument order**: Lightning's `stxi(offset, base, value)` was initially called with swapped arguments, writing to random memory. Fixed by using the correct order: offset first, base register second, value register third.
- **Missing `init_jit()`**: GNU Lightning requires `init_jit(NULL)` before any JIT state creation. Without it, internal tables are uninitialized and code generation produces incorrect output.
- **`jit_getarg` after C calls**: Lightning's `jit_getarg` reads from the argument register (RDI on x86-64) without spilling to the stack. After any `jit_finishi` call, RDI is clobbered. Fixed by loading `e` into JIT_V0 (callee-saved) at function entry and using `jit_movr(JIT_R0, JIT_V0)` to reload after calls.
- **Block continuation after state change**: Without a bail-out check, the block would continue executing instructions after STP/WAI/BRK set `e->state != RUNNING`. Fixed by emitting a state check after each fallback call and branching to the return point if no longer running.
- **Stale JIT cache**: When a test loads a new ROM into the same emulator, previously compiled blocks contain stale code. Fixed by calling `rk65c02_jit_flush()` before loading a new ROM.
- **Branch condition polarity**: `jit_bmsi`/`jit_bmci` were swapped in all branch instructions, causing branches to be taken when they should not be and vice versa.
