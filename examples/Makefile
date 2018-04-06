CFLAGS=-Wall -pedantic -I../src -g
LDFLAGS=-latf-c -lgc
RK6502LIB=../src/librk65c02.a
VASM=vasm6502_std
VASMFLAGS=-Fbin -c02

EXAMPLES=min3
EXAMPLES_ROMS:=$(addsuffix .rom,$(basename $(wildcard *.s)))

all : $(EXAMPLES) $(EXAMPLES_ROMS)

min3 : min3.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

%.rom : %.s
	$(VASM) $(VASMFLAGS) -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) -c $<
clean :
	rm -f *.o
	rm -f $(EXAMPLES) $(EXAMPLES_ROMS)

