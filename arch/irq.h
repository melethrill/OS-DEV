#ifndef IRQ_H
#define IRQ_H

#include "isr.h"

typedef void (*irq_handler_t)(struct regs*);

void irq_install(void);
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);
void irq_handler(struct regs* r);

#endif