#include <elf.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include <map>
#include <optional>
#include <string>

#include "vm.hpp"

bool debug = false;
uint16_t dos21_seg = 0;
uint16_t dos21_offset = 0;
std::optional<char> key_queue;
std::map<int, std::string> dos_map;

int main(int argc, char **argv) {
    VM vm;
    if (argc < 2) {
        return 1;
    }

    size_t argv_len = strlen(argv[1]);
    setup_ivt(&vm);

    vm.run_mode = RUN_MODE::DOS_KERNEL;
    if (argv_len > 4) {
        if ((argv[1][argv_len - 4] == '.') && (argv[1][argv_len - 3] == 'E') &&
            (argv[1][argv_len - 2] == 'X') && (argv[1][argv_len - 1] == 'E')) {
            vm.run_mode = RUN_MODE::DOS_EXE;
        } else if ((argv[1][argv_len - 4] == '.') &&
                   (argv[1][argv_len - 3] == 'C') &&
                   (argv[1][argv_len - 2] == 'O') &&
                   (argv[1][argv_len - 1] == 'M')) {
            vm.run_mode = RUN_MODE::DOS_COM;
        }
    }

    if (vm.run_mode == RUN_MODE::DOS_KERNEL) {
        vm.set_floppy(argv[1]);
        install_dos_driver(&vm);
    }
    vm.cpu->setup(vm.addr_config, vm.run_mode);

    if (vm.run_mode == RUN_MODE::DOS_EXE) {
        std::string dos_argv = "";
        if (argc > 2) {
            dos_argv = argv[2];
        }

        if (load_mz(&vm, argv[1], dos_argv) == -1) {
            return 1;
        }
    } else if (vm.run_mode == RUN_MODE::DOS_KERNEL) {
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
        memcpy(vm.full_mem + addr + 0x100, command_com->data(),
               command_com->size());

        set_seg(vm.cpu->sregs.es, vm.cpu->sregs.ds.selector);
        set_seg(vm.cpu->sregs.ss, vm.cpu->sregs.ds.selector);
        set_seg(vm.cpu->sregs.cs, vm.cpu->sregs.ds.selector);
        vm.cpu->regs.rsp = 0x5c;
        vm.cpu->regs.rip = 0x100;  // command com start
    }

    run_with_handler(&vm);

    return 0;
}
