# This needs fairly recent GNU Make.
# Build with JIT (default): make -C src
# Build without JIT:       make -C src HAVE_LIGHTNING=0
#
UNAME_S := $(shell uname -s)

HAVE_LIGHTNING ?= 1

LIB_OBJS=rk65c02.o bus.o instruction.o emulation.o debug.o device_ram.o log.o assembler.o
ifeq ($(HAVE_LIGHTNING),1)
LIB_OBJS += jit.o
else
LIB_OBJS += jit_stub.o
endif

ifeq ($(UNAME_S),Darwin)
    LIB_SO=librk65c02.dylib
else
    LIB_SO=librk65c02.so
endif

LIB_STATIC=librk65c02.a

LDFLAGS=-shared -lgc
LDFLAGS_MACOSX=-L/opt/local/lib
#CFLAGS=-Wall -fpic -ggdb -Og -DHAVE_LIGHTNING -MMD -MP
CFLAGS=-Wall -Wpedantic -fpic -O3 -MMD -MP
ifeq ($(HAVE_LIGHTNING),1)
CFLAGS += -DHAVE_LIGHTNING
LDFLAGS += -llightning
endif
#CFLAGS=-Wall -fpic -Os -fplugin=annobin
CFLAGS_MACOSX=-I/opt/local/include/uthash -I/opt/local/include

ifeq ($(UNAME_S),Darwin)
    CFLAGS+=$(CFLAGS_MACOSX)
    LDFLAGS+=$(LDFLAGS_MACOSX)
endif

65C02ISA=65c02isa
EMULATION=emulation
DEVICE=device

all : $(LIB_SO) $(LIB_STATIC) $(CLI)

#$(CLI) : $(CLI_OBJS)
#	$(CC) -o $(CLI) $(LDFLAGS) $(CLI_OBJS) $(LIB_STATIC)

$(LIB_SO) : $(LIB_OBJS)
	$(CC) -o $(LIB_SO) $(LDFLAGS) $(LIB_OBJS)

$(LIB_STATIC) : $(LIB_OBJS)
	$(AR) rcs $(LIB_STATIC) $(LIB_OBJS)

$(65C02ISA).h : $(65C02ISA).csv $(65C02ISA).awk
	awk -f $(65C02ISA).awk $(65C02ISA).csv > $(65C02ISA).h 

$(EMULATION).h : $(65C02ISA).csv $(EMULATION).awk
	awk -f $(EMULATION).awk $(65C02ISA).csv > $(EMULATION).h

# Keep generated ISA/emulation headers as prerequisites for core objects.
%.o : %.c %.h $(65C02ISA).h $(EMULATION).h $(DEVICE).h
	$(CC) $(CFLAGS) -c $<

# jit_stub.c has no matching .h; use jit.h for declarations.
jit_stub.o : jit_stub.c jit.h rk65c02.h $(65C02ISA).h $(EMULATION).h $(DEVICE).h
	$(CC) $(CFLAGS) -c jit_stub.c

clean :
	rm -f $(65C02ISA).h $(EMULATION).h
	rm -f *.d
	rm -f $(LIB_OBJS) jit_stub.o #$(CLI_OBJS)
	rm -f $(LIB_SO) $(LIB_STATIC) #$(CLI)

-include $(wildcard *.d)

