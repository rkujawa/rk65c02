OBJS=rk65c02.o bus.o
SO=librk65c02.so
LDFLAGS=-shared
CFLAGS=-Wall -fpic

$(SO) : $(OBJS)
	$(CC) -o $(SO) $(LDFLAGS) $(OBJS)

%.o : %.c %.h
	$(CC) $(CFLAGS) -c $<

clean :
	rm -f $(OBJS)
	rm -f $(SO) 

