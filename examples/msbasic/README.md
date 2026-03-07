# MS BASIC example

This example runs [Microsoft BASIC](https://github.com/mist64/msbasic) (6502 port by mist64) inside the rk65c02 emulator. You can type BASIC programs and run them, with terminal I/O and optional Ctrl-C break support.

## Prerequisites

- **ca65 / ld65** (cc65 toolchain) for assembling and linking the BASIC ROM and I/O stub
- **Git** with the `msbasic` submodule initialised
- **librk65c02** built (from `src/`); JIT is used when available (Lightning)

## Submodule and patches

The BASIC source is included as a git submodule. After cloning the repo, initialise and update it:

```bash
git submodule update --init examples/msbasic/msbasic
```

The example applies small patches to the submodule so it can build the `rk65c02` platform. Patches are under `examples/msbasic/patches/` and are applied automatically when you run `make` in `examples/msbasic/` (they add the rk65c02 branch to `defines.s`, `extra.s`, `iscntc.s`, `loadsave.s`, and `init.s`). The `init.s` patches set the output vector (GOSTROUT) to STROUT for rk65c02 so that prompts and output work correctly; `init.s.patch` applies the first three changes and `init.s.hunk4.patch` applies the fourth (run in that order).

## Build

From the project root or from `examples/`:

```bash
make msbasic
```

Or from this directory:

```bash
make
```

This builds:

- **basic.bin** — BASIC ROM (loaded at $B000)
- **iostub.bin** — I/O stub (loaded at $F002)
- **run_msbasic** — host program

The host links against `../../src/librk65c02.a`. To build without JIT (e.g. if Lightning is not installed), build the library with `NO_LIGHTNING=1` and then build this example with `make NO_LIGHTNING=1`.

## Run

From `examples/msbasic/`:

```bash
./run_msbasic
```

On startup, BASIC will prompt for **MEMORY SIZE** and **TERMINAL WIDTH**. You can press Enter to accept defaults, or type values (e.g. `65535` then Enter for memory, then `72` then Enter for width). After that you get the BASIC prompt and can type programs:

```
PRINT 1+1
RUN
```

To exit: type `END` and press Enter, or press Ctrl-C (if supported; the host sets bit 0 of the status port $F001 when SIGINT is received so that BASIC’s ISCNTC can break).

## I/O and memory map

- **$0000–$7FFF** — 32 KB RAM (program, variables, input buffer)
- **$B000–$DFFF** — BASIC ROM and init (from ld65 `BASROM` segment)
- **$F000** — I/O data port: **write** = character output (host `putchar`), **read** = character input (host `getchar`, blocking)
- **$F001** — Status port: **read** returns bit 0 = 1 when Ctrl-C has been pressed (for ISCNTC)
- **$F002–$F00F** — I/O stub code: MONCOUT ($F002), MONRDKEY ($F006), MONRDKEY2 ($F00A); these access $F000 for actual I/O

Entry point is **COLD_START**, read from `basic.lbl` at build time.

## JIT

The host enables the emulator JIT with `rk65c02_jit_enable(&e, true)`. I/O is performed via the bus device at $F000; when the guest executes the stub at $F002 (e.g. STA $F000), the write is handled by the device and the host’s `putchar` runs. Blocking `getchar` in the device is fine while the emulator is running.

## Optional timer

BASIC’s RND uses an in-memory seed (RNDSEED); no hardware timer is required for basic operation. The plan allowed for an optional jiffy counter at $F002/$F003; this example does not implement it. You can add a timer device and drive it from a tick or idle callback if you want time-based RND or similar.

## Files in this directory

| File | Purpose |
|------|--------|
| `defines_rk65c02.s` | Platform defines (ZP, CONFIG_*, MONCOUT/MONRDKEY addresses) |
| `rk65c02_extra.s` | EXTRA segment (minimal) |
| `rk65c02_iscntc.s` | ISCNTC (Ctrl-C via $F001) |
| `rk65c02_loadsave.s` | SAVE/LOAD stubs (messages only) |
| `rk65c02_io.s` | I/O stub assembly (MONCOUT, MONRDKEY, MONRDKEY2 at $F002) |
| `rk65c02.cfg` / `iostub.cfg` | ld65 configs for BASIC and stub |
| `patches/*.patch` | Patches applied to the msbasic submodule |
| `msbasic_host.c` | Host program (bus, device, load ROM/stub, run with JIT) |
| `Makefile` | Build BASIC, stub, and host |

## References

- [mist64/msbasic](https://github.com/mist64/msbasic) — Microsoft 6502 BASIC source (submodule)
- rk65c02 examples: `hello_serial`, `jit_bench` for bus device and JIT usage
