#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"
#include "instruction.h"
#include "utils.h"

/*
 * Test case for software generated interrupt (by BRK instruction).
 */
ATF_TC_WITHOUT_HEAD(intr_brk);
ATF_TC_BODY(intr_brk, tc)
{
	const uint16_t isr_addr = 0xC100;

	rk65c02emu_t e;	
	bus_t b;


	b = bus_init();
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;
	e.regs.SP = 0xFF;

	bus_write_1(&b, 0x10, 0x40);

	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_interrupt_brk.rom", tc)));

	ATF_REQUIRE(bus_load_file(&b, isr_addr,
	    rom_path("test_interrupt_brk_isr.rom", tc)));

        bus_write_1(&b, VECTOR_IRQ, isr_addr & 0xFF);
        bus_write_1(&b, VECTOR_IRQ+1, isr_addr >> 8);

	/* Execute first instruction, of the main program, which is a nop... */
	rk65c02_step(&e, 1);
	rk65c02_dump_regs(&e);
	/* BRK is next, save its address... */
	//brkaddr = e.regs.PC + 1;
	/* Execute BRK instruction, which should start ISR (regardless of IRQ disable flag). */
	rk65c02_step(&e, 1);
	rk65c02_dump_regs(&e);
	rk65c02_dump_stack(&e, 0x4);
	/* Are we in ISR really? */
	ATF_CHECK(e.regs.PC == isr_addr);
	ATF_CHECK(e.regs.P & P_IRQ_DISABLE);

	/* XXX: separate test case is needed to check return to main program. */

	/*
	rk65c02_step(&e, 1);
	rk65c02_dump_regs(&e);
	rk65c02_start(&e);
	*/
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, intr_brk);

	return (atf_no_error());
}

