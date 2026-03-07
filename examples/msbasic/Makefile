# MS BASIC example: host + BASIC ROM + I/O stub.
# Requires: ca65, ld65; git submodule msbasic/; patches applied.
# From repo root: make -C examples/msbasic, or from examples/: make msbasic

UNAME_S := $(shell uname -s)
CFLAGS = -Wall -pedantic -I../../src -g -MMD -MP
LDFLAGS = -lgc $(if $(NO_LIGHTNING),,-llightning)
LDFLAGS_MACOSX = -L/opt/local/lib
CFLAGS_MACOSX = -I/opt/local/include
ifeq ($(UNAME_S),Darwin)
	CFLAGS += $(CFLAGS_MACOSX)
	LDFLAGS += $(LDFLAGS_MACOSX)
endif

RK65C02LIB = ../../src/librk65c02.a
CA65 = ca65
LD65 = ld65
MSBASIC_S = msbasic/msbasic.s
PATCHES = patches/defines.s.patch patches/extra.s.patch patches/iscntc.s.patch patches/loadsave.s.patch patches/init.s.patch patches/print.s.patch
PATCHED_MARK = msbasic/.rk65c02-patched

.PHONY: all clean patch

HOST_BIN = run_msbasic

all: $(HOST_BIN) basic.bin iostub.bin basic.lbl

$(HOST_BIN): msbasic_host.o $(RK65C02LIB) basic.bin iostub.bin basic.lbl
	$(CC) -o $@ $(LDFLAGS) msbasic_host.o $(RK65C02LIB)

basic.bin basic.map basic.lbl: msbasic.o rk65c02.cfg $(PATCHED_MARK)
	$(LD65) -C rk65c02.cfg msbasic.o -o basic.bin -m basic.map -Ln basic.lbl

msbasic.o: $(MSBASIC_S) rk65c02.cfg defines_rk65c02.s rk65c02_extra.s rk65c02_iscntc.s rk65c02_loadsave.s $(PATCHED_MARK)
	$(CA65) -D rk65c02 -I. $(MSBASIC_S) -o msbasic.o

iostub.bin: rk65c02_io.o iostub.cfg
	$(LD65) -C iostub.cfg rk65c02_io.o -o iostub.bin

rk65c02_io.o: rk65c02_io.s
	$(CA65) rk65c02_io.s -o rk65c02_io.o

$(PATCHED_MARK): msbasic/msbasic.s $(PATCHES) patches/init.s.hunk4.patch
	@for p in $(PATCHES); do \
		(cd msbasic && patch -p1 --forward < "../$$p") || true; \
	done
	@(cd msbasic && patch -p1 --forward < ../patches/init.s.hunk4.patch) || true
	@touch $(PATCHED_MARK)

patch: $(PATCHED_MARK)

msbasic_host.o: msbasic_host.c
	$(CC) $(CFLAGS) -c -o $@ msbasic_host.c

clean:
	rm -f msbasic.o rk65c02_io.o msbasic_host.o msbasic_host.d
	rm -f basic.bin basic.map basic.lbl iostub.bin
	rm -f $(HOST_BIN)
	rm -f $(PATCHED_MARK)

-include msbasic_host.d
