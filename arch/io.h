#ifndef IO_H
#define IO_H

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    // Port 0x80 is unused during runtime; writing to it adds a ~1-4 microsecond bus delay
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif