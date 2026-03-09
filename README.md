# rk65c02
65C02 code interpreter/emulator/disassembler.

![rk65c02 logo](https://raw.githubusercontent.com/rkujawa/rk65c02/master/res/rk65c02_small.png)

This project provides a library implementing a complete JIT
emulator of WDC 65C02S CPU. 

It does not aim to be a cycle-exact emulator, but
otherwise it tries to mimic behaviour of 65C02S as close as possible. 

Currently, the following features are implemented:
- Emulation of all opcodes, including WDC extensions and BCD mode.
- Optional JIT using GNU Lightning.
- Support for interrupts.
- Host callbacks for stop notification and periodic execution ticks.
- Optional idle-wait callback integration for `WAI`-driven guest idle loops.
- Optional host-defined MMU: translation and fault callbacks, software TLB, and JIT-coherent invalidation so the host can implement bank switching, memory expansion beyond 64k, or multitasking without the guest depending on library internals. See [MMU documentation](doc/MMU.md) and the `examples/mmu_cart` and `examples/mmu_multitasking` examples.

External dependencies (besides standard C library):

- **Library/runtime**
  - Boehm GC
  - uthash
- **Optional JIT support**
  - GNU Lightning — when available, the library can use it for optional JIT compilation; the library can be built **without** Lightning (interpreter only, JIT not possible). See "Building with or without JIT" below.
- **Test/tooling**
  - `kyua` quality assurance toolkit
  - `atf` testing framework
  - `vasm` (6502 std syntax, recent snapshot such as 1.8f+) for assembling test and examples ROMs
  - `ca65` for both Klaus Dormann test suites (`6502_functional_test` and
    `65C02_extended_opcodes_test`)

On Fedora, common packages include `gc-devel`, `uthash-devel`, `lightning`,
`lightning-devel`, `kyua`, `atf`, and `cc65` (for `ca65`).

[![Built by neckbeards](https://forthebadge.com/images/badges/built-by-neckbeards.svg)](https://forthebadge.com)

## Host control API

Typical host integration pattern is:

1. Create and configure `rk65c02emu_t`.
2. Register optional callbacks:
   - `rk65c02_on_stop_set()` to be notified why execution stopped.
   - `rk65c02_tick_set()` to periodically run host code while interpreter runs.
   - `rk65c02_idle_wait_set()` to let host sleep/block while guest is in `WAI`.
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
- Idle-wait callback is only consulted when CPU stops with `WAI`; it is not
  used for `STP`.
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
- `idle_wait` - demonstrates `rk65c02_idle_wait_set()` by sleeping on host
  while guest executes `WAI`, then waking via IRQ assertion.
- `interrupts` - IRQ vector at $FFFE, host asserts IRQ from idle_wait callback,
  minimal guest handler; demonstrates `rk65c02_assert_irq` / `rk65c02_deassert_irq`.
- `hello_serial` - custom bus device at $DE00: guest writes bytes, host device
  callback prints to stdout (no MMU); demonstrates `bus_device_add`.
- `stepper` - step-by-step execution with `rk65c02_step(e, 1)`, prints regs and
  disassembly each step; demonstrates state inspection and optional tracing.
- `jit_bench` - runs the same workload (min3.rom) with JIT on and off, reports
  wall time; compares interpreter vs JIT performance.
- `breakpoints` - sets a breakpoint with `debug_breakpoint_add`, runs until
  BREAKPOINT stop, prints regs and disassembly, removes breakpoint and continues.
- **MMU examples** (see [doc/MMU.md](doc/MMU.md) for the full guide):
  - `mmu_cart` - C64-style bank-switched cartridge: guest writes bank id to 0xDE00,
    host polls in the tick callback and remaps the cart window via the MMU API.
  - `mmu_multitasking` - minimal task switching: two tasks with private low memory,
    guest yields by writing next task id to 0xFF00, host remaps and continues.
  - `tinyos` - Tiny OS scheduler: three tasks in extended physical RAM (above 64K),
    first 32KB virtual per task, cooperative yield and WAI+IRQ-driven switch,
    console at $DE00. See [doc/MMU.md](doc/MMU.md) §Tiny OS.
  - `mmu_pae` - PAE-like one-level page table, extended physical addresses, and
    demand paging (fault → install mapping → restart); runs with JIT enabled.
  - `mmu_mpu` - simple MPU: flat 64K with programmable protection regions, MMIO
    registers at $FE00, violation triggers IRQ; guest handler can clear and fix perms.
  All are heavily commented (host and guest) to show the rk65c02↔host interface
  and how the host can define the contract between emulated hardware and guest code.
  Run MMU examples that may loop (e.g. `mmu_mpu`) with a timeout: `timeout 5 ./mmu_mpu`.
- **MS BASIC** (`examples/msbasic/`) — runs Microsoft BASIC (mist64/msbasic) in the emulator;
  terminal I/O via a bus device at $F000, JIT supported. Requires ca65/ld65 and the msbasic
  git submodule. See [examples/msbasic/README.md](examples/msbasic/README.md).

**Building with or without JIT**

- **With JIT (default):** `make -C src` then `make -C examples` (and optionally `make -C test`). Requires GNU Lightning; `rk65c02_jit_enable(&e, true)` enables JIT at run time.
- **Without JIT:** Build the library without Lightning so it does not depend on it and JIT is unavailable: `make -C src HAVE_LIGHTNING=0`. Then build examples and tests with `make -C examples NO_LIGHTNING=1` and `make -C test NO_LIGHTNING=1` so they do not link with Lightning. `rk65c02_jit_enable()` is a no-op in that build; execution is always interpreter.

Build examples with:

```sh
make -C src
make -C examples
```

Run examples from the `examples/` directory (e.g. `./min3`) so ROM files are found; or use `make run-<name>` from the repo root (e.g. `make -C examples run-min3`).

