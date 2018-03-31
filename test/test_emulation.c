#include <atf-c.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"
#include "instruction.h"
#include "debug.h"
#include "log.h"
#include "utils.h"

ATF_TC_WITHOUT_HEAD(emul_bit);
ATF_TC_BODY(emul_bit, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* BIT immediate  */
	e.regs.A = 0x40;
	ATF_REQUIRE(rom_start(&e, "test_emulation_bit_imm.rom", tc));
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* BIT zero page */
	e.regs.A = 0x40;
	bus_write_1(&b, 0x10, 0x80);
	ATF_REQUIRE(rom_start(&e, "test_emulation_bit_zp.rom", tc));
	ATF_CHECK(e.regs.P & P_ZERO);
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	ATF_CHECK(e.regs.P & P_NEGATIVE);
	/* BIT zero page X */
	e.regs.A = 0x40;
	e.regs.X = 0x1;
	bus_write_1(&b, 0x10, 0x40);
	ATF_REQUIRE(rom_start(&e, "test_emulation_bit_zpx.rom", tc));
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(e.regs.P & P_SIGN_OVERFLOW);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* BIT absolute */
	e.regs.A = 0x80;
	bus_write_1(&b, 0x2010, 0x80);
	ATF_REQUIRE(rom_start(&e, "test_emulation_bit_abs.rom", tc));
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	ATF_CHECK(e.regs.P & P_NEGATIVE);
	/* BIT absolute X */
	e.regs.A = 0x40;
	e.regs.X = 0x2;
	bus_write_1(&b, 0x2010, 0x80);
	ATF_REQUIRE(rom_start(&e, "test_emulation_bit_absx.rom", tc));
	ATF_CHECK(e.regs.P & P_ZERO);
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	ATF_CHECK(e.regs.P & P_NEGATIVE);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_cmp);
ATF_TC_BODY(emul_cmp, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* CMP immediate */
	e.regs.A = 0xAA;
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_imm.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(e.regs.P & P_ZERO);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* CMP zero page */
	e.regs.A = 0xAA;
	bus_write_1(&b, 0x10, 0xAB);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_zp.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(e.regs.P & P_NEGATIVE);
	/* CMP zero page X */
	e.regs.A = 0xAA;
	e.regs.X = 0x1;
	bus_write_1(&b, 0x11, 0xA0);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_zpx.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* CMP indirect zero page */
	e.regs.A = 0x01;
	bus_write_1(&b, 0x20, 0x0);
	bus_write_1(&b, 0x21, 0x20);
	bus_write_1(&b, 0x2000, 0xFF);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_izp.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* CMP indirect zero page X */
	e.regs.A = 0x02;
	e.regs.X = 0x02;
	bus_write_1(&b, 0x22, 0x0);
	bus_write_1(&b, 0x23, 0x20);
	bus_write_1(&b, 0x2000, 0x3);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_izpx.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(e.regs.P & P_NEGATIVE);
	/* CMP indirect zero page Y */
	e.regs.A = 0x10;
	e.regs.Y = 0x01;
	bus_write_1(&b, 0x22, 0x0);
	bus_write_1(&b, 0x23, 0x20);
	bus_write_1(&b, 0x2001, 0x10);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_izpy.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(e.regs.P & P_ZERO);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* CMP absolute */
	e.regs.A = 0xFF;
	bus_write_1(&b, 0x2010, 0xFE);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_abs.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* CMP absolute X */
	e.regs.A = 0x55;
	e.regs.X = 0x2;
	bus_write_1(&b, 0x2012, 0x55);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_absx.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(e.regs.P & P_ZERO);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* CMP absolute Y */
	e.regs.A = 0xAA;
	e.regs.Y = 0x50;
	bus_write_1(&b, 0x2065, 0x55);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cmp_absy.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(e.regs.P & P_NEGATIVE);
}

ATF_TC_WITHOUT_HEAD(emul_cpx);
ATF_TC_BODY(emul_cpx, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* CPX immediate */
	e.regs.X = 0xAA;
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cpx_imm.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(e.regs.P & P_ZERO);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* CPX zero page */
	e.regs.X = 0xAA;
	bus_write_1(&b, 0x10, 0xAB);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cpx_zp.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(e.regs.P & P_NEGATIVE);
	/* CPX absolute */
	e.regs.X = 0xFF;
	bus_write_1(&b, 0x2010, 0xFE);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cpx_abs.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
}

ATF_TC_WITHOUT_HEAD(emul_cpy);
ATF_TC_BODY(emul_cpy, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* CPY immediate */
	e.regs.Y = 0xAA;
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cpy_imm.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(e.regs.P & P_ZERO);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
	/* CPY zero page */
	e.regs.Y = 0xAA;
	bus_write_1(&b, 0x10, 0xAB);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cpy_zp.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(e.regs.P & P_NEGATIVE);
	/* CPY absolute */
	e.regs.Y = 0xFF;
	bus_write_1(&b, 0x2010, 0xFE);
	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_cpy_abs.rom", tc));
	rk65c02_dump_regs(e.regs);
	ATF_CHECK(!(e.regs.P & P_ZERO));
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_NEGATIVE));
}

ATF_TC_WITHOUT_HEAD(emul_dex_dey);
ATF_TC_BODY(emul_dex_dey, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* DEX  */
	e.regs.X = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_dex.rom", tc));
	ATF_CHECK(e.regs.X == 0x0);
	/* DEX underflow */
	ATF_REQUIRE(rom_start(&e, "test_emulation_dex.rom", tc));
	ATF_CHECK(e.regs.X == 0xFF);

	/* DEY */
	e.regs.Y = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_dey.rom", tc));
	ATF_CHECK(e.regs.Y == 0x0);
	/* DEY underflow */
	ATF_REQUIRE(rom_start(&e, "test_emulation_dey.rom", tc));
	ATF_CHECK(e.regs.Y == 0xFF);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_dec);
ATF_TC_BODY(emul_dec, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* DEC A */
	e.regs.A = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_a.rom", tc));
	ATF_CHECK(e.regs.A == 0x0);
	/* DEC A underflow */
	e.regs.A = 0x0;
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_a.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);

	/* DEC zero page */
	bus_write_1(&b, 0x10, 0x01);
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_zp.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x0);
	/* DEC zero page underflow */
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_zp.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x10) == 0xFF);

	/* DEC zero page X */
	e.regs.X = 1;
	bus_write_1(&b, 0x11, 0x01);
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_zpx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x11) == 0x0);
	/* DEC  zero page X overflow */
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_zpx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x11) == 0xFF);

	/* DEC absolute */
	bus_write_1(&b, 0x2010, 0xA1);
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_abs.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0xA0);
	/* DEC absolute overflow */
	bus_write_1(&b, 0x2010, 0x0);
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_abs.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0xFF);

	/* DEC absolute X */
	e.regs.X = 0x10;
	bus_write_1(&b, 0x2020, 0x1);
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_absx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2020) == 0x0);
	/* DEC absolute X underflow */
	ATF_REQUIRE(rom_start(&e, "test_emulation_dec_absx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2020) == 0xFF);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_inc);
ATF_TC_BODY(emul_inc, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* INC A */
	e.regs.A = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_a.rom", tc));
	ATF_CHECK(e.regs.A == 0x2);
	/* rk65c02_dump_regs(e.regs);*/
	/* INC A overflow */
	e.regs.A = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_a.rom", tc));
	ATF_CHECK(e.regs.A == 0x0);

	/* INC zero page */
	bus_write_1(&b, 0x10, 0x00);
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_zp.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x1);
	/* INC zero page overflow */
	bus_write_1(&b, 0x10, 0xFF);
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_zp.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x00);

	/* INC zero page X */
	e.regs.X = 1;
	bus_write_1(&b, 0x11, 0x00);
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_zpx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x11) == 0x1);
	/* INC zero page X overflow */
	bus_write_1(&b, 0x11, 0xFF);
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_zpx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x11) == 0x00);

	/* INC absolute */
	bus_write_1(&b, 0x2010, 0xA0);
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_abs.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0xA1);
	/* INC absolute overflow */
	bus_write_1(&b, 0x2010, 0xFF);
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_abs.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0x00);

	/* INC absolute X */
	e.regs.X = 0x10;
	bus_write_1(&b, 0x2020, 0xFE);
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_absx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2020) == 0xFF);
	/* INC absolute X overflow */
	ATF_REQUIRE(rom_start(&e, "test_emulation_inc_absx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2020) == 0x00);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_inx_iny);
ATF_TC_BODY(emul_inx_iny, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* INX */
	e.regs.X = 0;
	ATF_REQUIRE(rom_start(&e, "test_emulation_inx.rom", tc));
	ATF_CHECK(e.regs.X == 0x1);
	/* INX overflow */
	e.regs.X = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_inx.rom", tc));
	ATF_CHECK(e.regs.X == 0x0);

	/* INY */
	e.regs.Y = 0;
	ATF_REQUIRE(rom_start(&e, "test_emulation_iny.rom", tc));
	ATF_CHECK(e.regs.Y == 0x1);
	/* INY overflow */
	e.regs.Y = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_iny.rom", tc));
	ATF_CHECK(e.regs.Y == 0x0);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_lda);
ATF_TC_BODY(emul_lda, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* LDA immediate */
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_imm.rom", tc));
/*	ATF_CHECK(e.state == STOPPED);  // separate test case for states? */
	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+3); // separate test case for PC? */
	ATF_CHECK(e.regs.A == 0xAF);

	/* LDA zero page */
	bus_write_1(&b, 0x10, 0xAE);
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_zp.rom", tc));
	ATF_CHECK(e.regs.A == 0xAE);

	/* LDA absolute */
	bus_write_1(&b, 0x2F5A, 0xEA);
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_abs.rom", tc));
	ATF_CHECK(e.regs.A == 0xEA);

	/* LDA absolute X */
	bus_write_1(&b, 0x2F5A, 0xEB);
	e.regs.X = 0x5A;
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_absx.rom", tc));
	ATF_CHECK(e.regs.A == 0xEB);

	/* LDA absolute X */
	bus_write_1(&b, 0x2F5E, 0xEC);
	e.regs.Y = 0x5E;
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_absy.rom", tc));
	ATF_CHECK(e.regs.A == 0xEC);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_stz);
ATF_TC_BODY(emul_stz, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* STZ zp */
	bus_write_1(&b, 0x10, 0xAA);
	ATF_REQUIRE(rom_start(&e, "test_emulation_stz_zp.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x00);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_clv);
ATF_TC_BODY(emul_clv, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.P |= P_SIGN_OVERFLOW;
	/* CLV */
	ATF_REQUIRE(rom_start(&e, "test_emulation_clv.rom", tc));
	ATF_CHECK(e.regs.P ^ P_SIGN_OVERFLOW);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_clc_sec);
ATF_TC_BODY(emul_clc_sec, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* SEC */
	e.regs.P &= ~P_CARRY;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sec.rom", tc));
	ATF_CHECK(e.regs.P & P_CARRY);
	/* CLC */
	ATF_REQUIRE(rom_start(&e, "test_emulation_clc.rom", tc));
	ATF_CHECK(e.regs.P ^ P_CARRY);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_cli_sei);
ATF_TC_BODY(emul_cli_sei, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* CLI */
	ATF_REQUIRE(rom_start(&e, "test_emulation_cli.rom", tc));
	ATF_CHECK(!(e.regs.P & P_IRQ_DISABLE));
	/* SEI */
	ATF_REQUIRE(rom_start(&e, "test_emulation_sei.rom", tc));
	ATF_CHECK(e.regs.P & P_IRQ_DISABLE);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_and);
ATF_TC_BODY(emul_and, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* AND immediate */
	e.regs.A = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_and_imm.rom", tc));
	ATF_CHECK(e.regs.A == 0xAA);

	/* AND zero page */
/*	bus_write_1(&b, 0x10, 0xAE);
	ATF_REQUIRE(rom_start(&e, "test_emulation_and_zp.rom", tc));
	ATF_CHECK(e.regs.A == 0xAE);*/

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_asl);
ATF_TC_BODY(emul_asl, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.A = 0xAA;
	e.regs.X = 0x1;
	bus_write_1(&b, 0x10, 0xAA);
	bus_write_1(&b, 0x11, 0xAA);
	bus_write_1(&b, 0x300, 0xFF);
	bus_write_1(&b, 0x301, 0xFF);

	ATF_REQUIRE(rom_start(&e, "test_emulation_asl.rom", tc));

	ATF_CHECK(e.regs.A == 0x54);
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x54);
	ATF_CHECK(bus_read_1(&b, 0x11) == 0x54);
	ATF_CHECK(bus_read_1(&b, 0x300) == 0xFE);
	ATF_CHECK(bus_read_1(&b, 0x301) == 0xFE);
	ATF_CHECK(e.regs.P & P_CARRY);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_lsr);
ATF_TC_BODY(emul_lsr, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.A = 0x55;
	e.regs.X = 0x1;
	bus_write_1(&b, 0x10, 0x55);
	bus_write_1(&b, 0x11, 0x55);
	bus_write_1(&b, 0x300, 0xFF);
	bus_write_1(&b, 0x301, 0xFF);

	ATF_REQUIRE(rom_start(&e, "test_emulation_lsr.rom", tc));

	ATF_CHECK(e.regs.A == 0x2A);
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x2A);
	ATF_CHECK(bus_read_1(&b, 0x11) == 0x2A);
	ATF_CHECK(bus_read_1(&b, 0x300) == 0x7F);
	ATF_CHECK(bus_read_1(&b, 0x301) == 0x7F);
	ATF_CHECK(e.regs.P & P_CARRY);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_nop);
ATF_TC_BODY(emul_nop, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;

	ATF_REQUIRE(rom_start(&e, "test_emulation_nop.rom", tc));

	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+2);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_sta);
ATF_TC_BODY(emul_sta, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* STA zero page */
	e.regs.A = 0xAA;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sta_zp.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x20) == 0xAA);
	/* STA zero page X */
	e.regs.A = 0x55;
	e.regs.X = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sta_zpx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x20) == 0x55);
	/* STA absolute */
	e.regs.A = 0xAA;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sta_abs.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0xAA);
	/* STA absolute X */
	e.regs.A = 0x55;
	e.regs.X = 0x10;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sta_absx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0x55);
	/* STA absolute Y */
	e.regs.A = 0xAA;
	e.regs.X = 0;
	e.regs.Y = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sta_absy.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0xAA);
	/* STA indirect zero */
	e.regs.A = 0x55;
	bus_write_1(&b, 0x25, 0x10);
	bus_write_1(&b, 0x26, 0x20);
	ATF_REQUIRE(rom_start(&e, "test_emulation_sta_izp.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0x55);
	/* STA indirect zero page X */
	e.regs.A = 0xAA;
	e.regs.X = 0x4;
	e.regs.Y = 0;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sta_izpx.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2010) == 0xAA);
	/* STA indirect zero page Y */
	e.regs.A = 0x55;
	e.regs.X = 0;
	e.regs.Y = 0x1;
	ATF_REQUIRE(rom_start(&e, "test_emulation_sta_izpy.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x2011) == 0x55);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_ora);
ATF_TC_BODY(emul_ora, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	/* ORA immediate */
	e.regs.A = 0x55;
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_imm.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);
	/* ORA zero page */
	e.regs.A = 0xAA;
	bus_write_1(&b, 0x10, 0x55);
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_zp.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);
	/* ORA zero page X */
	e.regs.A = 0xAA;
	e.regs.X = 0x11;
	bus_write_1(&b, 0x21, 0x55);
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_zpx.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);
	/* ORA absolute */
	e.regs.A = 0x55;
	bus_write_1(&b, 0x2A01, 0xAA);
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_abs.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);
	/* ORA absolute X */
	e.regs.A = 0xAA;
	e.regs.X = 0x1;
	bus_write_1(&b, 0x2A01, 0x55);
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_absx.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);
	/* ORA absolute Y */
	e.regs.A = 0x55;
	e.regs.X = 0;
	e.regs.Y = 0x2;
	bus_write_1(&b, 0x2A02, 0xAA);
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_absy.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);
	/* ORA indirect zero */
	e.regs.A = 0xAA;
	bus_write_1(&b, 0x2A04, 0x55);
	bus_write_1(&b, 0x12, 0x04);
	bus_write_1(&b, 0x13, 0x2A);
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_izp.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);
	/* ORA indirect zero page X */
	e.regs.A = 0xAA;
	e.regs.X = 0x2;
	e.regs.Y = 0;
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_izpx.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);
	/* ORA indirect zero page Y */
	e.regs.A = 0xAA;
	e.regs.X = 0;
	e.regs.Y = 0x1;
	bus_write_1(&b, 0x2A05, 0x55);
	bus_write_1(&b, 0x14, 0x04);
	bus_write_1(&b, 0x15, 0x2A);
	ATF_REQUIRE(rom_start(&e, "test_emulation_ora_izpy.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_txa_tya_tax_tay);
ATF_TC_BODY(emul_txa_tya_tax_tay, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.A = 0x0;
	e.regs.X = 0xAA;
	e.regs.Y = 0x55;

	ATF_REQUIRE(rom_start(&e, "test_emulation_txa.rom", tc));
	ATF_CHECK(e.regs.A == 0xAA);

	ATF_REQUIRE(rom_start(&e, "test_emulation_tya.rom", tc));
	ATF_CHECK(e.regs.A == 0x55);

	ATF_REQUIRE(rom_start(&e, "test_emulation_tax.rom", tc));
	ATF_CHECK(e.regs.X == 0x55);

	e.regs.A = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_tay.rom", tc));
	ATF_CHECK(e.regs.A == 0xFF);

	bus_finish(&b);
}

/* test stack operation and stack related opcodes - PLA, PHA... */
ATF_TC_WITHOUT_HEAD(emul_stack);
ATF_TC_BODY(emul_stack, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* place 0xAA on stack */
	e.regs.SP = 0xFF;
	e.regs.A = 0xAA;

	ATF_REQUIRE(rom_start(&e, "test_emulation_pha.rom", tc));

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
	ATF_REQUIRE(rom_start(&e, "test_emulation_pla.rom", tc));

	ATF_CHECK(e.regs.SP == 0xFE);
	ATF_CHECK(e.regs.A == 0xAB);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_php_plp);
ATF_TC_BODY(emul_php_plp, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.SP = 0xFF;
	e.regs.P |= P_CARRY|P_ZERO|P_UNDEFINED;

	ATF_REQUIRE(rom_start(&e, "test_emulation_php.rom", tc));

	ATF_CHECK(e.regs.SP == 0xFE);
	ATF_CHECK(bus_read_1(e.bus, STACK_END) == (P_IRQ_DISABLE|P_CARRY|P_ZERO|P_UNDEFINED));

	/*
	 * Now let's see if loading back into accumulator works.
	 */
	bus_write_1(e.bus, STACK_END, P_CARRY|P_DECIMAL);
	ATF_REQUIRE(rom_start(&e, "test_emulation_plp.rom", tc));

	ATF_CHECK(e.regs.SP == 0xFF);
	ATF_CHECK(e.regs.P == (P_CARRY|P_DECIMAL|P_UNDEFINED));

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_phx_phy_plx_ply);
ATF_TC_BODY(emul_phx_phy_plx_ply, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* check push X to stack */
	e.regs.X = 0xAA;
	e.regs.SP = 0xFF;

	ATF_REQUIRE(rom_start(&e, "test_emulation_phx.rom", tc));

	ATF_CHECK(e.regs.SP == 0xFE);
	ATF_CHECK(bus_read_1(e.bus, STACK_END) == 0xAA);

	/* check pull X from stack */
	e.regs.X = 0;

	ATF_REQUIRE(rom_start(&e, "test_emulation_plx.rom", tc));

	ATF_CHECK(e.regs.SP == 0xFF);
	ATF_CHECK(e.regs.X == 0xAA);

	/* check push Y to stack */
	e.regs.Y = 0x55;
	e.regs.SP = 0xFF;

	ATF_REQUIRE(rom_start(&e, "test_emulation_phy.rom", tc));

	ATF_CHECK(e.regs.SP == 0xFE);
	ATF_CHECK(bus_read_1(e.bus, STACK_END) == 0x55);

	/* check pull X from stack */
	e.regs.Y = 0xFF;

	ATF_REQUIRE(rom_start(&e, "test_emulation_ply.rom", tc));

	ATF_CHECK(e.regs.SP == 0xFF);
	ATF_CHECK(e.regs.Y == 0x55);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_jmp);
ATF_TC_BODY(emul_jmp, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* JMP absolute */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR, 
	    rom_path("test_emulation_jmp_abs.rom", tc)));

	rk65c02_step(&e, 3);
	ATF_CHECK(e.regs.PC == 0xC000);

	/* JMP indirect absolute */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR, 
	    rom_path("test_emulation_jmp_iabs.rom", tc)));

	bus_write_1(&b, 0x20, 0x0);
	bus_write_1(&b, 0x21, 0xC0);

	rk65c02_step(&e, 3);
	ATF_CHECK(e.regs.PC == 0xC000);

	/* JMP indirect absolute X */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR, 
	    rom_path("test_emulation_jmp_iabsx.rom", tc)));

	e.regs.X = 0x10;
	bus_write_1(&b, 0x40, 0x0);
	bus_write_1(&b, 0x41, 0xC0);

	rk65c02_step(&e, 3);
	ATF_CHECK(e.regs.PC == 0xC000);
}

ATF_TC_WITHOUT_HEAD(emul_jsr_rts);
ATF_TC_BODY(emul_jsr_rts, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* JSR and RTS */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR, 
	    rom_path("test_emulation_jsr_rts.rom", tc)));

	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC006);
	rk65c02_start(&e); 
	ATF_CHECK(e.regs.PC == 0xC006);

}

ATF_TC_WITHOUT_HEAD(emul_branch);
ATF_TC_BODY(emul_branch, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	/* BCC */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_emulation_bcc.rom", tc)));

	e.regs.P &= ~P_CARRY;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC005);
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC003);
	rk65c02_start(&e); 

	/* BCS */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_emulation_bcs.rom", tc)));

	e.regs.P |= P_CARRY;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC005);
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC003);
	rk65c02_start(&e); 

	/* BRA */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR, 
	    rom_path("test_emulation_bra.rom", tc)));

	rk65c02_step(&e, 1);
	ATF_CHECK(e.regs.PC == 0xC004);
	rk65c02_start(&e); 

	/* BEQ */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR, 
	    rom_path("test_emulation_beq.rom", tc)));

	e.regs.P |= P_ZERO;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC005);
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC003);
	rk65c02_start(&e); 

	/* BMI */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_emulation_bmi.rom", tc)));

	e.regs.P |= P_NEGATIVE;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC005);
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC003);
	rk65c02_start(&e); 

	/* BNE */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_emulation_bne.rom", tc)));

	e.regs.P &= ~P_ZERO;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC005);
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC003);
	rk65c02_start(&e); 

	/* BPL */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_emulation_bpl.rom", tc)));

	e.regs.P &= ~P_NEGATIVE;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC005);
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC003);
	rk65c02_start(&e); 

	/* BVC */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_emulation_bvc.rom", tc)));

	e.regs.P &= ~P_SIGN_OVERFLOW;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC005);
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC003);
	rk65c02_start(&e); 

	/* BVS */
	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_emulation_bvs.rom", tc)));

	e.regs.P |= P_SIGN_OVERFLOW;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC005);
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.PC == 0xC003);
	rk65c02_start(&e); 
}

ATF_TC_WITHOUT_HEAD(emul_sign_overflow_basic);
ATF_TC_BODY(emul_sign_overflow_basic, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;
	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR, 
	    rom_path("test_emulation_sign_overflow_basic.rom", tc)));

	/* 0x50 + 0x10 */
	e.regs.A = 0x50;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0x60);
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0x50 + 0x50 */
	e.regs.A = 0x50;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0xA0);
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(e.regs.P & P_SIGN_OVERFLOW);
	rk65c02_dump_regs(e.regs);

	/* 0x50 + 0x90 */
	e.regs.A = 0x50;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0xE0);
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0x50 + 0xD0 */
	e.regs.A = 0x50;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0x20);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0xD0 + 0x10 */
	e.regs.A = 0xD0;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0xE0);
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0xD0 + 0x50 */
	e.regs.A = 0xD0;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0x20);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0xD0 + 0x90 */
	e.regs.A = 0xD0;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0x60);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(e.regs.P & P_SIGN_OVERFLOW);
	rk65c02_dump_regs(e.regs);

	/* 0xD0 + 0xD0 */
	e.regs.A = 0xD0;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0xA0);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);


	/* 0x50 - 0xF0 */
	e.regs.A = 0x50;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0x60);
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0x50 - 0xB0 */
	e.regs.A = 0x50;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0xA0);
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(e.regs.P & P_SIGN_OVERFLOW);
	rk65c02_dump_regs(e.regs);

	/* 0x50 - 0x70 */
	e.regs.A = 0x50;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0xE0);
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0x50 - 0x30 */
	e.regs.A = 0x50;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0x20);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0xD0 - 0xF0 */
	e.regs.A = 0xD0;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0xE0);
	ATF_CHECK(!(e.regs.P & P_CARRY));
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0xD0 - 0xB0 */
	e.regs.A = 0xD0;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0x20);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

	/* 0xD0 - 0x70 */
	e.regs.A = 0xD0;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0x60);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(e.regs.P & P_SIGN_OVERFLOW);
	rk65c02_dump_regs(e.regs);

	/* 0xD0 - 0x30 */
	e.regs.A = 0xD0;
	rk65c02_step(&e, 2);
	ATF_CHECK(e.regs.A == 0xA0);
	ATF_CHECK(e.regs.P & P_CARRY);
	ATF_CHECK(!(e.regs.P & P_SIGN_OVERFLOW));
	rk65c02_dump_regs(e.regs);

}

ATF_TC_WITHOUT_HEAD(emul_sign_overflow_thorough);
ATF_TC_BODY(emul_sign_overflow_thorough, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	ATF_REQUIRE(rom_start(&e, "test_emulation_sign_overflow_thorough.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x20) == 0x0);

}

ATF_TC_WITHOUT_HEAD(emul_sbc);
ATF_TC_BODY(emul_sbc, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	ATF_REQUIRE(rom_start(&e, "test_emulation_sbc_imm.rom", tc));
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x0);
	ATF_CHECK(bus_read_1(&b, 0x11) == 0xFF);
	rk65c02_dump_regs(e.regs);

}

ATF_TC_WITHOUT_HEAD(emul_adc_bcd);
ATF_TC_BODY(emul_adc_bcd, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	ATF_REQUIRE(rom_start(&e, "test_emulation_adc_bcd.rom", tc));

	ATF_CHECK(bus_read_1(&b, 0x10) == 0x05);
	ATF_CHECK(bus_read_1(&b, 0x11) & P_CARRY);
	ATF_CHECK(bus_read_1(&b, 0x20) == 0x46);
	ATF_CHECK(!(bus_read_1(&b, 0x21) & P_CARRY));
	ATF_CHECK(bus_read_1(&b, 0x30) == 0x41);
	ATF_CHECK(!(bus_read_1(&b, 0x31) & P_CARRY));
	ATF_CHECK(bus_read_1(&b, 0x40) == 0x73);
	ATF_CHECK(bus_read_1(&b, 0x41) & P_CARRY);

	rk65c02_dump_regs(e.regs);

}

ATF_TC_WITHOUT_HEAD(emul_sbc_bcd);
ATF_TC_BODY(emul_sbc_bcd, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	ATF_REQUIRE(rom_start(&e, "test_emulation_sbc_bcd.rom", tc));

	ATF_CHECK(bus_read_1(&b, 0x10) == 0x34);
	ATF_CHECK(bus_read_1(&b, 0x11) & P_CARRY);
	ATF_CHECK(bus_read_1(&b, 0x20) == 0x27);
	ATF_CHECK(bus_read_1(&b, 0x21) & P_CARRY);
	ATF_CHECK(bus_read_1(&b, 0x30) == 0x29);
	ATF_CHECK(bus_read_1(&b, 0x31) & P_CARRY);

	rk65c02_dump_regs(e.regs);

}

ATF_TC_WITHOUT_HEAD(emul_adc_16bit);
ATF_TC_BODY(emul_adc_16bit, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	bus_write_1(&b, 0x62, 0x55);
	bus_write_1(&b, 0x63, 0xAA);
	bus_write_1(&b, 0x64, 0xAA);
	bus_write_1(&b, 0x65, 0x55);

	ATF_REQUIRE(rom_start(&e, "test_emulation_adc_16bit.rom", tc));

	ATF_CHECK(bus_read_1(&b, 0x66) == 0xFF);
	ATF_CHECK(bus_read_1(&b, 0x67) == 0xFF);
	rk65c02_dump_regs(e.regs);

}

ATF_TC_WITHOUT_HEAD(emul_sbc_16bit);
ATF_TC_BODY(emul_sbc_16bit, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	bus_write_1(&b, 0x62, 0xFF);
	bus_write_1(&b, 0x63, 0xFF);
	bus_write_1(&b, 0x64, 0xAA);
	bus_write_1(&b, 0x65, 0x55);
	ATF_REQUIRE(rom_start(&e, "test_emulation_sbc_16bit.rom", tc));

	printf("%x %x\n", bus_read_1(&b, 0x66), bus_read_1(&b, 0x67)) ;
	ATF_CHECK(bus_read_1(&b, 0x66) == 0x55);
	ATF_CHECK(bus_read_1(&b, 0x67) == 0xAA);
	rk65c02_dump_regs(e.regs);
}

/*
 * This test tries to check every variant of RMBx instruction by resetting bits within 0x10-0x17 memory range.
 * This area is filled with 0xFF's before starting the emulator. It is expected that after running code within
 * the emulator, appropriate bits will be cleared (i.e. bit 0 in 0x10, bit 1 in 0x11, etc.).
 */
ATF_TC_WITHOUT_HEAD(emul_rmb);
ATF_TC_BODY(emul_rmb, tc)
{
	rk65c02emu_t e;
	bus_t b;
	assembler_t a;
	uint8_t i;

	char instr[] = "rmb ";

	b = bus_init_with_default_devs();
	a = assemble_init(&b, ROM_LOAD_ADDR);
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;

	for (i = 0; i < 8; i++) {
		instr[3] = '0'+i; 
		ATF_REQUIRE(assemble_single(&a, instr, ZP, 0x10+i, 0));
	}

	ATF_REQUIRE(assemble_single_implied(&a, "stp"));

	for (i = 0; i < 8; i++) {
		bus_write_1(&b, 0x10+i, 0xFF);
	}

	rk65c02_start(&e);

	for (i = 0; i < 8; i++) {
		ATF_CHECK(!(BIT(bus_read_1(&b, 0x10+i), i)));
	}
}

/*
 * This test tries to check every variant of SMBx instruction by setting bits within 0x10-0x17 memory range.
 * This area is filled with 0x00's before starting the emulator. It is expected that after running code within
 * the emulator, appropriate bits will be set (i.e. bit 0 in 0x10, bit 1 in 0x11, etc.).
 */
ATF_TC_WITHOUT_HEAD(emul_smb);
ATF_TC_BODY(emul_smb, tc)
{
	rk65c02emu_t e;
	bus_t b;
	assembler_t a;
	uint8_t i;

	char instr[] = "smb ";

	b = bus_init_with_default_devs();
	a = assemble_init(&b, ROM_LOAD_ADDR);
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;

	for (i = 0; i < 8; i++) {
		instr[3] = '0'+i; 
		ATF_REQUIRE(assemble_single(&a, instr, ZP, 0x10+i, 0));
	}

	ATF_REQUIRE(assemble_single_implied(&a, "stp"));

	for (i = 0; i < 8; i++) {
		bus_write_1(&b, 0x10+i, 0x00);
	}

	rk65c02_start(&e);

	for (i = 0; i < 8; i++) {
		ATF_CHECK(BIT(bus_read_1(&b, 0x10+i), i));
	}
}

ATF_TC_WITHOUT_HEAD(emul_wrap_izpx);
ATF_TC_BODY(emul_wrap_izpx, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.A = 0xAA;
	e.regs.X = 0xA0;

	bus_write_1(&b, 0xB0, 0x10);
	bus_write_1(&b, 0xB1, 0x20);
	bus_write_1(&b, 0x90, 0x11);
	bus_write_1(&b, 0x91, 0x20);

	bus_write_1(&b, 0x2011, 0x55);

	rk65c02_dump_regs(e.regs);
	ATF_REQUIRE(rom_start(&e, "test_emulation_wrap_izpx.rom", tc));
	rk65c02_dump_regs(e.regs);

	ATF_CHECK(bus_read_1(&b, 0x2010) == 0xAA);	
	ATF_CHECK(e.regs.A == 0x55);	
}

ATF_TC_WITHOUT_HEAD(emul_wrap_zpx);
ATF_TC_BODY(emul_wrap_zpx, tc)
{
	rk65c02emu_t e;
	bus_t b;
	uint16_t i;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);

	e.regs.A = 0xAA;
	e.regs.X = 0x7F;
	ATF_REQUIRE(rom_start(&e, "test_emulation_wrap_zpx.rom", tc));

	ATF_CHECK(bus_read_1(&b, 0x8F) == 0xAA);
	ATF_CHECK(bus_read_1(&b, 0xFF) == 0xAA);
	ATF_CHECK(bus_read_1(&b, 0x00) == 0xAA);
	ATF_CHECK(bus_read_1(&b, 0x01) == 0xAA);
	ATF_CHECK(bus_read_1(&b, 0x7E) == 0xAA);

	i = 0x200;

	while (i < 0x205) {
		ATF_CHECK(bus_read_1(&b, i) == 0xAA);
		i++;
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, emul_and);
	ATF_TP_ADD_TC(tp, emul_asl);
	ATF_TP_ADD_TC(tp, emul_adc_16bit);
	ATF_TP_ADD_TC(tp, emul_adc_bcd);
	ATF_TP_ADD_TC(tp, emul_bit);
	ATF_TP_ADD_TC(tp, emul_branch);
	ATF_TP_ADD_TC(tp, emul_cmp);
	ATF_TP_ADD_TC(tp, emul_cpx);
	ATF_TP_ADD_TC(tp, emul_cpy);
	ATF_TP_ADD_TC(tp, emul_dec);
	ATF_TP_ADD_TC(tp, emul_dex_dey);
	ATF_TP_ADD_TC(tp, emul_clc_sec);
	ATF_TP_ADD_TC(tp, emul_cli_sei);
	ATF_TP_ADD_TC(tp, emul_clv);
	ATF_TP_ADD_TC(tp, emul_inc);
	ATF_TP_ADD_TC(tp, emul_inx_iny);
	ATF_TP_ADD_TC(tp, emul_jmp);
	ATF_TP_ADD_TC(tp, emul_jsr_rts);
	ATF_TP_ADD_TC(tp, emul_lda);
	ATF_TP_ADD_TC(tp, emul_lsr);
	ATF_TP_ADD_TC(tp, emul_nop);
	ATF_TP_ADD_TC(tp, emul_ora);
	ATF_TP_ADD_TC(tp, emul_stz);
	ATF_TP_ADD_TC(tp, emul_php_plp);
	ATF_TP_ADD_TC(tp, emul_phx_phy_plx_ply);
	ATF_TP_ADD_TC(tp, emul_stack);
	ATF_TP_ADD_TC(tp, emul_txa_tya_tax_tay);
	ATF_TP_ADD_TC(tp, emul_sta);
	ATF_TP_ADD_TC(tp, emul_sbc);
	ATF_TP_ADD_TC(tp, emul_sbc_16bit);
	ATF_TP_ADD_TC(tp, emul_sbc_bcd);
	ATF_TP_ADD_TC(tp, emul_rmb);
	ATF_TP_ADD_TC(tp, emul_smb);

	ATF_TP_ADD_TC(tp, emul_sign_overflow_basic);
	ATF_TP_ADD_TC(tp, emul_sign_overflow_thorough);

	ATF_TP_ADD_TC(tp, emul_wrap_zpx);
	ATF_TP_ADD_TC(tp, emul_wrap_izpx);

	return (atf_no_error());
}

