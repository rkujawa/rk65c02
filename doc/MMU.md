# MMU: Host-Pluggable Memory Translation

The rk65c02 library provides a **host-facing MMU API**: one translation callback and optional fault callback, plus hooks to notify the library when mappings change. The library enforces permissions and keeps internal caches (TLB, JIT) coherent. The **guest-visible MMU** (bank registers, page tables, fault behaviour) is **not** defined by the library; the host implements it on top of this API.

---

## 1. Design goals

The MMU interface is designed to support:

| Goal | Meaning |
|------|--------|
| **Memory protection** | Host defines R/W/X per mapping; library enforces and reports violations. |
| **Virtual memory** | Unmapped or invalid access faults; host is notified and can fix the mapping and re-run (e.g. demand paging). |
| **Simple bank switching** | One or more 16-bit “windows” map to different physical banks; host decides when the mapping changes and notifies the library. |
| **Larger physical memory** | The API uses a **32-bit physical address** so the host can expose more than 64K of physical RAM/ROM; the guest still addresses 64K at a time. (Current bus and library only support paddr &lt; 64K; see §6.) |

The **single abstraction** is: one translation callback maps (virtual address, access type) → (physical address, permissions) or fault. The library caches results in an internal TLB and invalidates on mapping changes. For context-dependent mappings (e.g. “new bank only at entry address”), the host sets `no_fill_tlb` so that translation is not cached for that page.

---

## 2. Host API reference

All types and functions are declared in `rk65c02.h`. The emulator state (`rk65c02emu_t`) holds MMU-related fields used after a fault (§7).

### 2.1 Enabling and clearing the MMU

**`rk65c02_mmu_set`**

```c
bool rk65c02_mmu_set(rk65c02emu_t *e,
    rk65c02_mmu_translate_cb_t translate, void *translate_ctx,
    rk65c02_mmu_fault_cb_t on_fault, void *fault_ctx,
    bool enabled, bool identity_fastpath);
```

- **`translate`** — Translation callback; must be non-NULL when `enabled` is true.
- **`translate_ctx`** — Opaque pointer passed to `translate` as `ctx`.
- **`on_fault`** — Optional fault callback; may be NULL.
- **`fault_ctx`** — Opaque pointer passed to `on_fault`.
- **`enabled`** — If true, MMU is active and `translate` is used for every memory access (subject to TLB hits).
- **`identity_fastpath`** — If true, the host asserts that the current mapping is identity (vaddr = paddr) with full R/W/X; the library may skip translation overhead until the next mapping update.
- **Returns** — true on success, false on invalid arguments.

**`rk65c02_mmu_clear`**

```c
void rk65c02_mmu_clear(rk65c02emu_t *e);
```

Removes MMU callbacks and reverts to direct memory access (no translation).

---

### 2.2 Translation callback

**Type**

```c
typedef rk65c02_mmu_result_t (*rk65c02_mmu_translate_cb_t)(
    rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access, void *ctx);
```

- **`vaddr`** — Guest (virtual) address, 0–65535.
- **`access`** — One of:
  - `RK65C02_MMU_FETCH` — instruction fetch
  - `RK65C02_MMU_READ` — data read
  - `RK65C02_MMU_WRITE` — data write
- **`ctx`** — The `translate_ctx` passed to `rk65c02_mmu_set`.

**Result type `rk65c02_mmu_result_t`**

| Field | Meaning |
|-------|--------|
| **`ok`** | true if the access is allowed; false means translation/permission fault. |
| **`paddr`** | Physical address (32-bit). When `ok` is true, must be in range [0, bus size − 1]. Today the library rejects paddr ≥ 64K; see §6. |
| **`perms`** | Bitmask: `RK65C02_MMU_PERM_R`, `RK65C02_MMU_PERM_W`, `RK65C02_MMU_PERM_X`. The library checks that the requested `access` is permitted by `perms`. |
| **`fault_code`** | Host-defined value reported on fault (e.g. for logging or guest-visible status). |
| **`no_fill_tlb`** | When true, this translation is **not** stored in the internal TLB. Use for pages whose effective mapping depends on context; see §3.2. |

If `ok` is false, or `paddr` is out of range, or permissions do not allow the access, the library calls the fault callback (if set), sets `e->mmu_last_fault_*`, and stops execution with stop reason `EMUERROR`.

The callback should be **side-effect free** and **deterministic** for the same `(vaddr, access)` and host mapping state; the library may cache translations unless `no_fill_tlb` is true.

---

### 2.3 Fault callback (optional)

**Type**

```c
typedef void (*rk65c02_mmu_fault_cb_t)(
    rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    uint16_t fault_code, void *ctx);
```

Invoked when a translation or permission check fails, immediately before execution stops. The host can log the event or update guest-visible fault registers. Fault details are also in `e->mmu_last_fault_addr`, `e->mmu_last_fault_access`, `e->mmu_last_fault_code`.

---

### 2.4 Notifying mapping changes

When the host changes how virtual addresses map to physical (e.g. bank switch, task switch), it must notify the library so the internal TLB and JIT stay coherent:

1. **`rk65c02_mmu_begin_update(e)`** — Start a batch of mapping changes.
2. For each affected virtual page or range:
   - **`rk65c02_mmu_mark_changed_vpage(e, vpage)`** — `vpage` is the high byte of the virtual address (0–255).
   - **`rk65c02_mmu_mark_changed_vrange(e, start, end)`** — Inclusive range; use for wide or wrapped ranges.
3. **`rk65c02_mmu_end_update(e)`** — Apply: TLB entries for marked pages are flushed; JIT blocks whose **code** lies on those pages are invalidated (data accesses go through the callback, so only code-page invalidation is needed).

For a full remap, mark a range that covers 0x0000–0xFFFF; the library performs a full TLB flush and full JIT invalidation.

---

### 2.5 TLB (internal, transparent)

The library uses an internal software TLB (one entry per virtual page). It is **transparent**: the host does not need to disable it for correct behaviour. When the host calls `begin_update` / `mark_changed_*` / `end_update`, the library flushes the affected TLB entries. For context-dependent mappings, the host sets `no_fill_tlb` in the translate result (§3.2).

- **`rk65c02_mmu_tlb_set(e, enabled)`** — Enable or disable the internal TLB. Default is enabled. Disabling is for debugging or special cases (higher overhead).
- **`rk65c02_mmu_tlb_flush(e)`** — Flush all TLB entries.
- **`rk65c02_mmu_tlb_flush_vpage(e, vpage)`** — Flush one virtual page.

The host does not need to flush manually when using `begin_update` / `mark_changed_*` / `end_update`.

---

## 3. Intended usage

### 3.1 Translation callback contract

- For each guest memory access (fetch, read, write), the library calls the translate callback (or uses a cached TLB result). The callback returns a physical address and permissions, or a fault.
- The library enforces: fetch requires execute (X), read requires read (R), write requires write (W). If the callback returns a mapping that does not allow the access, the library faults.
- The callback must be deterministic and side-effect free for the same inputs and host mapping state; the library may cache the result unless `no_fill_tlb` is true.

### 3.2 When to set `no_fill_tlb`

The internal TLB caches **one translation per virtual page**. That is correct when the mapping for a page is stable until the host calls `end_update`. It is **wrong** when the **effective** mapping for the same page depends on **which address** is accessed or on a “phase” (e.g. “has the guest entered at the new bank?”).

**Example:** Bank-switched cart. The host’s policy is “apply the new bank only when the guest fetches from the cart **entry** (e.g. 0x8000).” The same page contains both the old code (e.g. a JMP at 0x8008) and the entry. After the host flushes the TLB and the guest runs that JMP, the next fetch is from 0x8000. If the fetch of the JMP at 0x8008 had been cached, the TLB would say “page 0x80 → physical 0x80” for the whole page, so the fetch from 0x8000 would hit the TLB and never call the callback — the new bank would never apply. **Fix:** Set `no_fill_tlb` for every translation in the cart window. Then each access to that window calls the callback and sees the current state. Cost is more callbacks for that window only.

**Typical use:** Bank-switched cart or overlay where the new bank applies only when the guest jumps to the entry address. The guest writes the bank id to a register and JMPs to the entry; the host polls the register and flushes on change; the callback must see the new bank on the very next fetch at the entry. Mark that window with `no_fill_tlb`.

### 3.3 Handling faults and “resume”

On fault, the library calls the fault callback (if set), sets `e->mmu_last_fault_addr`, `e->mmu_last_fault_access`, `e->mmu_last_fault_code`, and stops with stop reason `EMUERROR`. The CPU state (including `e->regs.PC`) is left so that the **faulting instruction** is the one that would execute next. There is no separate “resume” call: to continue after handling a fault (e.g. demand paging), the host updates its mapping, calls `begin_update` / `mark_changed_vpage` / `end_update`, then calls `rk65c02_start()` or `rk65c02_step()` again. The same instruction re-executes and now translates successfully.

---

## 4. Guest contract (host-defined)

The library does **not** define what the guest sees (bank registers, page tables, fault vectors, etc.). The host implements that contract using the API above. Two concrete patterns are implemented as examples:

**Bank-switched cart (C64-style)** — A fixed window (e.g. $8000–$BFFF) is backed by one of several banks. The guest writes the bank id to a control address (e.g. $DE00); the host polls that address (e.g. in the tick callback), updates its “current bank,” calls `begin_update` / `mark_changed_vpage` / `end_update`, and the translate callback maps the window to the chosen bank. See `examples/mmu_cart`.

**Minimal multitasking** — Low addresses are per-task, high addresses shared. The guest yields by writing the next task id to a “yield” address; the host sees it (e.g. in the tick callback), sets the current task, calls `begin_update` / `mark_changed_vpage` / `end_update`, and the translate callback maps low addresses to the current task’s region. See `examples/mmu_multitasking`.

Other possibilities (all host-defined): MMIO “MMU” device with registers for page table base and fault status; syscall-based mapping changes; multiple bank registers per region. The library API stays the same.

---

## 5. Faults and stop reason

On translation failure or permission violation, execution stops with **`emu_stop_reason_t` = `EMUERROR`**. The host can identify an MMU fault by MMU being enabled and by reading `e->mmu_last_fault_addr`, `e->mmu_last_fault_access`, `e->mmu_last_fault_code`. There is no separate `MMUFAULT` reason; the host defines fault taxonomy (e.g. page fault vs. protection fault) via `fault_code` and the fault callback.

---

## 6. Does the host have enough?

| Goal | Verdict |
|------|--------|
| **Memory protection** | Yes. Permissions, enforcement, fault callback, and `mmu_last_fault_*` plus `e->regs.PC` at stop give the host what it needs. |
| **Virtual memory** | Yes. “Resume” = update mapping and call `start`/`step` again; the same instruction re-executes. Enough for classic demand paging. |
| **Bank switching** | Yes. Translate + update notifications + `no_fill_tlb` when the new bank applies only at an entry address. Host learns about guest “bank register” writes by polling (e.g. tick) or a custom bus device. |
| **Physical memory &gt; 64K** | **Not yet.** The API uses a 32-bit `paddr`, but the current bus API uses 16-bit addresses and the library rejects `paddr >= RK65C02_BUS_SIZE` (64K). Supporting larger physical memory would require extending the bus and the library check. |

---

## 7. Fault state in `rk65c02emu_t`

After an MMU fault, the following fields are set before the fault callback and before run returns:

- **`mmu_last_fault_addr`** — Virtual address that caused the fault.
- **`mmu_last_fault_access`** — Access type (`RK65C02_MMU_FETCH`, `_READ`, or `_WRITE`).
- **`mmu_last_fault_code`** — The `fault_code` from the translate result (when `ok` was false) or a library code (e.g. permission violation, paddr out of range).

`e->regs.PC` still points at the faulting instruction.

---

## 8. Performance

- **MMU off** — Baseline (no translation).
- **MMU on, TLB on, stable mapping** — About 15% overhead; translate is called only on TLB miss.
- **MMU on, TLB off** — About 60% overhead; every access calls the callback.
- **Remap-heavy** — With batched updates and code-page-only JIT invalidation, remap-heavy workloads stay close to “MMU on, TLB on” when the host uses `begin_update` / `mark_changed_*` / `end_update` and the tick interval for remaps is not per-instruction.

**Benchmark:** `test/bench_mmu` reports ns/instruction for baseline, MMU on/off, TLB on/off, and remap-heavy. Representative values (after tuning): baseline ~40–42 ns/insn, MMU+TLB on ~46, MMU+TLB off ~65–67, remap-heavy ~41–42.

**Tests:** `test/test_mmu`; run with the test suite (e.g. `kyua test`) or run the `test_mmu` program directly.

---

## 9. Implementation notes

- The internal TLB is per virtual page; epoch-based coherence; targeted flush on `end_update`.
- On mapping change, only JIT blocks whose **code** lies on changed pages are invalidated (data accesses go through the callback).
- The TLB and JIT are invisible to the guest; the host’s translate callback remains the single source of truth.
