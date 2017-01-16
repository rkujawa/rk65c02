CFLAGS=-Wall -I../src
LDFLAGS=-latf-c 
RK6502LIB=../src/librk65c02.a

test_bus : test_bus.o
	$(CC) -o test_bus $(LDFLAGS) $< $(RK6502LIB)

%.o : %.c
	$(CC) $(CFLAGS) -c $< 


