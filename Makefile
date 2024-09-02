LINK.o=g++
CXXFLAGS=-Wall -O2 -MMD
vm: floppy.o cpu.o dump.o disasm.o vm_vm.o dos.o vm_bios.o vm_main.o
	$(LINK.o) -o $@ $^

clean:
	-rm -f *.o *.d vm

-include *.d
