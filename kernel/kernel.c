#include "fb.h"
#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "timer.h"
#include "keyboard.h"
#include "mouse.h"
#include "ui.h"

void kernel_main(unsigned int magic, multiboot_info_t* mb_info) {
    (void)magic;

    // 1. Initialize CPU Interrupt Gates
    idt_install();
    isrs_install();
    irq_install();

    // 2. Hardware Peripheral Drivers
    timer_install(100);
    keyboard_install();
    mouse_install();

    // 3. Switch hardware into Graphics Mode
    fb_init(mb_info);

    // 4. Enable CPU Hardware Interrupts
    __asm__ volatile ("sti");

    // 5. Draw Graphical Workspace
    ui_init();

    unsigned char last_left = 0;
    unsigned int last_ticks = 0;
    int last_mx = -1, last_my = -1;

    while (1) {
        mouse_state_t ms = mouse_get_state();

        if (ms.left_button && !last_left) {
            ui_handle_mouse_down(ms.x, ms.y);
        } else if (!ms.left_button && last_left) {
            ui_handle_mouse_up(ms.x, ms.y);
        } else if (ms.x != last_mx || ms.y != last_my) {
            ui_handle_mouse_move(ms.x, ms.y);
        }
        last_left = ms.left_button;
        last_mx = ms.x;
        last_my = ms.y;

        unsigned int cur_ticks = timer_get_ticks();
        if (cur_ticks != last_ticks) {
            ui_update_telemetry(cur_ticks);
            last_ticks = cur_ticks;
        }

        __asm__ volatile ("hlt");
    }
}