static inline void outb(unsigned char val, unsigned short port) {
    __asm__ __volatile__("outb %b0,%w1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm__ __volatile__("inb %w1,%b0" : "=a"(val) : "Nd"(port));
    return val;
}

#define LSR 5

void uart_put(unsigned char c) { outb(c, 0x3f8); }
unsigned char uart_get() {
    while (1) {
        unsigned char lsr = inb(0x3f8 + LSR);
        if (lsr & 1) { /* DR */
            return inb(0x3f8);
        }
    }
}

void puts(const char *p) {
    while (*p) {
        uart_put(*p);
        p++;
    }
    uart_put('\r');
    uart_put('\n');
}

int cmain() {
    puts("hello world!");

    while (1) {
        unsigned char c = uart_get();
        uart_put(c);
        if (c == '\r') {
            uart_put('\n');
        }
    }
}
