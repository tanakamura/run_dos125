#include "vm.hpp"

void VM::emu_reti() {
    auto &regs = cpu->regs;
    auto &sregs = cpu->sregs;
    uint16_t prev_ip = *(uint16_t *)(full_mem + (sregs.ss.base + regs.rsp));
    uint16_t prev_cs = *(uint16_t *)(full_mem + (sregs.ss.base + regs.rsp + 2));
    uint16_t prev_flags =
        *(uint16_t *)(full_mem + (sregs.ss.base + regs.rsp + 4));

    sregs.cs.base = prev_cs * 16;
    sregs.cs.selector = prev_cs;
    regs.rflags = prev_flags;
    regs.rip = prev_ip;
    regs.rsp += 6;
}

void VM::emu_far_ret() {
    auto &regs = cpu->regs;
    auto &sregs = cpu->sregs;

    uint16_t prev_ip = *(uint16_t *)(full_mem + (sregs.ss.base + regs.rsp));
    uint16_t prev_cs =
        *(uint16_t *)(full_mem + (sregs.ss.base + regs.rsp + 2));

    sregs.cs.base = prev_cs * 16;
    sregs.cs.selector = prev_cs;

    regs.rip = prev_ip;
    regs.rsp += 4;

}

void VM::emu_push16(uint16_t val) {
    auto &regs = cpu->regs;
    auto &sregs = cpu->sregs;

    regs.rsp -= 2;
    auto dest = (uint16_t *)(full_mem + sregs.ss.base + regs.rsp);

    *dest = val;
}

void VM::set_floppy(const std::string &image_path) {
  this->floppy = std::make_unique<Floppy>(image_path);
}
