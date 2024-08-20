#include "vm.hpp"

void disasm(const VM *vm) {
    auto &sregs = vm->cpu->sregs;
    auto &regs = vm->cpu->regs;
    auto full_mem = vm->full_mem;

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

        fflush(stdout);
        int r = system("ndisasm out.bin|head -n2");
        if (r != 0) {
            exit(1);
        }
        fflush(stdout);
    }

    printf("rip=%x, cs=%x, ds=%x ss=%x flags=%08x\n", (int)regs.rip,
           (int)sregs.cs.base, (int)sregs.ds.base, (int)sregs.ss.base, (int)regs.rflags);

    fflush(stdout);
}
