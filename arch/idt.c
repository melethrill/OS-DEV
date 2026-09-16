#include "idt.h"

struct idt_entry idt_table[256];
struct idt_ptr idtp;

void idt_set_gate(int num, unsigned int base, unsigned short sel, unsigned char flags) {
    if (num < 0 || num >= 256) return;
    
    idt_table[num].base_low = (unsigned short)(base & 0xFFFF);
    idt_table[num].base_high = (unsigned short)((base >> 16) & 0xFFFF);
    idt_table[num].sel = sel;
    idt_table[num].zero = 0;
    idt_table[num].flags = flags;
}

void idt_install(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt_table;

    __asm__ volatile ("lidt (%0)" : : "r" (&idtp));
}