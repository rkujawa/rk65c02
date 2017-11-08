CLI=g65c02
CLI_OBJ=$(CLI).o

LIB_OBJS=rk65c02.o bus.o instruction.o emulation.o debug.o device_ram.o device_serial.o
LIB_SO=librk65c02.so
LIB_STATIC=librk65c02.a

LDFLAGS_SO=-shared
LDFLAGS_CLI=`pkg-config --libs guile-2.2`
CFLAGS=-Wall -fpic -ggdb $(CFLAGS_EXTRA)
CFLAGS_CLI=`pkg-config --cflags guile-2.2`

65C02ISA=65c02isa
EMULATION=emulation
DEVICE=device
# TODO: better include handling

all : $(LIB_SO) $(LIB_STATIC) $(CLI)

$(CLI) : $(CLI_OBJ)
	$(CC) -o $(CLI) $(LDFLAGS_CLI) $(CLI_OBJ) $(LIB_STATIC)

$(CLI_OBJ) : $(CLI).c
	$(CC) $(CFLAGS_CLI) -c $<

$(LIB_SO) : $(LIB_OBJS)
	$(CC) -o $(LIB_SO) $(LDFLAGS_SO) $(LIB_OBJS)

$(LIB_STATIC) : $(LIB_OBJS)
	$(AR) rcs $(LIB_STATIC) $(LIB_OBJS)

$(65C02ISA).h : $(65C02ISA).csv $(65C02ISA).awk
	awk -f $(65C02ISA).awk $(65C02ISA).csv > $(65C02ISA).h 

$(EMULATION).h : $(65C02ISA).csv $(EMULATION).awk
	awk -f $(EMULATION).awk $(65C02ISA).csv > $(EMULATION).h

# XXX: dependency on 65c02isa.h is only for instruction.c ?
%.o : %.c %.h $(65C02ISA).h $(EMULATION).h $(DEVICE).h
	$(CC) $(CFLAGS) -c $<

clean :
	rm -f $(65C02ISA).h $(EMULATION).h
	rm -f $(LIB_OBJS) $(CLI_OBJ)
	rm -f $(LIB_SO) $(LIB_STATIC) $(CLI)

