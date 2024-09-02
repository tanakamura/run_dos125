#include "dos.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <cstdint>

#include "dosdriver.h"
#include "vm.hpp"

namespace {
struct datetime {
    uint16_t days;
    uint8_t milsec;
    uint8_t sec;
    uint8_t hour;
    uint8_t min;
};
datetime dos_gettime() {
    time_t now = time(NULL);
    auto tm = localtime(&now);

    datetime ret;

    ret.days = tm->tm_yday;
    ret.milsec = 0;
    ret.sec = tm->tm_sec;
    ret.min = tm->tm_min;
    ret.hour = tm->tm_hour;
    return ret;
}

const char *truncate_drive(const char *p) {
    if (p[0] == '\0' || p[1] == '\0') {
        return p;
    }

    if (p[1] == ':') {
        return p + 2;
    }
    return p;
}

}  // namespace

void handle_dos_driver_call(VM *vm, const ExitReason *r) {
    auto &regs = vm->cpu->regs;
    auto &sregs = vm->cpu->sregs;
    auto full_mem = vm->full_mem;

    bool zf = 0;
    bool cf = 0;

    switch (r->dos_driver_call) {
        case DOSIO_STATUS:
            zf = 1;
            break;
        case DOSIO_INP: {
            int a = getchar();
            if (a == '\n') {
                a = '\r';
            }
            regs.rax = a;
            zf = 0;
        } break;
        case DOSIO_OUTP:
            // puts("print");
            putchar(regs.rax & 0xff);
            break;

        case DOSIO_READ:  // disk read
            /*
             * AL = Disk I/O driver number
             * BX = Disk transfer address in DS
             * CX = Number of sectors to transfer
             * DX = Logical record number of transfer
             */

            {
                auto addr = &full_mem[sregs.ds.base + regs.rbx];
                pread(vm->floppy->image_fd, addr, regs.rcx * 512,
                      regs.rdx * 512);
                if (0) {
                    printf(
                        "disk read addr=0x%08x, "
                        "sector=0x%08x(byte=0x%08x), "
                        "count=%x\n",
                        (int)(uintptr_t)(sregs.ds.base + regs.rbx),
                        (int)regs.rdx, (int)regs.rdx * 512,
                        (int)regs.rcx * 512);
                }
            }
            regs.rax = 0;
            cf = 0;
            break;

        case DOSIO_WRITE:  // disk read
            /*
             * AL = Disk I/O driver number
             * BX = Disk transfer address in DS
             * CX = Number of sectors to transfer
             * DX = Logical record number of transfer
             */

            {
                auto addr = &full_mem[sregs.ds.base + regs.rbx];
                pwrite(vm->floppy->image_fd, addr, regs.rcx * 512,
                       regs.rdx * 512);
                if (0) {
                    printf(
                        "disk read addr=0x%08x, "
                        "sector=0x%08x(byte=0x%08x), "
                        "count=%x\n",
                        (int)(uintptr_t)(sregs.ds.base + regs.rbx),
                        (int)regs.rdx, (int)regs.rdx * 512,
                        (int)regs.rcx * 512);
                }
            }
            regs.rax = 0;
            cf = 0;
            break;

        case DOSIO_DSKCHG:
            regs.rax = 0;
            break;
        case DOSIO_FLUSH:
            fflush(stdout);
            break;

        case DOSIO_MAPDEV:
            regs.rax = 0;  // always use 0
            break;

        case DOSIO_GETTIME: {
            auto t = dos_gettime();
            regs.rax = t.days;
            regs.rdx = (t.sec << 8) | t.milsec;
            regs.rcx = (t.hour << 8) | (t.min);
        } break;

        default:
            printf("unknown dos driver call %02x\n", r->dos_driver_call);
            exit(1);
            break;
    }

    regs.rflags &= ~((1 << 6) | (1 << 0));  // clear zf, cf
    if (zf) {
        regs.rflags |= (1 << 6);
    }
    if (cf) {
        regs.rflags |= (1 << 0);
    }

    vm->emu_far_ret();
}

void handle_dos_system_call(VM *vm, const ExitReason *r) {
    uint8_t ah = (vm->cpu->regs.rax >> 8) & 0xff;
    vm->inthandler_clear_cf();
    // printf("dos call %x\n", ah);
    // dump_regs(vm->cpu.get());
    switch (ah) {
        case 0x2: {
            uint8_t dl = vm->cpu->regs.rdx & 0xff;
            putchar(dl);
            fflush(stdout);
        } break;

        case 0x09: {
            char *p = (char *)(vm->full_mem + vm->cpu->sregs.ds.base +
                               vm->cpu->regs.rdx);
            while ((*p) != '$') {
                putchar(*p);
                p++;
            }
            putchar('\n');
            vm->cpu->regs.rax = 0x0024;
        } break;
        case 0x0a: {
            uint8_t *p = (uint8_t *)(vm->full_mem + vm->cpu->sregs.ds.base +
                                     vm->cpu->regs.rdx);
            uint8_t len = p[0];
            ssize_t rdsz = read(0, p + 2, len);
            p[1] = rdsz;
        } break;

        case 0x19: {
            vm->cpu->regs.rax = 0;
        } break;

        case 0x25: {
            uint8_t al = vm->cpu->regs.rax & 0xff;

            switch (al) {
                case 0x34:
                    vm->cpu->regs.rax = 0;
                    break;
                default:
                    //printf("unknown 0x25 al=0x%x\n", al);
                    vm->inthandler_set_cf();
                    break;
            }
        } break;

    case 0x2a: {
        vm->cpu->regs.rcx = 1;  // 1981
        vm->cpu->regs.rdx = 0x0101;  // 1
        } break;

        case 0x30:
            vm->cpu->regs.rax = 0x00000002;  // 2.0
            vm->cpu->regs.rbx = 0x00000000;
            vm->inthandler_set_cf();
            break;
        case 0x37:
            vm->cpu->regs.rdx = 1;
            break;

        case 0x3c: {
            const char *p = (char *)(vm->full_mem + vm->cpu->sregs.ds.base +
                                     vm->cpu->regs.rdx);
            p = truncate_drive(p);
            int fd = creat(p, 0644);
            if (fd < 0) {
                vm->inthandler_set_cf();
            } else {
                vm->cpu->regs.rax = fd;
            }
        } break;

        case 0x3d: {
            const char *p = (char *)(vm->full_mem + vm->cpu->sregs.ds.base +
                                     vm->cpu->regs.rdx);
            p = truncate_drive(p);
            int mode = 0;
            uint8_t al = vm->cpu->regs.rax & 0xff;
            if (al == 0) {
                mode = O_RDONLY;
            } else if (al == 1) {
                mode = O_WRONLY;
            } else {
                mode = O_RDWR;
            }
            int fd = open(p, mode);
            if (fd < 0) {
                vm->inthandler_set_cf();
            } else {
                vm->cpu->regs.rax = fd;
            }
        } break;

        case 0x3e: {
            close(vm->cpu->regs.rbx);
            break;
        }

        case 0x3f:
        case 0x40: {
            char *p = (char *)(vm->full_mem + vm->cpu->sregs.ds.base +
                               vm->cpu->regs.rdx);
            ssize_t sz;
            if (ah == 0x3f) {
                sz = read(vm->cpu->regs.rbx, p, vm->cpu->regs.rcx);
            } else {
                sz = write(vm->cpu->regs.rbx, p, vm->cpu->regs.rcx);
            }
            if (sz < 0) {
                vm->inthandler_set_cf();
            } else {
                vm->cpu->regs.rax = sz;
            }
        } break;

        case 0x42: {
            size_t off = (vm->cpu->regs.rcx << 16) | (vm->cpu->regs.rdx);
            uint8_t al = vm->cpu->regs.rax & 0xff;
            int whence = 0;
            if (al == 0) {
                whence = SEEK_SET;
            } else if (al == 1) {
                whence = SEEK_CUR;
            } else {
                whence = SEEK_END;
            }
            int r = lseek(vm->cpu->regs.rbx, off, whence);
            if (r < 0) {
                vm->inthandler_set_cf();
            }
        } break;

        case 0x4c: {
            exit(vm->cpu->regs.rax & 0xff);
            break;
        }

        default:
            printf("unknown dos system call ah=0x%x\n", ah);
            dump_regs(vm->cpu.get());
            vm->inthandler_set_cf();
            exit(1);
            break;
    }
    vm->emu_reti();
}

void install_dos_driver(VM *vm) {
    auto dos = vm->floppy->read("MSDOS", "SYS");
    if (!dos) {
        perror("unable to load MSDOS.SYS");
        exit(1);
    }
    auto full_mem = vm->full_mem;
    memcpy(full_mem + vm->addr_config.dos_seg * 16, dos->data(), dos->size());

    *((uint16_t *)&full_mem[0x8000 - 4]) = 0x100 * 2;  // ret from dos init. rip
    *((uint16_t *)&full_mem[0x8000 - 2]) = 0xf000;     // ret from dos init. cs

    auto dos_io_seg = vm->addr_config.dos_io_seg;
    auto drv_param = vm->addr_config.drv_param;
    auto drv_init_tab = vm->addr_config.drv_init_tab;

    full_mem[dos_io_seg * 16 + drv_init_tab] = 1;      // number of drive
    full_mem[dos_io_seg * 16 + drv_init_tab + 1] = 0;  // disk id
    *(uint16_t *)&full_mem[dos_io_seg * 16 + drv_init_tab + 2] = drv_param;

    auto bpb = (struct dos_bpb *)&full_mem[dos_io_seg * 16 + drv_param + 0];
    *bpb = vm->floppy->bpb;
}

namespace {
struct __attribute__((packed)) MZ {
    char sig[2];
    uint16_t extra_bytes;
    uint16_t pages;
    uint16_t reloc_items;
    uint16_t header_size;
    uint16_t minimum_allocation;
    uint16_t maximum_allocation;
    uint16_t initial_ss;
    uint16_t initial_sp;
    uint16_t checksum;
    uint16_t initial_ip;
    uint16_t initial_cs;
    uint16_t reloc_table;
    uint16_t overlay;
};
};  // namespace

int load_mz(VM *vm, const std::string &path, const std::string &argv) {
    const char *cp = path.c_str();
    int fd = open(cp, O_RDONLY);
    if (fd == -1) {
        perror(cp);
        return -1;
    }

    struct stat st;
    fstat(fd, &st);
    size_t sz = st.st_size;

    void *mapped = mmap(0, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

    MZ *mz = (MZ *)mapped;
    if (0) {
        printf("minimum_allocation:%x\n", mz->minimum_allocation);
        printf("maximum_allocation:%x\n", mz->maximum_allocation);

        printf("initial_ip:%x\n", mz->initial_ip);
        printf("initial_cs:%x\n", mz->initial_cs);
        printf("initial_ip:%x\n", mz->initial_sp);
        printf("initial_ss:%x\n", mz->initial_ss);
    }

    uint8_t *ptr = (uint8_t *)mapped;
    uint8_t *load_data = ptr + mz->header_size * 16;
    size_t ldsz = sz - mz->header_size * 16;
    size_t psp_offset = 0x1000;
    size_t psp_seg = 0x100;

    char *psp = (char *)(vm->full_mem + psp_offset);
    memset(vm->full_mem + psp_offset, 0, 256);
    {
        psp[0x00] = 0xcd;
        psp[0x01] = 0x20;
        psp[0x02] = 0xff;
        psp[0x03] = 0x7f;

        psp[0x80] = argv.size();
        strcpy(psp + 0x81, argv.c_str());
        psp[0x81 + argv.size()] = 0x0d;
    }

    size_t exe_seg = 0x110;  // 1100:0000
    uint8_t *dst = vm->full_mem + exe_seg * 16;
    memcpy(dst, load_data, ldsz);
    memset(dst + ldsz, 0, mz->minimum_allocation * 16);
    set_seg(vm->cpu->sregs.ds, psp_seg);
    set_seg(vm->cpu->sregs.es, psp_seg);
    set_seg(vm->cpu->sregs.cs, exe_seg + mz->initial_cs);
    set_seg(vm->cpu->sregs.ss, exe_seg + mz->initial_ss);
    vm->cpu->regs.rip = mz->initial_ip;
    vm->cpu->regs.rsp = mz->initial_sp;

    uint16_t *reloc = (uint16_t *)(ptr + mz->reloc_table);
    for (size_t r = 0; r < mz->reloc_items; r++) {
        uint16_t reloc_offset = reloc[r * 2 + 0];
        uint16_t reloc_seg = reloc[r * 2 + 1];
        uint16_t *reloc_dst = (uint16_t *)(dst + reloc_seg * 16 + reloc_offset);

        // printf("%d: off:%x seg:%x, %x->%x\n", (int)r, reloc[r * 2 + 0],
        //        reloc[r * 2 + 1], (*reloc_dst), (int)((*reloc_dst) +
        //        exe_seg));

        (*reloc_dst) += exe_seg;
    }

    munmap(mapped, sz);
    close(fd);

    return 0;
}
