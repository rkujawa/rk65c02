#include <atf-c.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"

#define ROM_LOAD_ADDR 0xC000

bool rom_start(rk65c02emu_t *, const char *);

bool
rom_start(rk65c02emu_t *e, const char *name)
{
	e->regs.PC = ROM_LOAD_ADDR;
	if(!bus_load_file(e->bus, ROM_LOAD_ADDR, name))
		return false;
	rk65c02_start(e);

	return true;
}

ATF_TC_WITHOUT_HEAD(emul_dex_dey);
ATF_TC_BODY(emul_dex_dey, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* DEX  */
	e.regs.X = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_dex.rom"));
	ATF_CHECK(e.regs.X == 0x0);
	/* DEX underflow */
	ATF_REQUIRE(rom_start(&e, "test_emulation_dex.rom"));
	ATF_CHECK(e.regs.X == 0xFF);

	/* DEY */
	e.regs.Y = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_dey.rom"));
	ATF_CHECK(e.regs.Y == 0x0);
	/* DEY underflow */
	ATF_REQUIRE(rom_start(&e, "test_emulation_dey.rom"));
	ATF_CHECK(e.regs.Y == 0xFF);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_inx_iny);
ATF_TC_BODY(emul_inx_iny, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* INX */
	e.regs.X = 0;
	ATF_REQUIRE(rom_start(&e, "test_emulation_inx.rom"));
	ATF_CHECK(e.regs.X == 0x1);
	/* INX overflow */
	e.regs.X = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_inx.rom"));
	ATF_CHECK(e.regs.X == 0x0);

	/* INY */
	e.regs.Y = 0;
	ATF_REQUIRE(rom_start(&e, "test_emulation_iny.rom"));
	ATF_CHECK(e.regs.Y == 0x1);
	/* INY overflow */
	e.regs.Y = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_iny.rom"));
	ATF_CHECK(e.regs.Y == 0x0);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_lda);
ATF_TC_BODY(emul_lda, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* LDA immediate */
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_imm.rom"));
/*	ATF_CHECK(e.state == STOPPED);  // separate test case for states? */
	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+3); // separate test case for PC? */
	ATF_CHECK(e.regs.A == 0xAF);

	/* LDA zero page */
	bus_write_1(&b, 0x10, 0xAE);
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_zp.rom"));
	ATF_CHECK(e.regs.A == 0xAE);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_stz);
ATF_TC_BODY(emul_stz, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* STZ zp */
	bus_write_1(&b, 0x10, 0xAA);
	ATF_REQUIRE(rom_start(&e, "test_emulation_stz_zp.rom"));
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x00);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_clc_sec);
ATF_TC_BODY(emul_clc_sec, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* SEC */
	e.regs.P &= ~P_CARRY;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sec.rom"));
	ATF_CHECK(e.regs.P & P_CARRY);
	/* CLC */
	ATF_REQUIRE(rom_start(&e, "test_emulation_clc.rom"));
	ATF_CHECK(e.regs.P ^ P_CARRY);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_and);
ATF_TC_BODY(emul_and, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* AND immediate */
	e.regs.A = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_and_imm.rom"));
	ATF_CHECK(e.regs.A == 0xAA);

	/* AND zero page */
/*	bus_write_1(&b, 0x10, 0xAE);
	ATF_REQUIRE(rom_start(&e, "test_emulation_and_zp.rom"));
	ATF_CHECK(e.regs.A == 0xAE);*/

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_nop);
ATF_TC_BODY(emul_nop, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;

	ATF_REQUIRE(rom_start(&e, "test_emulation_nop.rom"));

	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+2);

	bus_finish(&b);
}

/* test stack operation and stack related opcodes - PLA, PHA... */
ATF_TC_WITHOUT_HEAD(emul_stack);
ATF_TC_BODY(emul_stack, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* place 0xAA on stack */
	e.regs.SP = 0xFF;
	e.regs.A = 0xAA;

	ATF_REQUIRE(rom_start(&e, "test_emulation_pha.rom"));

	ATF_CHECK(e.regs.SP == 0xFE);
	ATF_CHECK(bus_read_1(e.bus, STACK_END) == 0xAA);

	/* 
	 * Run again to see if stack pointer further decrements and we'll
	 * end up with one more value on stack.
	 */
	e.regs.PC = ROM_LOAD_ADDR;
	e.regs.A = 0xAB;

	rk65c02_start(&e);

	ATF_CHECK(e.regs.SP == 0xFD);
	ATF_CHECK(bus_read_1(e.bus, STACK_END) == 0xAA);
	ATF_CHECK(bus_read_1(e.bus, STACK_END-1) == 0xAB);

	/*
	 * Now let's see if loading back into accumulator works.
	 */
	e.regs.A = 0x0;
	ATF_REQUIRE(rom_start(&e, "test_emulation_pla.rom"));

	ATF_CHECK(e.regs.SP == 0xFE);
	ATF_CHECK(e.regs.A == 0xAB);

	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, emul_and);
	ATF_TP_ADD_TC(tp, emul_dex_dey);
	ATF_TP_ADD_TC(tp, emul_clc_sec);
	ATF_TP_ADD_TC(tp, emul_inx_iny);
	ATF_TP_ADD_TC(tp, emul_lda);
	ATF_TP_ADD_TC(tp, emul_nop);
	ATF_TP_ADD_TC(tp, emul_stz);

	ATF_TP_ADD_TC(tp, emul_stack);

	return (atf_no_error());
}

