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

    // memset(full_mem + 0x400, 0x88, 256);  // bda
    // int image_fd = open("FD13BOOT.img", O_RDONLY);
    // int image_fd = open("dos11src.orig.img", O_RDONLY);
//    int image_fd = open("floppy", O_RDWR);
//    if (image_fd < 0) {
//        perror("unable to open a");
//        return 1;
//    }
//    read(image_fd, full_mem + 0x7c00, 64 * 1024);
//    bool has_dos = false;
//
//    if (1) {
//        int dos = open("dos/jwasm/STDDOS.COM", O_RDONLY);
//        if (dos != -1) {
//            read(dos, full_mem + dosseg * 16, 64 * 1024);
//            close(dos);
//            has_dos = true;
//
//            *((uint16_t *)&full_mem[0x8000 - 4]) =
//                0x100 * 2;  // ret from dos init. rip
//            *((uint16_t *)&full_mem[0x8000 - 2]) =
//                0xf000;  // ret from dos init. cs
//
//            full_mem[dos_io_seg * 16 + INITTAB] = 1;      // number of drive
//            full_mem[dos_io_seg * 16 + INITTAB + 1] = 0;  // disk id
//            full_mem[dos_io_seg * 16 + INITTAB + 2] =
//                DRVPARAM & 0xff;  // disk param
//            full_mem[dos_io_seg * 16 + INITTAB + 3] =
//                DRVPARAM >> 8;  // disk param
//            full_mem[dos_io_seg * 16 + INITTAB + 4] =
//                (DRVPARAM + 10) & 0xff;  // disk param
//            full_mem[dos_io_seg * 16 + INITTAB + 5] =
//                (DRVPARAM + 10) >> 8;  // disk param
//
//            full_mem[dos_io_seg * 16 + DRVPARAM + 0] =
//                512 & 0xff;  // num of sector lo
//            full_mem[dos_io_seg * 16 + DRVPARAM + 1] =
//                512 >> 8;                                  // num of sector high
//            full_mem[dos_io_seg * 16 + DRVPARAM + 2] = 2;  // cluster size
//            full_mem[dos_io_seg * 16 + DRVPARAM + 3] = 1;  // lo of ?
//            full_mem[dos_io_seg * 16 + DRVPARAM + 4] = 0;  // high of ?
//            full_mem[dos_io_seg * 16 + DRVPARAM + 5] = 2;  // fat count
//            full_mem[dos_io_seg * 16 + DRVPARAM + 6] =
//                112;  // lo of root dir entry
//            full_mem[dos_io_seg * 16 + DRVPARAM + 7] =
//                0;  // high of root dir entry
//            full_mem[dos_io_seg * 16 + DRVPARAM + 8] = 640 & 0xff;  // ?
//            full_mem[dos_io_seg * 16 + DRVPARAM + 9] = 640 >> 8;    // high of ?
//        }
//        {
//            std::ifstream ifs;
//            ifs.open("dos/syms4.txt");
//
//            while (1) {
//                std::string s;
//                if (!std::getline(ifs, s)) {
//                    break;
//                }
//
//                std::stringstream ss(s);
//                std::string v, v2;
//                int hexval;
//
//                std::getline(ss, v, ',');
//                ss >> std::hex >> hexval;
//
//                dos_map.insert(std::make_pair(hexval, v));
//            }
//        }
//    }
//
//    capability_test(kvm_fd, KVM_CAP_USER_MEMORY, "KVM_CAP_USER_MEMORY");
//    int nr_memslots =
//        ioctl(kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_NR_MEMSLOTS, NULL);
//    if (nr_memslots < 0) {
//        perror("KVM_CAP_NR_MEMSLOTS");
//        return 1;
//    }
//    if (nr_memslots < 3) {
//        fprintf(stderr, "too few mem slots (%d)\n", nr_memslots);
//        return 1;
//    }
//
//    int vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, NULL);
//    int vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, (void *)0, NULL);
//    size_t vcpu_region_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, NULL);
//    struct kvm_run *run = (struct kvm_run *)mmap(
//        0, vcpu_region_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, vcpu_fd, 0);
//
//    ioctl(vcpu_fd, KVM_RUN, NULL);
//
//    setup_vcpu(&vm, has_dos);
//    // signal(SIGINT, sigint);
//
//    bool done = false;
//    struct kvm_guest_debug single_step = {};
//    single_step.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP;
//    // ioctl(vcpu_fd, KVM_SET_GUEST_DEBUG, &single_step, NULL);
//
//    //    std::thread tK([&q_lock, &key_queue]() {
//    //        while (1) {
//    //            int c = getchar();
//    //            if (c == EOF) {
//    //                break;
//    //            }
//    //
//    //            std::lock_guard lk(q_lock);
//    //            key_queue.push(c);
//    //        }
//    //    });
//
//    while (!done) {
//        if (debug) {
//            ioctl(vcpu_fd, KVM_SET_GUEST_DEBUG, &single_step, NULL);
//        }
//        ioctl(vcpu_fd, KVM_RUN, NULL);
//
//        switch (run->exit_reason) {
//            case KVM_EXIT_INTERNAL_ERROR:
//                printf("exit internal error : %d [",
//                       (int)run->internal.suberror);
//                for (int ei = 0; ei < run->internal.ndata; ei++) {
//                    printf("%llx, ", (long long)run->internal.data[ei]);
//                }
//                printf("]\n");
//                dump_regs(vcpu_fd);
//                dump_ivt(full_mem);
//                done = 1;
//                break;
//
//            case KVM_EXIT_MMIO:
//                printf("reference unmapped region : addr=%16llx\n",
//                       (long long)run->mmio.phys_addr);
//                dump_regs(vcpu_fd);
//                disasm(vcpu_fd, full_mem);
//                dump_ivt(full_mem);
//                done = 1;
//                break;
//
//            case KVM_EXIT_SHUTDOWN:
//                printf("kvm exit shutdown\n");
//                dump_regs(vcpu_fd);
//                done = 1;
//                break;
//
//            case KVM_EXIT_HLT:
//                // disasm(vcpu_fd, full_mem);
//                handle_hlt(vcpu_fd, full_mem, image_fd);
//                break;
//
//            case KVM_EXIT_FAIL_ENTRY:
//                printf(
//                    "fail entry : reason=%llx\n",
//                    (long long)run->fail_entry.hardware_entry_failure_reason);
//                dump_regs(vcpu_fd);
//                done = 1;
//                break;
//
//            case KVM_EXIT_DEBUG: {
//                struct kvm_regs regs;
//                ioctl(vcpu_fd, KVM_GET_REGS, &regs, NULL);
//                struct kvm_sregs sregs;
//                ioctl(vcpu_fd, KVM_GET_SREGS, &sregs, NULL);
//                if (1) {
//                    disasm(vcpu_fd, full_mem);
//                    // if (full_mem[regs.rip + sregs.cs.base] == 0xcd &&
//                    //     full_mem[regs.rip + sregs.cs.base + 1] == 0x21) {
//                    //     dump_regs(vcpu_fd);
//                    //     dump_ivt(full_mem);
//                    // }
//                }
//            } break;
//
//            case KVM_EXIT_IO:
//                printf("%x %x %x\n", run->io.direction, run->io.port,
//                       run->io.size);
//                dump_regs(vcpu_fd);
//                dump_ivt(full_mem);
//                disasm(vcpu_fd, full_mem);
//                done = 1;
//                break;
//
//            case KVM_EXIT_INTR: {
//                struct kvm_regs regs;
//                ioctl(vcpu_fd, KVM_GET_REGS, &regs, NULL);
//                struct kvm_sregs sregs;
//                ioctl(vcpu_fd, KVM_GET_SREGS, &sregs, NULL);
//                disasm(vcpu_fd, full_mem);
//                dump_regs(vcpu_fd);
//                done = 1;
//                break;
//            }
//
//            default:
//                printf("unknown exit %d\n", run->exit_reason);
//                dump_regs(vcpu_fd);
//                done = 1;
//                break;
//        }
//    }
//
//    fflush(stdout);

    return 0;
}
