#include "mouse.h"
#include "irq.h"
#include "io.h"
#include "ui.h"

#define MOUSE_DATA_PORT    0x60
#define MOUSE_STATUS_PORT  0x64
#define MOUSE_COMMAND_PORT 0x64

static unsigned char mouse_cycle = 0;
static unsigned char mouse_packet[3];
static mouse_state_t current_state = { 960, 540, 0, 0, 0 };

static void mouse_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) return;
        io_wait();
    }
}

static void mouse_wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(MOUSE_STATUS_PORT) & 0x01) return;
        io_wait();
    }
}

static void mouse_write(unsigned char val) {
    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0xD4);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, val);
}

static unsigned char mouse_read(void) {
    mouse_wait_read();
    return inb(MOUSE_DATA_PORT);
}

static void mouse_callback(struct regs* r) {
    (void)r;

    unsigned char status = inb(MOUSE_STATUS_PORT);
    if (!(status & 0x01)) return;
    if (!(status & 0x20)) return;

    unsigned char data = inb(MOUSE_DATA_PORT);

    if (mouse_cycle == 0) {
        if (!(data & 0x08)) return;
        mouse_packet[0] = data;
        mouse_cycle = 1;
    } else if (mouse_cycle == 1) {
        mouse_packet[1] = data;
        mouse_cycle = 2;
    } else if (mouse_cycle == 2) {
        mouse_packet[2] = data;
        mouse_cycle = 0;

        current_state.left_button   = (mouse_packet[0] & 0x01);
        current_state.right_button  = (mouse_packet[0] & 0x02) >> 1;
        current_state.middle_button = (mouse_packet[0] & 0x04) >> 2;

        int rel_x = (int)mouse_packet[1];
        int rel_y = (int)mouse_packet[2];

        if (mouse_packet[0] & 0x10) rel_x |= ~0xFF;
        if (mouse_packet[0] & 0x20) rel_y |= ~0xFF;

        current_state.x += rel_x;
        current_state.y -= rel_y;

        if (current_state.x < 0) current_state.x = 0;
        if (current_state.x >= 1920) current_state.x = 1919;
        if (current_state.y < 0) current_state.y = 0;
        if (current_state.y >= 1080) current_state.y = 1079;

        ui_draw_cursor(current_state.x, current_state.y);
    }
}

mouse_state_t mouse_get_state(void) {
    return current_state;
}

void mouse_install(void) {
    while (inb(MOUSE_STATUS_PORT) & 0x01) inb(MOUSE_DATA_PORT);

    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0xA8);

    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0x20);
    unsigned char compaq = mouse_read();

    compaq |= 0x03;
    compaq &= ~(1 << 5);

    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0x60);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, compaq);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    while (inb(MOUSE_STATUS_PORT) & 0x01) inb(MOUSE_DATA_PORT);

    irq_install_handler(12, mouse_callback);
}