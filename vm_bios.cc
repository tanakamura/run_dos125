#include "dosdriver.h"
#include "vm.hpp"

void setup_vmbios(VM *vm) {
    int num_int = 256;
    auto full_mem = vm->full_mem;

    for (size_t intr = 0; intr < num_int; intr++) {
        *(unsigned short *)(full_mem + intr * 4 + 0) = intr;    // offset
        *(unsigned short *)(full_mem + intr * 4 + 2) = 0xf000;  // seg

        full_mem[0xf0000 + intr] = 0xf4;  // hlt
    }
}

void handle_10h(VM *vm) {
    auto regs = vm->cpu->regs;

    vm->inthandler_clear_cf();

    switch ((regs.rax >> 8) & 0xff) {
        case 0x0e:
            putchar(regs.rax & 0xff);
            fflush(stdout);
            break;
        case 1:  // set cursor shape
            break;
        case 0xf:  // get current video mode
            vm->inthandler_set_cf();
            break;
        default:
            printf("video intr %x\n", (regs.rax) >> 8);
            break;
    }
}

void handle_13h(VM *vm) {
    auto &regs = vm->cpu->regs;
    auto &sregs = vm->cpu->sregs;

    switch ((regs.rax >> 8) & 0xff) {
        case 0: {
            int drive = regs.rdx & 0x7f;
            if (drive != 0) {
                vm->inthandler_set_cf();
            }
            regs.rax = 0;
            break;  // reset
        }

        case 2:
        case 3: {
            int num_sector = regs.rax & 0xff;
            int cyl = (regs.rcx >> 8) | (((regs.rcx & 0xc0) << 2) & 0x300);
            int sector = (regs.rcx & 0x3f) - 1;
            int head = (regs.rdx >> 8);
            int drive = regs.rdx & 0xff;
            if (drive != 0) {
                printf("%d\n", drive);
                vm->inthandler_set_cf();
                regs.rax = 0x01 << 8;
            } else {
                uint32_t buffer = sregs.es.base + regs.rbx;
                int lba = sector;
                lba += head * vm->floppy->num_sector;
                lba += cyl * vm->floppy->num_head * vm->floppy->num_sector;

                // dump_regs(fd);
                // printf(
                //    "lba=%x (byte=%x) addr=%x, num_sector=%d "
                //    "cyl=%d sector=%d "
                //    "head=%d  drive = %d %x\n",
                //    lba, lba * 512, buffer, num_sector, cyl, sector,
                //    head, drive, buffer);

                if ((regs.rax >> 8 & 0xff) == 2) {
                    pread(vm->floppy->image_fd, vm->full_mem + buffer,
                          num_sector * 512, lba * 512);
                } else {
                    pwrite(vm->floppy->image_fd, vm->full_mem + buffer,
                           num_sector * 512, lba * 512);
                }
                regs.rax = num_sector;
            }
            break;
        }

        case 8: {
            int drive = regs.rdx & 0xff;
            if (drive == 0) {
                regs.rax = 0;
                regs.rbx = vm->floppy->type;
                regs.rcx =
                    (vm->floppy->num_cylinder << 8) | vm->floppy->num_sector;
                regs.rdx = (vm->floppy->num_head << 8) | 1;
            } else {
                vm->inthandler_set_cf();
            }
            break;
        }

        case 0x15: {
            int drive = regs.rdx & 0xff;
            if (drive != 0) {
                printf("unknown drive %d\n", drive);
                vm->inthandler_set_cf();
            } else {
                int num_total_sector = vm->floppy->num_sector *
                                       vm->floppy->num_head *
                                       vm->floppy->num_cylinder;
                regs.rcx = num_total_sector >> 16;
                regs.rdx = num_total_sector & 0xffff;
                regs.rax = 1 << 8;  // floppy without change-line
            }

            break;
        }

        default:
            printf("unknown 0x13 %x\n", (int)(regs.rax >> 8));
            vm->inthandler_set_cf();
            regs.rax = 0x100;
            break;
    }
}

void handle_16h(VM *vm) {
    auto &regs = vm->cpu->regs;
    switch (regs.rax >> 8) {
        case 0x00:
        case 0x10: {
            char c;
            read(0, &c, 1);
            if (c == '\n') {
                c = '\r';
            }
            regs.rax = c;
        } break;
        case 0x01:
        case 0x11: {
            fd_set rfds;
            struct timeval tv = {0};
            FD_ZERO(&rfds);

            FD_SET(0, &rfds);  // stdin

            int r = select(1, &rfds, nullptr, nullptr, &tv);
            if (r < 0) {
                perror("select");
                exit(1);
            }

            if (FD_ISSET(0, &rfds)) {
                vm->inthandler_clear_zf();
            } else {
                vm->inthandler_set_zf();
            }

            regs.rax = '?';
        } break;
        default:
            printf("unknown 0x16 %x", (int)(regs.rax >> 8));
            vm->inthandler_set_zf();
            vm->inthandler_set_cf();
            break;
    }
}

void handle_bios_call(VM *vm, const ExitReason *r) {
    auto &regs = vm->cpu->regs;
    auto &sregs = vm->cpu->sregs;
    auto full_mem = vm->full_mem;

    uint32_t rip = regs.rip;
    int intr_nr = rip - 1;

    vm->inthandler_clear_cf();

    switch (r->bios_nr) {
        case 0x10: /* video */
            handle_10h(vm);
            break;

        case 0x11:  // get equipment list
            regs.rax = 0x1;
            break;

        case 0x12:  // get memory size
            regs.rax = 256;
            break;

        case 0x13:  // disk
            handle_13h(vm);
            break;

        case 0x16:  // kbd
            handle_16h(vm);
            break;
            //
            //            case 0x1a:
            //                switch (regs.rax >> 8) {
            //                    case 0:
            //                        rflags2 = 0;
            //                        update_rflags = true;
            //                        break;
            //                    case 1:
            //                    case 3:
            //                    case 5:
            //                        break;
            //
            //                    case 0x02:
            //                        rflags2 = 0;
            //                        update_rflags = true;
            //
            //                        regs.rcx = (1 << 8) | (1);
            //                        regs.rdx = (1 << 8) | (1);
            //
            //                        break;
            //                    case 0x04:
            //                        break;
            //                    default:
            //                        printf("unknown 0x1a");
            //                        dump_regs(fd);
            //                        exit(1);
            //                }
            //                break;

            //        case 0x14:{
            //            dump_regs(fd);
            //        }
            //            break;

        case 0x17: {
            vm->inthandler_set_cf();
        } break;

            // for dos override
            //        case 0x21: {
            //            // debug = true;
            //            dont_return = true;
            //            sregs.cs.base = dos21_seg * 16;
            //            sregs.cs.selector = dos21_seg;
            //            regs.rip = dos21_offset;
            //            printf("dos21 %04x:%04x\n", dos21_seg, dos21_offset);
            //            break;
            //        }

            //        case 0x200: {
            //            {
            //                FILE *command = fopen("dos/jwasm/COMMAND.COM",
            //                "rb"); if (command == nullptr) {
            //                    perror("command.com");
            //                    exit(1);
            //                }
            //                int addr = sregs.ds.base;
            //                memset(full_mem + addr + 0x100, 0xff, 64 * 1024);
            //                fread(full_mem + addr + 0x100, 1, 64 * 1024,
            //                command); fclose(command);
            //            }
            //
            //            sregs.es.base = sregs.ds.base;
            //            sregs.es.selector = sregs.ds.selector;
            //            sregs.ss.base = sregs.ds.base;
            //            sregs.ss.selector = sregs.ds.selector;
            //            sregs.cs.base = sregs.ds.base;
            //            sregs.cs.selector = sregs.ds.selector;
            //
            //            regs.rsp = 0x58;
            //            *(uint16_t *)&full_mem[sregs.ss.base + regs.rsp] = 0;
            //
            //            /* start command.com */
            //            regs.rip = 0x100;
            //            dont_return = true;
            //            // debug = true;
            //        } break;

        default:
            vm->inthandler_set_cf();
            break;
    }

    vm->emu_reti();
}

void handle_dos_driver_call(VM *vm) {
    auto &regs = vm->cpu->regs;
    auto &sregs = vm->cpu->sregs;
    auto full_mem = vm->full_mem;

    bool zf = 0;
    bool cf = 0;

    /* dos dirver */
    int dos_driver_call = (regs.rip - 1) / 3;
    // printf("%x\n", dos_driver_call);

    switch (dos_driver_call) {
        case DOSIO_STATUS:
            zf = 1;
            break;
        case DOSIO_INP:
            {
                int a = getchar();
                if (a == '\n') {
                    a = '\r';
                }
                regs.rax = a;
                zf = 0;
            }
            break;
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
            printf("unknown dos driver call %02x\n", dos_driver_call);
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
