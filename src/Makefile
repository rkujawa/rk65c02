OBJS=rk65c02.o bus.o instruction.o 65c02isa.o
LIB_SO=librk65c02.so
LIB_STATIC=librk65c02.a
LDFLAGS=-shared
CFLAGS=-Wall -fpic

all : $(LIB_SO) $(LIB_STATIC)

$(LIB_SO) : $(OBJS)
	$(CC) -o $(LIB_SO) $(LDFLAGS) $(OBJS)

$(LIB_STATIC) : $(OBJS)
	$(AR) rcs $(LIB_STATIC) $(OBJS)

%.o : %.c %.h
	$(CC) $(CFLAGS) -c $<

clean :
	rm -f $(OBJS)
	rm -f $(LIB_SO) $(LIB_STATIC)

