/** @file rk65c02.h
 *  @brief Public functions for managing rk65c02 emulator.
 */
#ifndef _RK6502_H_
#define _RK6502_H_

#include "bus.h"

struct rk65c02_jit;
struct rk65c02emu;
typedef struct rk65c02emu rk65c02emu_t;

/**
 * @brief State of the emulator.
 */
typedef enum {
	STOPPED,	/**< Stopped. */
	RUNNING,	/**< Running. */
	STEPPING	/**< Stepping. */
} emu_state_t;

/**
 * @brief Enum describing why emulation stopped.
 */
typedef enum {
	STP,		/**< Due to 65C02 STP instruction. */
	WAI,		/**< Waiting for interrupt (WAI instruction). */
	BREAKPOINT,	/**< Due to breakpoint set. */
	WATCHPOINT,	/**< Due to watchpoint set (not implemented). */
	STEPPED,	/**< Stepped appropriate number of instructions. */
	HOST,		/**< Due to host stop function called. */
	EMUERROR	/**< Due to emulator error. */
} emu_stop_reason_t;

/**
 * @brief Callback executed when emulation stops.
 */
typedef void (*rk65c02_on_stop_cb_t)(rk65c02emu_t *e, emu_stop_reason_t reason,
    void *ctx);

/**
 * @brief Callback executed periodically while emulation is active.
 *
 * Called from host control polling points in both interpreter and JIT paths.
 * Cadence is therefore boundary-based, not guaranteed per instruction.
 */
typedef void (*rk65c02_tick_cb_t)(rk65c02emu_t *e, void *ctx);

/**
 * @brief Callback executed when CPU is stopped in WAI idle state.
 *
 * Typical callback behavior is to block until the next host/device event and
 * then wake the core (usually via rk65c02_assert_irq()).
 */
typedef void (*rk65c02_wait_cb_t)(rk65c02emu_t *e, void *ctx);

/**
 * @brief MMU access class for translation/permission checks.
 */
typedef enum {
	RK65C02_MMU_FETCH = 0, /**< Instruction fetch access. */
	RK65C02_MMU_READ,      /**< Data read access. */
	RK65C02_MMU_WRITE      /**< Data write access. */
} rk65c02_mmu_access_t;

#define RK65C02_MMU_PERM_R 0x01
#define RK65C02_MMU_PERM_W 0x02
#define RK65C02_MMU_PERM_X 0x04

/**
 * @brief Translation result returned by host MMU callback.
 */
typedef struct {
	bool ok;		/**< False indicates translation/access fault. */
	uint32_t paddr;		/**< Translated physical address on emulator bus. */
	uint8_t perms;		/**< Permission mask, see RK65C02_MMU_PERM_* bits. */
	uint16_t fault_code;	/**< Host-defined reason code when ok=false. */
	/** When true, this translation is not cached in the internal TLB.
	 *  Use for mappings that depend on context (e.g. bank switch at entry only)
	 *  so the TLB stays transparent and correct without host disabling it. */
	bool no_fill_tlb;
} rk65c02_mmu_result_t;

/**
 * @brief Host MMU translation callback.
 */
typedef rk65c02_mmu_result_t (*rk65c02_mmu_translate_cb_t)(
    rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access, void *ctx);

/**
 * @brief Optional callback invoked when an MMU fault occurs.
 */
typedef void (*rk65c02_mmu_fault_cb_t)(
    rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    uint16_t fault_code, void *ctx);

typedef uint8_t (*rk65c02_mem_read_cb_t)(
    rk65c02emu_t *e, uint16_t addr, rk65c02_mmu_access_t access);
typedef void (*rk65c02_mem_write_cb_t)(
    rk65c02emu_t *e, uint16_t addr, uint8_t val, rk65c02_mmu_access_t access);

struct rk65c02_mmu_tlb_entry {
	bool valid;
	uint8_t vpage;
	uint16_t ppage_base;
	uint8_t perms;
	uint32_t epoch;
};

/**
 * @brief State of the emulated CPU registers.
 */
struct reg_state {
	uint8_t A;	/**< Accumulator. */
	uint8_t X;      /**< Index X. */
	uint8_t Y;      /**< Index Y. */

	uint16_t PC;    /**< Program counter. */
	uint8_t SP;     /**< Stack pointer. */
	uint8_t P;      /**< Status. */
};

typedef struct reg_state reg_state_t;

#define P_CARRY 0x1
#define P_ZERO 0x2
#define P_IRQ_DISABLE 0x4	/**< Status register flag: IRQ disabled */
#define P_DECIMAL 0x8		/**< Status register flag: BCD mode */
#define P_BREAK 0x10		/**< Status register flag: BRK was the cause of interrupt */
#define P_UNDEFINED 0x20	/**< Status register flag: Undefined (always 1) */
#define P_SIGN_OVERFLOW 0x40
#define P_NEGATIVE 0x80

#define NEGATIVE P_NEGATIVE /* sign bit */

#define STACK_START 0x0100
#define STACK_END 0x01FF

#define VECTOR_IRQ 0xFFFE	/* also used for BRK */

#define BIT(val,n) ((val) & (1 << (n)))

typedef struct breakpoint_t {
	uint16_t address;
	struct breakpoint_t *next;
} breakpoint_t;

typedef struct trace_t {
	uint16_t address;
	uint8_t opcode, op1, op2;
	reg_state_t regs;
	struct trace_t *prev,*next;
} trace_t;

/**
 * @brief Instance of the emulator.
 */
struct rk65c02emu {
	emu_state_t state;	/**< Current emulator status. */
	bus_t *bus;		/**< Bus to which CPU is attached. */
	reg_state_t regs;	/**< CPU registers. */
	emu_stop_reason_t stopreason; /**< Reason for stopping emulation. */
	bool irq;		/**< Interrupt request line state, true is asserted. */

	breakpoint_t *bps_head;	/**< Pointer to linked list with breakpoints. */
	bool runtime_disassembly; /**< Disassemble code when emulator is running. */
	bool trace;		/**< Tracing mode enable/disable. */
	trace_t *trace_head;	/**< Pointer to linked list with trace log. */
	bool use_jit;		/**< Current JIT execution state (may change at runtime). */
	bool jit_requested;	/**< Host-requested JIT preference across start() calls. */
	struct rk65c02_jit *jit; /**< Opaque JIT backend state. */
	bool stop_requested;	/**< Host requested stop at next safe boundary. */
	rk65c02_on_stop_cb_t on_stop; /**< Callback executed after stop. */
	void *on_stop_ctx;	/**< Host context for on_stop callback. */
	rk65c02_tick_cb_t tick;	/**< Periodic host tick callback. */
	void *tick_ctx;		/**< Host context for tick callback. */
	uint32_t tick_interval; /**< Poll-count period (0 = every poll boundary). */
	uint32_t tick_countdown; /**< Internal countdown for periodic ticks. */
	bool idle_wait_enabled;	/**< Enable host wait callback while STOPPED/WAI. */
	rk65c02_wait_cb_t idle_wait; /**< Host wait callback for WAI idle state. */
	void *idle_wait_ctx;	/**< Host context for wait callback. */
	bool mmu_enabled;	/**< Host MMU translation path enabled. */
	bool mmu_identity_fastpath; /**< Host allows identity-map fast path bypass. */
	bool mmu_identity_active; /**< Current map is identity+RWX (bypass active). */
	bool mmu_update_pending; /**< Host signaled pending MMU mapping changes. */
	uint32_t mmu_epoch;	/**< Monotonic epoch for MMU map changes. */
	rk65c02_mmu_translate_cb_t mmu_translate; /**< Host MMU translate callback. */
	void *mmu_translate_ctx; /**< Host context for MMU translation callback. */
	rk65c02_mmu_fault_cb_t mmu_fault; /**< Optional host callback on MMU fault. */
	void *mmu_fault_ctx;	/**< Host context for MMU fault callback. */
	uint16_t mmu_last_fault_addr; /**< Last virtual address that faulted. */
	rk65c02_mmu_access_t mmu_last_fault_access; /**< Last fault access class. */
	uint16_t mmu_last_fault_code; /**< Last host-defined MMU fault code. */
	bool mmu_tlb_enabled;	/**< Enable internal software MMU TLB. */
	bool mmu_changed_all;	/**< MMU update changed broad mappings. */
	bool mmu_changed_vpage[256]; /**< MMU update touched specific virtual pages. */
	uint64_t mmu_tlb_hits;	/**< MMU TLB hits since last reset. */
	uint64_t mmu_tlb_misses; /**< MMU TLB misses since last reset. */
	struct rk65c02_mmu_tlb_entry mmu_tlb[256]; /**< Direct-mapped virtual-page TLB. */
	rk65c02_mem_read_cb_t mem_read_1_fn; /**< Active memory-read backend. */
	rk65c02_mem_write_cb_t mem_write_1_fn; /**< Active memory-write backend. */
};

/**
 * @brief Initialize the new emulator instance. Set initial CPU state.
 * @param b Bus description.
 * @return New emulator instance.
 */
rk65c02emu_t rk65c02_init(bus_t *b);

/**
 * @brief Start the emulator.
 * @param e Emulator instance.
 */
void rk65c02_start(rk65c02emu_t *e);

/**
 * @brief Execute as many instructions as specified in steps argument.
 * @param e Emulator instance.
 * @param steps Number of instructions to execute.
 */
void rk65c02_step(rk65c02emu_t *e, uint16_t steps);

/**
 * @brief Convert stop reason enum value to a static string.
 * @param reason Stop reason value.
 * @return Human-readable reason name.
 */
const char *rk65c02_stop_reason_string(emu_stop_reason_t reason);

/**
 * @brief Register callback executed whenever emulation stops.
 * @param e Emulator instance.
 * @param cb Callback function.
 * @param ctx Opaque host context passed back to callback.
 */
void rk65c02_on_stop_set(rk65c02emu_t *e, rk65c02_on_stop_cb_t cb, void *ctx);

/**
 * @brief Unregister on-stop callback.
 * @param e Emulator instance.
 */
void rk65c02_on_stop_clear(rk65c02emu_t *e);

/**
 * @brief Register periodic host tick callback.
 * @param e Emulator instance.
 * @param cb Callback function.
 * @param interval Number of host-poll boundaries between callbacks
 *    (0 = every poll boundary).
 * @param ctx Opaque host context passed back to callback.
 */
void rk65c02_tick_set(rk65c02emu_t *e, rk65c02_tick_cb_t cb,
    uint32_t interval, void *ctx);

/**
 * @brief Unregister tick callback.
 * @param e Emulator instance.
 */
void rk65c02_tick_clear(rk65c02emu_t *e);

/**
 * @brief Enable host wait callback for WAI idle periods.
 * @param e Emulator instance.
 * @param cb Callback function; when NULL, waits are disabled.
 * @param ctx Opaque host context passed back to callback.
 *
 * The callback is invoked only when state is STOPPED and stop reason is WAI.
 * Callback implementations may block, but should return once a wake event is
 * delivered or a host shutdown/stop condition is observed.
 */
void rk65c02_idle_wait_set(rk65c02emu_t *e, rk65c02_wait_cb_t cb, void *ctx);

/**
 * @brief Disable host wait callback for WAI idle periods.
 * @param e Emulator instance.
 */
void rk65c02_idle_wait_clear(rk65c02emu_t *e);

/**
 * @brief Configure host MMU translation callbacks.
 * @param e Emulator instance.
 * @param translate Translation callback (must not be NULL when enabled).
 * @param translate_ctx Opaque host context for translation callback.
 * @param on_fault Optional fault callback (may be NULL).
 * @param fault_ctx Opaque host context for fault callback.
 * @param enabled Enable or disable MMU translation.
 * @param identity_fastpath True if current mapping is identity+RWX.
 * @return true on success, false on invalid arguments.
 */
bool rk65c02_mmu_set(rk65c02emu_t *e, rk65c02_mmu_translate_cb_t translate,
    void *translate_ctx, rk65c02_mmu_fault_cb_t on_fault, void *fault_ctx,
    bool enabled, bool identity_fastpath);

/**
 * @brief Clear MMU callbacks and return to direct memory access.
 * @param e Emulator instance.
 */
void rk65c02_mmu_clear(rk65c02emu_t *e);

/**
 * @brief Begin host-coordinated MMU mapping update batch.
 * @param e Emulator instance.
 */
void rk65c02_mmu_begin_update(rk65c02emu_t *e);

/**
 * @brief Mark that MMU mapping changed for a virtual page.
 * @param e Emulator instance.
 * @param vpage Virtual page index (phase-1 semantic marker only).
 */
void rk65c02_mmu_mark_changed_vpage(rk65c02emu_t *e, uint8_t vpage);

/**
 * @brief Mark that MMU mapping changed for a virtual address range.
 * @param e Emulator instance.
 * @param start Inclusive virtual start address.
 * @param end Inclusive virtual end address.
 */
void rk65c02_mmu_mark_changed_vrange(rk65c02emu_t *e, uint16_t start,
    uint16_t end);

/**
 * @brief End host MMU mapping update batch and apply invalidation actions.
 * @param e Emulator instance.
 */
void rk65c02_mmu_end_update(rk65c02emu_t *e);

/**
 * @brief Enable or disable internal MMU software TLB.
 * @param e Emulator instance.
 * @param enabled Desired TLB state.
 */
void rk65c02_mmu_tlb_set(rk65c02emu_t *e, bool enabled);

/**
 * @brief Flush all entries from internal MMU software TLB.
 * @param e Emulator instance.
 */
void rk65c02_mmu_tlb_flush(rk65c02emu_t *e);

/**
 * @brief Flush one virtual page from internal MMU software TLB.
 * @param e Emulator instance.
 * @param vpage Virtual page index.
 */
void rk65c02_mmu_tlb_flush_vpage(rk65c02emu_t *e, uint8_t vpage);

/**
 * @brief Request stop at the next safe execution boundary.
 * @param e Emulator instance.
 */
void rk65c02_request_stop(rk65c02emu_t *e);

char *rk65c02_regs_string_get(reg_state_t);
void rk65c02_dump_regs(reg_state_t);
void rk65c02_dump_stack(rk65c02emu_t *, uint8_t);

/**
 * @brief Assert the IRQ line.
 * @param e Emulator instance.
 *
 * If the CPU is currently STOPPED because of WAI, this also transitions it
 * back to RUNNING so execution can continue and service interrupts.
 */
void rk65c02_assert_irq(rk65c02emu_t *e);

/**
 * @brief Respond to interrupt and start the interrupt service routine.
 * @param e Emulator instance.
 */
void rk65c02_irq(rk65c02emu_t *e);

/**
 * @brief Handle critical error - send message to log and stop emulation.
 * @param e Emulator instance.
 * @param fmt Message in printf-like format.
 */
void rk65c02_panic(rk65c02emu_t *e, const char *fmt, ...);

uint8_t rk65c02_mem_fetch_1(rk65c02emu_t *e, uint16_t addr);
uint8_t rk65c02_mem_read_1(rk65c02emu_t *e, uint16_t addr);
void rk65c02_mem_write_1(rk65c02emu_t *e, uint16_t addr, uint8_t val);

/**
 * @brief Prep the emulator, load code from file, pass bus config optionally.
 * @param path Path to ROM file to be loaded.
 * @param load_addr Address on the bus where ROM should be loaded.
 * @param b Pre-existing bus configuration, pass NULL if default requested.
 * @return New instance of the emulator prepared to run the ROM.
 */
rk65c02emu_t rk65c02_load_rom(const char *path, uint16_t load_addr,
    bus_t *b);

/**
 * @brief Enable or disable GNU lightning based JIT backend.
 * @param e Emulator instance.
 * @param enable true to enable, false to disable.
 */
void rk65c02_jit_enable(rk65c02emu_t *e, bool enable);

/**
 * @brief Flush JIT-compiled code for given emulator instance.
 * @param e Emulator instance.
 */
void rk65c02_jit_flush(rk65c02emu_t *e);

#endif

