UNAME_S := $(shell uname -s)

CFLAGS=-Wall -pedantic -I../src -g -MMD -MP
# When linking against library built without JIT: make NO_LIGHTNING=1
LDFLAGS=-lgc $(if $(NO_LIGHTNING),,-llightning)

LDFLAGS_MACOSX=-L/opt/local/lib
CFLAGS_MACOSX=-I/opt/local/include

ifeq ($(UNAME_S),Darwin)
	CFLAGS+=$(CFLAGS_MACOSX)
	LDFLAGS+=$(LDFLAGS_MACOSX)
endif

RK6502LIB=../src/librk65c02.a
VASM=vasm6502_std
VASMFLAGS=-Fbin -wdc02

EXAMPLES=min3 mul_8bit_to_8bits host_control idle_wait interrupts hello_serial stepper jit_bench breakpoints mmu_cart mmu_multitasking mmu_pae mmu_mpu
EXAMPLES_ROMS:=$(addsuffix .rom,$(basename $(wildcard *.s)))

all : $(EXAMPLES) $(EXAMPLES_ROMS)

msbasic : $(RK6502LIB)
	$(MAKE) -C msbasic all

min3 : min3.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

mul_8bit_to_8bits : mul_8bit_to_8bits.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

host_control : host_control.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

idle_wait : idle_wait.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

interrupts : interrupts.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

hello_serial : hello_serial.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

stepper : stepper.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

jit_bench : jit_bench.o min3.rom $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

breakpoints : breakpoints.o min3.rom $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

mmu_cart : mmu_cart.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

mmu_multitasking : mmu_multitasking.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

mmu_pae : mmu_pae.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

mmu_mpu : mmu_mpu.o $(RK6502LIB)
	$(CC) -o $@ $(LDFLAGS) $< $(RK6502LIB)

%.rom : %.s
	$(VASM) $(VASMFLAGS) -o $@ $<

# Run examples under timeout so they never hang (use: make run-<name>)
TIMEOUT ?= 5
run-mmu_mpu: mmu_mpu mmu_mpu.rom
	timeout $(TIMEOUT) ./mmu_mpu
run-mmu_pae: mmu_pae mmu_pae.rom
	timeout $(TIMEOUT) ./mmu_pae
run-mmu_multitasking: mmu_multitasking mmu_multitasking_kernel.rom mmu_multitasking_task.rom
	timeout $(TIMEOUT) ./mmu_multitasking
run-mmu_cart: mmu_cart mmu_cart_bank0.rom mmu_cart_bank1.rom
	timeout $(TIMEOUT) ./mmu_cart
run-min3: min3 min3.rom
	timeout $(TIMEOUT) ./min3
run-interrupts: interrupts interrupts.rom
	timeout $(TIMEOUT) ./interrupts
run-hello_serial: hello_serial hello_serial.rom
	./hello_serial
run-stepper: stepper stepper.rom
	./stepper
run-jit_bench: jit_bench min3.rom
	./jit_bench
run-breakpoints: breakpoints min3.rom
	./breakpoints
run-msbasic: msbasic
	cd msbasic && ./run_msbasic

%.o : %.c
	$(CC) $(CFLAGS) -c $<
clean :
	rm -f *.o
	rm -f *.d
	rm -f $(EXAMPLES) $(EXAMPLES_ROMS)

-include $(wildcard *.d)

