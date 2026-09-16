#include "keyboard.h"
#include "irq.h"
#include "io.h"
#include "ui.h"

#define KEYBOARD_DATA_PORT 0x60

static const char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',   0, ' '
};

static void keyboard_callback(struct regs* r) {
    (void)r;
    unsigned char scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode & 0x80) {
        return;
    }

    if (scancode < 128) {
        char c = kbd_us[scancode];
        if (c != 0) {
            ui_handle_key(c);
        }
    }
}

void keyboard_install(void) {
    inb(KEYBOARD_DATA_PORT); // Discard any pending byte
    irq_install_handler(1, keyboard_callback);
}