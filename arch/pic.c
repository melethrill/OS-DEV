#include "pic.h"
#include "io.h"

#define ICW1_INIT 0x11
#define ICW4_8086 0x01

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_remap(int offset1, int offset2) {
    // Save existing masks
    unsigned char a1 = inb(PIC1_DATA);
    unsigned char a2 = inb(PIC2_DATA);
    (void)a1;
    (void)a2;

    // ICW1: Init sequence
    outb(PIC1_COMMAND, ICW1_INIT);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT);
    io_wait();

    // ICW2: Vector offsets
    outb(PIC1_DATA, (unsigned char)offset1);
    io_wait();
    outb(PIC2_DATA, (unsigned char)offset2);
    io_wait();

    // ICW3: Cascade wiring
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    // ICW4: 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Master PIC: Unmask IRQ 0, 1, 2 (11111000b = 0xF8)
    outb(PIC1_DATA, 0xF8);
    io_wait();

    // Slave PIC: Unmask IRQ 12 (11101111b = 0xEF)
    outb(PIC2_DATA, 0xEF);
    io_wait();
}