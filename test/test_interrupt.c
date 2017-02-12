#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"
#include "instruction.h"
#include "utils.h"

#define ISR_ADDR 0xC100

/*
 * Test case for software generated interrupt (by BRK instruction).
 */
ATF_TC_WITHOUT_HEAD(intr_brk);
ATF_TC_BODY(intr_brk, tc)
{
	const uint16_t isr_addr = ISR_ADDR;

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

/*
 * Test case for return from interrupt by RTI instruction.
 */
ATF_TC_WITHOUT_HEAD(intr_rti);
ATF_TC_BODY(intr_rti, tc)
{
	bus_t b;
	rk65c02emu_t e;
	uint8_t *asmbuf;
	uint16_t israsmpc;
	uint8_t bsize;

	b = bus_init();
	e = rk65c02_init(&b);

	israsmpc = ISR_ADDR;

	ATF_REQUIRE(assemble_single_buf_implied(&asmbuf, &bsize, "nop"));
	ATF_REQUIRE(bus_load_buf(&b, israsmpc, asmbuf, bsize));
	free(asmbuf);
	israsmpc += bsize;

	ATF_REQUIRE(assemble_single_buf_implied(&asmbuf, &bsize, "rti"));
	ATF_REQUIRE(bus_load_buf(&b, israsmpc, asmbuf, bsize));
	free(asmbuf);
	israsmpc += bsize;

	ATF_REQUIRE(assemble_single_buf_implied(&asmbuf, &bsize, "nop"));
	ATF_REQUIRE(bus_load_buf(&b, ROM_LOAD_ADDR, asmbuf, bsize));
	free(asmbuf);

	/* There's a return address and saved processor flags on stack. */
	e.regs.SP = 0xFF;
	stack_push(&e, ROM_LOAD_ADDR >> 8);
	stack_push(&e, ROM_LOAD_ADDR & 0xFF);
	stack_push(&e, e.regs.P);

	/* We're in the middle of interrupt service routine, just before RTI. */
	e.regs.PC = ISR_ADDR;
	rk65c02_step(&e, 1);
	ATF_CHECK(e.regs.PC == ISR_ADDR + 1);
	rk65c02_dump_regs(&e);
	rk65c02_dump_stack(&e, 0x4);

	/* Step onto RTI. */
	rk65c02_step(&e, 1);
	rk65c02_dump_regs(&e);
	/* Check if we're back in the main program. */
	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR);

}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, intr_brk);
	ATF_TP_ADD_TC(tp, intr_rti);

	return (atf_no_error());
}

