#include "gdt.h"

struct gdt_entry gdt[5];
struct gdt_ptr gp;

extern void gdt_flush(unsigned int);

static void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_install(void) {
    // Disable interrupts while swapping CPU tables
    __asm__ volatile ("cli");

    gp.limit = (sizeof(struct gdt_entry) * 5) - 1;
    gp.base = (unsigned int)&gdt[0];

    // 0: Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Kernel Code Segment: Base=0, Limit=4GB, Ring 0, Exec/Read
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);

    // 2: Kernel Data Segment: Base=0, Limit=4GB, Ring 0, Read/Write
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);

    // 3: User Code Segment: Base=0, Limit=4GB, Ring 3, Exec/Read
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);

    // 4: User Data Segment: Base=0, Limit=4GB, Ring 3, Read/Write
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);

    // Load new GDT
    gdt_flush((unsigned int)&gp);
}