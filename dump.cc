#include "vm.hpp"

void dump_reg1(const char *tag, __u64 val) {
    printf("%10s:%22lld(%16llx)\n", tag, (long long)val, (long long)val);
}

void dump_seg1(const char *tag, const struct kvm_segment *seg) {
    printf(
        "%10s: base=%16llx, limit=%8x, sel=%8x, type=%4d, db=%d, l=%d, "
        "g=%d\n",
        tag, (unsigned long long)seg->base, (int)seg->limit, (int)seg->selector,
        (int)seg->type, (int)seg->db, (int)seg->l, (int)seg->g);
}

void dump_dt(const char *tag, const struct kvm_dtable *dt) {
    printf("%10s: base=%16llx, limit=%8x\n", tag, (long long)dt->base,
           (int)dt->limit);
}

void dump_regs(const CPU *cpu) {
    auto &regs = cpu->regs;
    auto &sregs = cpu->sregs;
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


void dump_ivt(VM *vm) {
    auto full_mem = vm->full_mem;

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

