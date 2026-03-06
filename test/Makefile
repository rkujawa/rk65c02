UNAME_S := $(shell uname -s)

CFLAGS=-Wall -I../src -O3 -MMD -MP
#CFLAGS=-Wall -I../src -ggdb -Og -MMD -MP
LDFLAGS=-latf-c -lgc -llightning

LDFLAGS_MACOSX=-L/opt/local/lib
CFLAGS_MACOSX=-I/opt/local/include -I/opt/local/include/uthash

ifeq ($(UNAME_S),Darwin)
	CFLAGS+=$(CFLAGS_MACOSX)
	LDFLAGS+=$(LDFLAGS_MACOSX)
endif

RK6502LIB=../src/librk65c02.a
VASM=vasm6502_std
VASMFLAGS=-Fbin -wdc02
UTILS=utils.o

TESTS=test_bus test_emulation test_stepping test_assemble test_interrupt test_debug test_functional
BENCH=bench_emulation
TESTROMS:=$(addsuffix .rom,$(basename $(wildcard *.s)))
FUNCTIONAL_DIR=functional_tests

all : $(TESTS) $(BENCH) $(TESTROMS) functional_bins

functional_bins:
	$(MAKE) -C $(FUNCTIONAL_DIR)

test_stepping : test_stepping.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

test_bus : test_bus.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

test_emulation : test_emulation.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

test_assemble : test_assemble.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

test_interrupt: test_interrupt.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

test_debug: test_debug.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

test_functional: test_functional.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

bench_emulation: bench_emulation.o $(RK6502LIB)
	$(CC) -o $@ $< $(RK6502LIB) -lgc -llightning

test_device_serial: test_device_serial.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

%.rom : %.s
	$(VASM) $(VASMFLAGS) -o $@ $< 

%.o : %.c
	$(CC) $(CFLAGS) -c $< 

clean :
	$(MAKE) -C $(FUNCTIONAL_DIR) clean
	rm -f *.o
	rm -f *.d
	rm -f $(TESTS) $(BENCH)
	rm -f $(TESTROMS)

-include $(wildcard *.d)

