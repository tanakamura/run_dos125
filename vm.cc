#include "vm.hpp"

#include <elf.h>
#include <fcntl.h>
#include <getopt.h>
#include <hugetlbfs.h>
#include <limits.h>
#include <linux/kvm.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

#include "dosdriver.h"

bool debug = false;
uint16_t dos21_seg = 0;
uint16_t dos21_offset = 0;
std::optional<char> key_queue;
static const int dos_io_seg = 0x60;
static const int INITTAB = 16 * 3;
static const int DRVPARAM = 16 * 3 + 8;
static const int DOS_IO_SIZE = 128;
static const int dosseg = dos_io_seg + DOS_IO_SIZE / 16;
std::map<int, std::string> dos_map;

int main(int argc, char **argv) {
    VM vm;
    vm.set_floppy(argv[1]);
    install_dos_driver(&vm);
    vm.cpu->setup(vm.addr_config, vm.run_mode);

    set_seg(vm.cpu->sregs.es, vm.addr_config.dos_seg);
    set_seg(vm.cpu->sregs.ds, vm.addr_config.dos_io_seg);

    /* dos init */
    vm.emu_far_call(vm.addr_config.dos_seg, 0);

    int addr = vm.cpu->sregs.ds.base;

    auto command_com = vm.floppy->read("COMMAND ", "COM");
    if (!command_com) {
        fprintf(stderr, "unable to read COMMAND.COM");
        exit(1);
    }
    memcpy(vm.full_mem + addr + 0x100, command_com->data(), command_com->size());

    set_seg(vm.cpu->sregs.es, vm.cpu->sregs.ds.selector);
    set_seg(vm.cpu->sregs.ss, vm.cpu->sregs.ds.selector);
    set_seg(vm.cpu->sregs.cs, vm.cpu->sregs.ds.selector);
    vm.cpu->regs.rsp = 0x5c;
    vm.cpu->regs.rip = 0x100;   // command com start

    run_with_handler(&vm);

    return 0;
}
