vm: floppy.o cpu.o dump.o disasm.o vm_vm.o dos.o vm_bios.o

ASFLAGS=-32
CFLAGS=-m16 -fno-pic -fno-builtin -Os -ffreestanding

all: bios.bin

bios.o: bios.s

OBJS=bios.o cstart.o
bios.bin: $(OBJS) bios.lds
	ld -Map a.map -T bios.lds $(OBJS) -o $@


clean:
	-rm -f *.o
