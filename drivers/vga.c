#include "vga.h"
#include "io.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static unsigned int terminal_row;
static unsigned int terminal_column;
static unsigned char terminal_color;
static volatile unsigned short* terminal_buffer;

static inline unsigned char vga_entry_color(vga_color_t fg, vga_color_t bg) {
    return (unsigned char)(fg | (bg << 4));
}

static inline unsigned short vga_entry(unsigned char uc, unsigned char color) {
    return (unsigned short)uc | ((unsigned short)color << 8);
}

static void update_cursor(int x, int y) {
    unsigned short pos = (unsigned short)(y * VGA_WIDTH + x);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (volatile unsigned short*)VGA_MEMORY;

    for (unsigned int y = 0; y < VGA_HEIGHT; y++) {
        for (unsigned int x = 0; x < VGA_WIDTH; x++) {
            const unsigned int index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }

    update_cursor(0, 0);
}

void terminal_setcolor(vga_color_t fg, vga_color_t bg) {
    terminal_color = vga_entry_color(fg, bg);
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = 0;
        }
        update_cursor(terminal_column, terminal_row);
        return;
    }

    if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            const unsigned int index = terminal_row * VGA_WIDTH + terminal_column;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
            update_cursor(terminal_column, terminal_row);
        }
        return;
    }

    const unsigned int index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry((unsigned char)c, terminal_color);

    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = 0;
        }
    }

    update_cursor(terminal_column, terminal_row);
}

void terminal_write(const char* data) {
    for (unsigned int i = 0; data[i] != '\0'; i++) {
        terminal_putchar(data[i]);
    }
}