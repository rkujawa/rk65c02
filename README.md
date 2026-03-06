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
- JIT using GNU Lightning.
- Host callbacks for stop notification and periodic execution ticks.

The only external dependencies (besides standard C library) are Boehm GC and
uthash. GNU Lightning is required for JIT support, but the library can be built 
without it.
On Fedora these can be installed with `gc-devel`, `uthash-devel`, 
`lightning` and `lightning-devel` packages.

If you want to build tests, `kyua` quality assurance toolkit, `atf` testing
framework and a recent snapshot (1.8f or newer) of `vasm` assembler (6502
with std syntax) are also necessary.

[![Built by neckbeards](https://forthebadge.com/images/badges/built-by-neckbeards.svg)](https://forthebadge.com)

## Host control API

Typical host integration pattern is:

1. Create and configure `rk65c02emu_t`.
2. Register optional callbacks:
   - `rk65c02_on_stop_set()` to be notified why execution stopped.
   - `rk65c02_tick_set()` to periodically run host code while interpreter runs.
3. Start execution with `rk65c02_start()` (or bounded execution with `rk65c02_step()`).
4. Inspect or modify state (`e.regs`, bus reads/writes) and continue.

Host can request cooperative stop using:

- `rk65c02_request_stop(&e)` - asks emulator to stop at the next safe boundary.
  Stop reason is reported as `HOST`.

Notes:

- Tick callback works in both execution modes:
  - interpreter mode checks tick after each instruction;
  - JIT mode checks tick at compiled block boundaries (coarser granularity).
- If precise per-instruction callback cadence is required, run without JIT.
- `on_stop` is called when execution stops from `rk65c02_start()` and
  `rk65c02_step()` (for example: `STP`, `WAI`, `BREAKPOINT`, `HOST`,
  `STEPPED`, `EMUERROR`).
- `rk65c02_stop_reason_string()` converts `emu_stop_reason_t` values to
  readable strings for logs/UI.

## Examples

`examples/` contains small host programs using the library:

- `min3` - computes minimum of three values using a ROM routine.
- `mul_8bit_to_8bits` - multiplies two 8-bit values.
- `host_control` - demonstrates full host-control flow with:
  - `rk65c02_on_stop_set()` callback.
  - `rk65c02_tick_set()` callback.
  - host-driven stop via `rk65c02_request_stop()`.
  - continuing with `rk65c02_step()` after stop.

Build examples with:

```sh
make -C src
make -C examples
```

