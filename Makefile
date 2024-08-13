ASFLAGS=-32
CFLAGS=-m16 -fno-pic -fno-builtin -Os -ffreestanding

all: bios.bin

bios.o: bios.s

OBJS=bios.o cstart.o
bios.bin: $(OBJS) bios.lds
	ld -Map a.map -T bios.lds $(OBJS) -o $@


clean:
	-rm -f *.o
