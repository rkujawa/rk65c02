CFLAGS=-Wall -I../src
LDFLAGS=-latf-c ../src/librk65c02.a

test_bus : test_bus.o
	$(CC) -o test_bus $(LDFLAGS) $<

%.o : %.c
	$(CC) $(CFLAGS) -c $< 


