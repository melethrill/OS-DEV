#ifndef TIMER_H
#define TIMER_H

#include "isr.h"

void timer_install(unsigned int frequency_hz);
unsigned int timer_get_ticks(void);

#endif