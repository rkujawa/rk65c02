OBJS=rk65c02.o bus.o instruction.o emulation.o
LIB_SO=librk65c02.so
LIB_STATIC=librk65c02.a
LDFLAGS=-shared
CFLAGS=-Wall -fpic

65C02ISA=65c02isa

all : $(LIB_SO) $(LIB_STATIC)

$(LIB_SO) : $(OBJS)
	$(CC) -o $(LIB_SO) $(LDFLAGS) $(OBJS)

$(LIB_STATIC) : $(OBJS)
	$(AR) rcs $(LIB_STATIC) $(OBJS)

$(65C02ISA).h : $(65C02ISA).csv $(65C02ISA).awk
	awk -f $(65C02ISA).awk $(65C02ISA).csv > $(65C02ISA).h 

# XXX: dependency on 65c02isa.h is only for instruction.c 
%.o : %.c %.h $(65C02ISA).h
	$(CC) $(CFLAGS) -c $<

clean :
	rm -f $(65C02ISA).h
	rm -f $(OBJS)
	rm -f $(LIB_SO) $(LIB_STATIC)

