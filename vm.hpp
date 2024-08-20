
#pragma once

#include <fcntl.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <memory>

#include "floppy.hpp"
#include "x86.hpp"

struct AddrConfig {
    uintptr_t dos_io_seg = 0x60;
    uintptr_t drv_init_tab = 16 * 3;
    uintptr_t drv_param = 16 * 3 + 8;
    uintptr_t dos_io_size = 128;
    uintptr_t dos_seg = dos_io_seg + dos_io_seg / 16;
};

static constexpr int INVOKE_SYSTEM_RET_ADDR = 0x200;
enum class ExitCode {
    HLT_BIOS_CALL,
    HLT_DOS_DRIVER,
    HLT_INVOKE_RETURN,  // from f000:0200
    SINGLE_STEP,
};
struct ExitReason {
    ExitCode code;
    int bios_nr;
    int dos_driver_call;
};
enum class RUN_MODE {
    DOS,                        // install dos kernel & start dos
    MBR,                        // 0000:7c00
    NATIVE                      // FFFFFFF0
};

struct CPU {
    int vcpu_fd;
    struct kvm_sregs sregs;
    struct kvm_regs regs;
    struct kvm_run *run_data;

    CPU(int kvm_fd, int vm_fd) {
        vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, (void *)0);
        if (vcpu_fd < 0) {
            perror("vcpu create");
            exit(1);
        }
        load_regs_from_vm();

        size_t vcpu_region_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, NULL);

        run_data =
            (struct kvm_run *)mmap(0, vcpu_region_size, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE, vcpu_fd, 0);
    }

    void load_regs_from_vm() {
        ioctl(vcpu_fd, KVM_GET_SREGS, &sregs);
        ioctl(vcpu_fd, KVM_GET_REGS, &regs);
    }
    void restore_regs_to_vm() {
        ioctl(vcpu_fd, KVM_SET_SREGS, &sregs);
        ioctl(vcpu_fd, KVM_SET_REGS, &regs);
    }

    ~CPU() { close(vcpu_fd); }

    void setup(const AddrConfig &config, RUN_MODE mode);

    ExitReason run(const AddrConfig &config, bool single_step);
};

inline void set_seg(struct kvm_segment &sreg, uintptr_t sel_val) {
    sreg.base = sel_val * 16;
    sreg.limit = 0xffff;
    sreg.selector = sel_val;
    sreg.db = 0;
    sreg.l = 0;
}


struct VM {
    int kvm_fd;
    int vm_fd;
    unsigned char *full_mem;
    std::unique_ptr<CPU> cpu;
    AddrConfig addr_config;
    std::unique_ptr<Floppy> floppy;

    RUN_MODE run_mode = RUN_MODE::MBR;

    VM() {
        kvm_fd = open("/dev/kvm", O_RDWR);
        if (kvm_fd < 0) {
            perror("/dev/kvm");
            exit(1);
        }

        full_mem = (unsigned char *)mmap(0, 1024 * 1024, PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
        if (full_mem == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
        memset(full_mem, 0xf4, 1024 * 1024);  // fill by hlt(0xf4)

        vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, NULL);

        struct kvm_userspace_memory_region mem = {0};
        mem.slot = 0;
        mem.flags = 0;
        mem.guest_phys_addr = 0;
        mem.memory_size = 1024 * 1024;
        mem.userspace_addr = (__u64)full_mem;
        int r = ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &mem, NULL);

        cpu = std::make_unique<CPU>(kvm_fd, vm_fd);
    }

    ~VM() {
        close(vm_fd);
        close(kvm_fd);
        munmap(full_mem, 1024 * 1024);
    }

    void inthandler_clear_cf() {
        uint16_t &stack_flags =
            *(uint16_t *)(full_mem + (cpu->sregs.ss.base + cpu->regs.rsp + 4));
        stack_flags &= ~FLAGS_CF;
    }
    void inthandler_set_cf() {
        uint16_t &stack_flags =
            *(uint16_t *)(full_mem + (cpu->sregs.ss.base + cpu->regs.rsp + 4));
        stack_flags |= FLAGS_CF;
    }
    void inthandler_clear_zf() {
        uint16_t &stack_flags =
            *(uint16_t *)(full_mem + (cpu->sregs.ss.base + cpu->regs.rsp + 4));
        stack_flags &= ~FLAGS_ZF;
    }
    void inthandler_set_zf() {
        uint16_t &stack_flags =
            *(uint16_t *)(full_mem + (cpu->sregs.ss.base + cpu->regs.rsp + 4));
        stack_flags |= FLAGS_ZF;
    }

    void emu_reti();
    void emu_push16(uint16_t val);
    void emu_far_ret();
    void emu_far_call(uintptr_t cs, uintptr_t ip);

    void set_floppy(const std::string &image_path);
};

void setup_vmbios(VM *vm);

void dump_regs(const CPU *cpu);
void dump_ivt(const VM *vm);

void handle_bios_call(VM *vm, const ExitReason *r);
void handle_dos_driver_call(VM *vm, const ExitReason *r);
void install_dos_driver(VM *vm);
void disasm(const VM *vm);
void invoke_intr(VM *vm, int intr_nr);
void run_with_handler(VM *vm);
ExitReason run(VM *vm, bool single_step);
