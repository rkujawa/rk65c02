UNAME_S := $(shell uname -s)

CFLAGS=-Wall -I../src -ggdb -Og
LDFLAGS=-latf-c -lgc

LDFLAGS_MACOSX=-L/opt/local/lib
CFLAGS_MACOSX=-I/opt/local/include -I/opt/local/include/uthash

ifeq ($(UNAME_S),Darwin)
	CFLAGS+=$(CFLAGS_MACOSX)
	LDFLAGS+=$(LDFLAGS_MACOSX)
endif

RK6502LIB=../src/librk65c02.a
VASM=vasm6502_std
VASMFLAGS=-Fbin -c02
UTILS=utils.o

TESTS=test_bus test_emulation test_stepping test_assemble test_interrupt test_debug test_device_serial
TESTROMS:=$(addsuffix .rom,$(basename $(wildcard *.s)))

all : $(TESTS) $(TESTROMS)

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

test_device_serial: test_device_serial.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

%.rom : %.s
	$(VASM) $(VASMFLAGS) -o $@ $< 

%.o : %.c
	$(CC) $(CFLAGS) -c $< 

clean :
	rm -f *.o
	rm -f $(TESTS)
	rm -f $(TESTROMS)

