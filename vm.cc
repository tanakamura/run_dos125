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
#include <sys/stat.h>
#include <unistd.h>

#include <mutex>
#include <queue>
#include <thread>

#if 0
#define NUM_SECTOR 8
#define NUM_HEAD 2
#define NUM_CYL 40
#else
/* 2HD = 18sector, 80cyl, 2head */
#define FLOPPY_TYPE 4
#define NUM_SECTOR 18
#define NUM_HEAD 2
#define NUM_CYL 80
#endif

bool debug = false;
uint16_t dos21_seg = 0;
uint16_t dos21_offset = 0;
std::queue<char> key_queue;
std::mutex q_lock;

struct vcpu {
    char *run;
    int fd;
};

struct mem_slot {
    int used;
};

static void dump_reg1(const char *tag, __u64 val) {
    printf("%10s:%22lld(%16llx)\n", tag, (long long)val, (long long)val);
}

static void dump_seg1(const char *tag, struct kvm_segment *seg) {
    printf(
        "%10s: base=%16llx, limit=%8x, sel=%8x, type=%4d, db=%d, l=%d, g=%d\n",
        tag, (unsigned long long)seg->base, (int)seg->limit, (int)seg->selector,
        (int)seg->type, (int)seg->db, (int)seg->l, (int)seg->g);
}

static void dump_dt(const char *tag, struct kvm_dtable *dt) {
    printf("%10s: base=%16llx, limit=%8x\n", tag, (long long)dt->base,
           (int)dt->limit);
}

static void setup_vcpu(int fd, __u64 entry) {
    struct kvm_sregs sregs;
    struct kvm_regs regs;
    ioctl(fd, KVM_GET_REGS, &regs, NULL);

    regs.rip = entry;
    regs.rsp = 0x8000;

    ioctl(fd, KVM_SET_REGS, &regs, NULL);

    ioctl(fd, KVM_GET_SREGS, &sregs, NULL);

    sregs.cs.base = 0x0;
    sregs.cs.limit = 0xffff;
    sregs.cs.selector = 0x0;
    sregs.ds.base = 0;
    sregs.ds.limit = 0xffff;
    sregs.ds.selector = 0;
    sregs.ss.base = 0;
    sregs.ss.limit = 0xffff;
    sregs.ss.selector = 0;

    sregs.cs.db = 0;
    sregs.ss.db = 0;
    sregs.ds.db = 0;

    sregs.cs.l = 0;
    sregs.ss.l = 0;
    sregs.ds.l = 0;

    ioctl(fd, KVM_SET_SREGS, &sregs, NULL);
}

static void dump_regs(int fd) {
    struct kvm_regs regs;
    ioctl(fd, KVM_GET_REGS, &regs, NULL);

    dump_reg1("rip", regs.rip);
    dump_reg1("rflags", regs.rflags);

    dump_reg1("rax", regs.rax);
    dump_reg1("rbx", regs.rbx);
    dump_reg1("rcx", regs.rcx);
    dump_reg1("rdx", regs.rdx);

    dump_reg1("rsi", regs.rsi);
    dump_reg1("rdi", regs.rdi);
    dump_reg1("rsp", regs.rsp);
    dump_reg1("rbp", regs.rbp);

    dump_reg1("r8", regs.r8);
    dump_reg1("r9", regs.r9);
    dump_reg1("r10", regs.r10);
    dump_reg1("r11", regs.r11);

    dump_reg1("r12", regs.r12);
    dump_reg1("r13", regs.r13);
    dump_reg1("r14", regs.r14);
    dump_reg1("r15", regs.r15);

    struct kvm_sregs sregs;
    ioctl(fd, KVM_GET_SREGS, &sregs, NULL);

    dump_reg1("cr0", sregs.cr0);
    dump_reg1("cr2", sregs.cr2);
    dump_reg1("cr3", sregs.cr3);
    dump_reg1("cr4", sregs.cr4);
    dump_reg1("cr8", sregs.cr8);
    dump_reg1("efer", sregs.efer);
    dump_reg1("apic_base", sregs.apic_base);

    dump_seg1("cs", &sregs.cs);
    dump_seg1("ds", &sregs.ds);
    dump_seg1("es", &sregs.es);
    dump_seg1("fs", &sregs.fs);
    dump_seg1("gs", &sregs.gs);
    dump_seg1("ss", &sregs.ss);
    dump_seg1("tr", &sregs.tr);
    dump_seg1("ldt", &sregs.ldt);

    dump_dt("gdt", &sregs.gdt);
    dump_dt("idt", &sregs.idt);
}

static void dump_ivt(unsigned char *full_mem) {
    for (int i = 0; i < 0x100; i++) {
        printf("ivt [%02x] = offset:%x cs:%x\n", i,
               *(unsigned short *)&full_mem[i * 4 + 0],
               *(unsigned short *)&full_mem[i * 4 + 2]);
    }

    for (int i = 0; i < 0x100; i++) {
        printf("handler [%02x] = %x %x %x %x\n", i, full_mem[0xf0000 + i + 0],
               full_mem[0xf0000 + i + 1], full_mem[0xf0000 + i + 2],
               full_mem[0xf0000 + i + 3]);
    }
}

static void handle_hlt(int fd, unsigned char *full_mem, int image_fd) {
    struct kvm_sregs sregs;
    ioctl(fd, KVM_GET_SREGS, &sregs, NULL);
    struct kvm_regs regs;
    ioctl(fd, KVM_GET_REGS, &regs, NULL);

    if (sregs.cs.base == 0xf0000) {
        uint32_t rip = regs.rip;
        int intr_nr = rip - 1;
        // printf("handle intr = %x\n", intr_nr);
        bool cf = 0;
        bool zf = 0;
        bool dos = false;

        if (intr_nr == 255) {
            puts("exit with intr 0xff");
            exit(0);
        }

        uint16_t prev_ip = *(uint16_t *)(full_mem + (sregs.ss.base + regs.rsp));
        uint16_t prev_cs =
            *(uint16_t *)(full_mem + (sregs.ss.base + regs.rsp + 2));
        uint16_t prev_flags =
            *(uint16_t *)(full_mem + (sregs.ss.base + regs.rsp + 4));

        // printf("intr_nr %x, prev_ip=%x, prev_cs=%x\n", intr_nr, prev_ip,
        //        prev_cs);

        switch (intr_nr) {
            case 0x10: /* video */
                switch ((regs.rax >> 8) & 0xff) {
                    case 0x0e:
                        putchar(regs.rax & 0xff);
                        fflush(stdout);
                        break;
                    case 1:  // set cursor shape
                        break;
                    case 0xf:  // get current video mode
                        cf = 1;
                        break;
                    default:
                        printf("video intr %x\n", (regs.rax) >> 8);
                        break;
                }

                break;

            case 0x11:  // get equipment list
                regs.rax = 0x1;
                break;

            case 0x12:  // get memory size
                regs.rax = 256;
                break;

            case 0x13:
                switch ((regs.rax >> 8) & 0xff) {
                    case 0: {
                        int drive = regs.rdx & 0x7f;
                        printf("init %d\n", drive);
                        if (drive != 0) {
                            cf = 1;
                        }
                        regs.rax = 0;
                        break;  // reset
                    }

                    case 2:
                    case 3: {
                        int num_sector = regs.rax & 0xff;
                        int cyl = (regs.rcx >> 8) |
                                  (((regs.rcx & 0xc0) << 2) & 0x300);
                        int sector = (regs.rcx & 0x3f) - 1;
                        int head = (regs.rdx >> 8);
                        int drive = regs.rdx & 0xff;
                        if (drive != 0) {
                            printf("%d\n", drive);
                            cf = 1;
                            regs.rax = 0x01 << 8;
                        } else {
                            uint32_t buffer = sregs.es.base + regs.rbx;
                            int lba = sector;
                            lba += head * NUM_SECTOR;
                            lba += cyl * NUM_HEAD * NUM_SECTOR;

                            // dump_regs(fd);
                            // printf(
                            //    "lba=%x num_sector=%d cyl=%d sector=%d head=%d
                            //    " "drive=%d %x\n", lba, num_sector, cyl,
                            //    sector, head, drive, buffer);

                            if ((regs.rax >> 8 &0xff) == 2) {
                                pread(image_fd, full_mem + buffer, num_sector * 512,
                                      lba * 512);
                            } else {
                                pwrite(image_fd, full_mem + buffer, num_sector * 512,
                                       lba * 512);
                            }
                            regs.rax = num_sector;
                        }
                        break;
                    }

                    case 8: {
                        int drive = regs.rdx & 0xff;
                        if (drive == 0) {
                            regs.rax = 0;
                            regs.rbx = FLOPPY_TYPE;
                            regs.rcx = (NUM_CYL << 8) | NUM_SECTOR;
                            regs.rdx = (NUM_HEAD << 8) | 1;
                        } else {
                            cf = 1;
                        }
                        break;
                    }

                    case 0x15: {
                        int drive = regs.rdx & 0xff;
                        if (drive != 0) {
                            printf("unknown drive %d\n", drive);
                            cf = 1;
                        } else {
                            int num_total_sector = 80 * 2 * 18;
                            regs.rcx = num_total_sector >> 16;
                            regs.rdx = num_total_sector & 0xffff;
                            regs.rax = 1 << 8;  // floppy without change-line
                        }

                        break;
                    }

                    default:
                        printf("unknown 0x13 %x\n", regs.rax >> 8);
                        dump_regs(fd);
                        cf = 1;
                        regs.rax = 0x100;
                        break;
                }

                break;

            case 0x16:
                switch (regs.rax >> 8) {
                    case 0x00:
                    case 0x10: {
                        std::lock_guard lk(q_lock);
                        if (key_queue.empty()) {
                            zf = 1;
                        } else {
                            zf = 0;
                            char c = key_queue.front();
                            key_queue.pop();
                            if (c == '\n') {
                                regs.rax = '\r';
                            } else {
                                regs.rax = c;
                            }
                        }
                    } break;
                    case 0x01:
                    case 0x11: {
                        std::lock_guard lk(q_lock);
                        if (key_queue.empty()) {
                            zf = 1;
                        } else {
                            zf = 0;
                            char c = key_queue.front();
                            if (c == '\n') {
                                regs.rax = '\r';
                            } else {
                                regs.rax = c;
                            }
                        }
                    } break;
                    default:
                        printf("unknown 0x16 %x", regs.rax >> 8);
                        zf = 1;
                        cf = 1;
                        // dump_regs(fd);
                        // exit(1);
                        break;
                }
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
                cf = 1;
            } break;

            case 0x21: {
                // debug = true;
                dos = true;
                sregs.cs.base = dos21_seg * 16;
                sregs.cs.selector = dos21_seg;
                regs.rip = dos21_offset;
                printf("dos21 %04x:%04x\n", dos21_seg, dos21_offset);
                break;
            }

            default:
                // printf("unknown intr 0x%x, prev_ip=%04x, prev_cs=%04x%x\n",
                // intr_nr, prev_ip, prev_cs);
                //  dump_regs(fd);
                cf = 1;
                break;
        }

        if (!dos) {
            sregs.cs.base = prev_cs * 16;
            sregs.cs.selector = prev_cs;
            regs.rflags &= ~((1 << 6) | (1 << 0));  // clear zf, cf
            if (zf) {
                regs.rflags |= (1 << 6);
            }
            if (cf) {
                regs.rflags |= (1 << 0);
            }

            regs.rip = prev_ip;
            regs.rsp += 6;
        }

        ioctl(fd, KVM_SET_SREGS, &sregs, NULL);
        ioctl(fd, KVM_SET_REGS, &regs, NULL);

        return;
    } else {
        dump_ivt(full_mem);

        printf("unknown hlt, cs=0x%04x\n", sregs.cs.base);
        dump_regs(fd);
        exit(1);
    }
}

static void capability_test(int fd, int cap, const char *tag) {
    int r = ioctl(fd, KVM_CHECK_EXTENSION, (void *)(uintptr_t)cap);
    if (!r) {
        fprintf(stderr, "kvm does not support %s\n", tag);
        exit(1);
    }
}

void sigint(int signum) {}

static void disasm(int fd, unsigned char *full_mem) {
    struct kvm_sregs sregs;
    ioctl(fd, KVM_GET_SREGS, &sregs, NULL);
    struct kvm_regs regs;
    ioctl(fd, KVM_GET_REGS, &regs, NULL);

    printf("rip=%x, cs=%x, ds=%x ss=%x flags=%08x\n", regs.rip, sregs.cs.base,
           sregs.ds.base, sregs.ss.base, regs.rflags);

    unsigned char *code = &full_mem[regs.rip + sregs.cs.base];
    if (code[0] == 0xf3 && code[1] == 0xa4) {
        puts("rep movsb");
    } else if (code[0] == 0xf3 && code[1] == 0xa5) {
        puts("rep movsw");
    } else if (code[0] == 0xf3 && code[1] == 0xab) {
        puts("rep stosw");
    } else {
        FILE *fp = fopen("out.bin", "wb");
        fwrite(&full_mem[regs.rip + sregs.cs.base], 1, 16, fp);
        fclose(fp);

        // int r = system("ndisasm out.bin|head -n2");
        // if (r != 0) {
        //     exit(1);
        // }
    }
    fflush(stdout);
}

int main(int argc, char **argv) {
    int kvm_fd = open("/dev/kvm", O_RDWR);
    if (kvm_fd < 0) {
        perror("/dev/kvm");
        return 1;
    }

    unsigned char *full_mem;

    full_mem =
        (unsigned char *)mmap(0, 1024 * 1024 * 32, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (full_mem == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    // for (int i = 0; i < 1024 * 1024 / 4; i++) {
    //     ((unsigned int *)full_mem)[i] = i;
    // }
    memset(full_mem, 0xf4, 1024 * 1024);
    memset(full_mem + 0x400, 0x88, 256);  // bda
    int num_int = 256;

    for (size_t intr = 0; intr < num_int; intr++) {
        *(unsigned short *)(full_mem + intr * 4 + 0) = intr;    // offset
        *(unsigned short *)(full_mem + intr * 4 + 2) = 0xf000;  // seg

        full_mem[0xf0000 + intr] = 0xf4;  // hlt
    }

    // int image_fd = open("FD13BOOT.img", O_RDONLY);
    //  int image_fd = open("dos11src.img", O_RDONLY);
    int image_fd = open("floppy", O_RDWR);
    if (image_fd < 0) {
        perror("unable to open a");
        return 1;
    }
    read(image_fd, full_mem + 0x7c00, 64 * 1024);

    capability_test(kvm_fd, KVM_CAP_USER_MEMORY, "KVM_CAP_USER_MEMORY");
    int nr_memslots =
        ioctl(kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_NR_MEMSLOTS, NULL);
    if (nr_memslots < 0) {
        perror("KVM_CAP_NR_MEMSLOTS");
        return 1;
    }
    if (nr_memslots < 3) {
        fprintf(stderr, "too few mem slots (%d)\n", nr_memslots);
        return 1;
    }

    int vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, NULL);
    int vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, (void *)0, NULL);
    size_t vcpu_region_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, NULL);
    struct kvm_run *run = (struct kvm_run *)mmap(
        0, vcpu_region_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, vcpu_fd, 0);

    ioctl(vcpu_fd, KVM_RUN, NULL);

    struct kvm_userspace_memory_region mem = {0};
    mem.slot = 0;
    mem.flags = 0;
    mem.guest_phys_addr = 0;
    mem.memory_size = (1024 - 64) * 1024;
    mem.userspace_addr = (__u64)full_mem;
    int r = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &mem, NULL);

    mem.slot = 1;
    mem.flags = KVM_MEM_READONLY;
    mem.guest_phys_addr = 0xf0000;
    mem.memory_size = 64 * 1024;
    mem.userspace_addr = (__u64)(full_mem + 0xf0000);
    r = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &mem, NULL);
    if (r < 0) {
        perror("map");
        exit(1);
    }

    setup_vcpu(vcpu_fd, 0x7c00);
    // signal(SIGINT, sigint);

    bool done = false;
    struct kvm_guest_debug single_step = {};
    single_step.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP;
    // ioctl(vcpu_fd, KVM_SET_GUEST_DEBUG, &single_step, NULL);

    std::thread tK([&q_lock, &key_queue]() {
        while (1) {
            int c = getchar();
            if (c == EOF) {
                break;
            }

            std::lock_guard lk(q_lock);
            key_queue.push(c);
        }
    });

    while (!done) {
        if (debug) {
            ioctl(vcpu_fd, KVM_SET_GUEST_DEBUG, &single_step, NULL);
        }
        ioctl(vcpu_fd, KVM_RUN, NULL);

        switch (run->exit_reason) {
            case KVM_EXIT_INTERNAL_ERROR:
                printf("exit internal error : %d [",
                       (int)run->internal.suberror);
                for (int ei = 0; ei < run->internal.ndata; ei++) {
                    printf("%llx, ", (long long)run->internal.data[ei]);
                }
                printf("]\n");
                dump_regs(vcpu_fd);
                dump_ivt(full_mem);
                done = 1;
                break;

            case KVM_EXIT_MMIO:
                printf("reference unmapped region : addr=%16llx\n",
                       (long long)run->mmio.phys_addr);
                dump_regs(vcpu_fd);
                disasm(vcpu_fd, full_mem);
                dump_ivt(full_mem);
                done = 1;
                break;

            case KVM_EXIT_SHUTDOWN:
                printf("kvm exit shutdown\n");
                dump_regs(vcpu_fd);
                done = 1;
                break;

            case KVM_EXIT_HLT:
                // disasm(vcpu_fd, full_mem);
                handle_hlt(vcpu_fd, full_mem, image_fd);
                break;

            case KVM_EXIT_FAIL_ENTRY:
                printf(
                    "fail entry : reason=%llx\n",
                    (long long)run->fail_entry.hardware_entry_failure_reason);
                dump_regs(vcpu_fd);
                done = 1;
                break;

            case KVM_EXIT_DEBUG: {
                struct kvm_regs regs;
                ioctl(vcpu_fd, KVM_GET_REGS, &regs, NULL);
                struct kvm_sregs sregs;
                ioctl(vcpu_fd, KVM_GET_SREGS, &sregs, NULL);
                if (1) {
                    disasm(vcpu_fd, full_mem);
                    if (regs.rip == 0xa11f) {
                        dump_regs(vcpu_fd);
                    }
                    if (full_mem[regs.rip + sregs.cs.base] == 0xcd &&
                        full_mem[regs.rip + sregs.cs.base + 1] == 0x21) {
                        dump_regs(vcpu_fd);
                        dump_ivt(full_mem);
                    }
                }
            } break;

            case KVM_EXIT_IO:
                printf("%x %x %x\n", run->io.direction, run->io.port,
                       run->io.size);
                dump_regs(vcpu_fd);
                dump_ivt(full_mem);
                disasm(vcpu_fd, full_mem);
                done = 1;
                break;

            case KVM_EXIT_INTR: {
                struct kvm_regs regs;
                ioctl(vcpu_fd, KVM_GET_REGS, &regs, NULL);
                struct kvm_sregs sregs;
                ioctl(vcpu_fd, KVM_GET_SREGS, &sregs, NULL);
                disasm(vcpu_fd, full_mem);
                dump_regs(vcpu_fd);
                exit(1);
            }

            default:
                printf("unknown exit %d\n", run->exit_reason);
                dump_regs(vcpu_fd);
                done = 1;
                break;
        }
    }

    return 0;
}
