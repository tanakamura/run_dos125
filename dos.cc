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
                pread(vm->floppy->image_fd, addr, regs.rcx * 512, regs.rdx * 512);
                if (0) {
                    printf(
                        "disk read addr=0x%08x, "
                        "sector=0x%08x(byte=0x%08x), "
                        "count=%x\n",
                           (int)(uintptr_t)(sregs.ds.base + regs.rbx), (int)regs.rdx,
                           (int)regs.rdx * 512, (int)regs.rcx * 512);
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
                pwrite(vm->floppy->image_fd, addr, regs.rcx * 512, regs.rdx * 512);
                if (0) {
                    printf(
                        "disk read addr=0x%08x, "
                        "sector=0x%08x(byte=0x%08x), "
                        "count=%x\n",
                           (int)(uintptr_t)(sregs.ds.base + regs.rbx), (int)regs.rdx,
                           (int)regs.rdx * 512, (int)regs.rcx * 512);
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
