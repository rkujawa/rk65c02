CFLAGS=-Wall -I../src -g
LDFLAGS=-latf-c 
RK6502LIB=../src/librk65c02.a
VASM=vasm6502_std
VASMFLAGS=-Fbin -c02

TESTS=test_bus test_emulation
TESTROMS=test_emulation_nop.rom test_emulation_lda_imm.rom test_emulation_lda_zp.rom

all : $(TESTS) $(TESTROMS)

test_bus : test_bus.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

test_emulation : test_emulation.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

%.rom : %.s
	$(VASM) $(VASMFLAGS) -o $@ $< 

%.o : %.c
	$(CC) $(CFLAGS) -c $< 

clean :
	rm -f *.o
	rm -f $(TESTS)
	rm -f $(TESTROMS)

