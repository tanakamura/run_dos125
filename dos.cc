#include "dosdriver.h"
#include "vm.hpp"

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
            break;

        case DOSIO_GETTIME:
            regs.rax = 0;
            break;

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

void install_dos_driver(VM *vm) {
    int dos = open("dos/jwasm/STDDOS.COM", O_RDONLY);
    if (dos < 0) {
        perror("unable to load dos kernel(dos/jwasm/stddos.com)");
        exit(1);
    }
    auto full_mem = vm->full_mem;
    read(dos, full_mem + vm->addr_config.dos_seg * 16, 64 * 1024);
    close(dos);
    vm->run_mode = RUN_MODE::DOS;

    *((uint16_t *)&full_mem[0x8000 - 4]) = 0x100 * 2;  // ret from dos init. rip
    *((uint16_t *)&full_mem[0x8000 - 2]) = 0xf000;     // ret from dos init. cs

    auto dos_io_seg = vm->addr_config.dos_io_seg;
    auto drv_param = vm->addr_config.drv_param;
    auto drv_init_tab = vm->addr_config.drv_init_tab;

    full_mem[dos_io_seg * 16 + drv_init_tab] = 1;      // number of drive
    full_mem[dos_io_seg * 16 + drv_init_tab + 1] = 0;  // disk id
    *(uint16_t *)&full_mem[dos_io_seg * 16 + drv_init_tab + 2] = drv_param;

    // size of sector
    *(uint16_t *)&full_mem[dos_io_seg * 16 + drv_param + 0] = 512;
    // cluster size (sector per cluster)
    full_mem[dos_io_seg * 16 + drv_param + 2] = 2;
    // reserved sectors (mbr)
    *(uint16_t *)&full_mem[dos_io_seg * 16 + drv_param + 3] = 1;
    // fat count
    full_mem[dos_io_seg * 16 + drv_param + 5] = 2;
    // num of root dir entry
    *(uint16_t *)&full_mem[dos_io_seg * 16 + drv_param + 6] = 112;
    // num total sector
    *(uint16_t *)&full_mem[dos_io_seg * 16 + drv_param + 8] = 640;
}
