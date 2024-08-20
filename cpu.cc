#include "vm.hpp"

void CPU::setup(const AddrConfig &config, bool dos) {
    struct kvm_sregs &sregs = this->sregs;
    struct kvm_regs &regs = this->regs;

    if (dos) {
        regs.rip = 0;
        regs.rsp = 0x8000 - 4;
        regs.rdx = 1024 * 256 / 64;      // number of 64byte paragraphs of mem
        regs.rsi = config.drv_init_tab;  // inittab
    } else {
        regs.rip = 0x7c00;
        regs.rsp = 0x8000;
    }

    if (dos) {
        /* start from dos_seg:0000 */

        set_seg(sregs.cs, config.dos_seg);
        set_seg(sregs.ss, 0);
        set_seg(sregs.es, config.dos_seg);
        set_seg(sregs.ds, config.dos_io_seg);
    } else {
        /* start from 0000:7c00 */

        set_seg(sregs.cs, 0);
        set_seg(sregs.ds, 0);
        set_seg(sregs.ss, 0);
    }
}

static void handle_exit_hlt(const AddrConfig &config, CPU *cpu,
                            ExitReason *ret) {
    auto &regs = cpu->regs;
    auto &sregs = cpu->sregs;
    uint32_t rip = regs.rip;

    if (sregs.cs.base == 0xf0000) {
        int intr_nr = rip - 1;
        if (intr_nr == INVOKE_SYSTEM_RET_ADDR) {
            ret->code = ExitCode::HLT_INVOKE_RETURN;
        } else {
            ret->code = ExitCode::HLT_BIOS_CALL;
            ret->bios_nr = intr_nr;
        }
    } else if (sregs.cs.base == config.dos_io_seg) {
        ret->code = ExitCode::HLT_DOS_DRIVER;
        ret->dos_driver_call = (regs.rip - 1) / 3;
    } else {
        fprintf(stderr, "unknown hlt %04x:%04x\n", (int)sregs.cs.base, (int)regs.rip);
        dump_regs(cpu);
        exit(1);
    }
}

ExitReason CPU::run(const AddrConfig &config, bool single_step) {
    restore_regs_to_vm();

    if (single_step) {
        struct kvm_guest_debug single_step = {};
        single_step.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP;
        ioctl(vcpu_fd, KVM_SET_GUEST_DEBUG, &single_step);
    }
    ioctl(vcpu_fd, KVM_RUN);

    load_regs_from_vm();
    ExitReason ret;

    switch (run_data->exit_reason) {
        case KVM_EXIT_HLT:
            handle_exit_hlt(config, this, &ret);
            break;

        case KVM_EXIT_INTERNAL_ERROR:
            printf("exit internal error : %d [", (int)run_data->internal.suberror);
            exit(1);
            break;
        case KVM_EXIT_MMIO:
            printf("reference unmapped region : addr=%16llx\n",
                   (long long)run_data->mmio.phys_addr);
            exit(1);
            break;
        case KVM_EXIT_SHUTDOWN:
            printf("kvm exit shutdown\n");
            exit(1);
            break;
        case KVM_EXIT_FAIL_ENTRY:
            printf("fail entry : reason=%llx\n",
                   (long long)run_data->fail_entry.hardware_entry_failure_reason);
            exit(1);
            break;
        case KVM_EXIT_DEBUG:
            ret.code = ExitCode::SINGLE_STEP;
            break;
        case KVM_EXIT_IO:
            printf("reference unconnected io %x %x %x\n", run_data->io.direction,
                   run_data->io.port, run_data->io.size);
            exit(1);
            break;
        default:
            printf("unknown exit %d\n", run_data->exit_reason);
            exit(1);
            break;
    }

    return ret;
}

void run_with_handler(VM *vm) {
    while (1) {
        auto r = vm->cpu->run(vm->addr_config, false);
        switch (r.code) {
            case ExitCode::HLT_BIOS_CALL:
                handle_bios_call(vm, &r);
                break;
            case ExitCode::HLT_DOS_DRIVER:
                handle_dos_driver_call(vm, &r);
                break;
            case ExitCode::HLT_INVOKE_RETURN:
                return;
            case ExitCode::SINGLE_STEP:
                fprintf(stderr, "TODO handle single step\n");
                exit(1);
                break;
        }
    }
}

void invoke_intr(VM *vm, int intr_nr) {
    vm->emu_push16((uint16_t)vm->cpu->regs.rflags);  // flags
    vm->emu_push16(0xf000);                          // bios cs
    vm->emu_push16(0x200);                           // ret intr

    uintptr_t ip = *(uint16_t*)(vm->full_mem + intr_nr * 4 + 0);
    uintptr_t cs = *(uint16_t*)(vm->full_mem + intr_nr * 4 + 2);

    vm->cpu->regs.rflags &= ~FLAGS_IF;
    set_seg(vm->cpu->sregs.cs, cs);
    vm->cpu->regs.rip = ip;

    run_with_handler(vm);
}
