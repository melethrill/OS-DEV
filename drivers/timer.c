#include "timer.h"
#include "irq.h"
#include "io.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40
#define PIT_BASE_FREQUENCY 1193182

static volatile unsigned int timer_ticks = 0;

static void timer_callback(struct regs* r) {
    (void)r;
    timer_ticks++;
}

unsigned int timer_get_ticks(void) {
    return timer_ticks;
}

void timer_install(unsigned int frequency_hz) {
    irq_install_handler(0, timer_callback);

    unsigned int divisor = PIT_BASE_FREQUENCY / frequency_hz;

    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (unsigned char)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (unsigned char)((divisor >> 8) & 0xFF));
}