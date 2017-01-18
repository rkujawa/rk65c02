CFLAGS=-Wall -I../src -g
LDFLAGS=-latf-c 
RK6502LIB=../src/librk65c02.a
TESTS=test_bus test_emulation

all : $(TESTS) 

test_bus : test_bus.o 
	$(CC) -o test_bus $(LDFLAGS) $< $(RK6502LIB)

test_emulation : test_emulation.o
	$(CC) -o test_emulation $(LDFLAGS) $< $(RK6502LIB)

%.o : %.c
	$(CC) $(CFLAGS) -c $< 

clean :
	rm -f *.o
	rm -f $(TESTS)

