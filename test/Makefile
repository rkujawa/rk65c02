CFLAGS=-Wall -I../src
LDFLAGS=-L../src/ -latf-c -lrk65c02
#-Wl,-R../src/

test_bus : test_bus.o
	$(CC) -o test_bus $(LDFLAGS) $<

%.o : %.c
	$(CC) $(CFLAGS) -c $< 


