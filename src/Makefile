OBJS=rk65c02.o bus.o
EXEC=rk65c02
LDFLAGS=
CFLAGS=-Wall

$(EXEC) : $(OBJS)
	$(CC) -o $(EXEC) $(LDFLAGS) $(OBJS)

%.o : %.c %.h
	$(CC) $(CFLAGS) -c $<

clean :
	rm -f $(OBJS)
	rm -f $(EXEC) 

