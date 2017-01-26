CFLAGS=-Wall -I../src -g
LDFLAGS=-latf-c 
RK6502LIB=../src/librk65c02.a
VASM=vasm6502_std
VASMFLAGS=-Fbin -c02
UTILS=utils.o

TESTS=test_bus test_emulation test_stepping
TESTROMS:=$(addsuffix .rom,$(basename $(wildcard *.s)))

all : $(TESTS) $(TESTROMS)

test_stepping : test_stepping.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

test_bus : test_bus.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

test_emulation : test_emulation.o $(UTILS) $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(UTILS) $(RK6502LIB)

%.rom : %.s
	$(VASM) $(VASMFLAGS) -o $@ $< 

%.o : %.c
	$(CC) $(CFLAGS) -c $< 

clean :
	rm -f *.o
	rm -f $(TESTS)
	rm -f $(TESTROMS)

