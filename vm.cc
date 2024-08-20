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
#include "vm.hpp"

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

    return 0;
}
