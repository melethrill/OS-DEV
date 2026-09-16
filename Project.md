Markdown
# OS Development Session Recap

## Summary of Progress
* We successfully transitioned the custom x86 kernel from text mode into a fully graphical 1080p (1920x1080, 32-bit color) environment.
* We implemented a reliable Bochs Graphics Adapter (BGA) fallback to ensure QEMU always boots into the correct high-resolution mode.
* We built a custom 4-quadrant graphical user interface (UI) with drop-shadow panels, sleek 1:1 pixel text rendering, and a white arrow mouse cursor.
* We mapped the PS/2 mouse and keyboard through a custom Interrupt Descriptor Table (IDT) so the mouse freely moves across the 1080p screen and clicks can maximize or restore the UI tiles.

## Important Notes for the Next Session
* The kernel is running in 32-bit protected mode (Ring 0).
* Hardware interrupts are active and unmasked for IRQ 0 (PIT Timer), IRQ 1 (Keyboard), and IRQ 12 (PS/2 Mouse).
* The next major goal is to add real functionality to the 4 quadrants (e.g., clickable buttons in the Command Builder, a working in-memory file system for the Navigator, dynamic RAM gauges, and connectable logic nodes).

## Instructions for the Gemini Agent
1. Read this recap and the provided code to understand the current state of the custom OS.
2. Do not suggest rewriting the graphics engine; it is stable using the BGA fallback on QEMU (`-vga std`).
3. Always provide code in complete, easily copy-pasteable blocks.
4. Use simple, easy-to-understand English and bullet points to explain concepts.
5. Provide instructions one step at a time, waiting for acknowledgment before proceeding.

---

## Final Code State

### 1. `arch/boot.asm`
```nasm
[BITS 32]

MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDMODE  equ  1 << 2
FLAGS    equ  MBALIGN | MEMINFO | VIDMODE
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0 
    dd 0                
    dd 1920             
    dd 1080             
    dd 32               

section .bss
align 16
stack_bottom:
    resb 16384          
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
2. drivers/fb.h
C
#ifndef FB_H
#define FB_H

typedef struct {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int syms[4];
    unsigned int mmap_length;
    unsigned int mmap_addr;
    unsigned int drives_length;
    unsigned int drives_addr;
    unsigned int config_table;
    unsigned int boot_loader_name;
    unsigned int apm_table;
    unsigned int vbe_control_info;
    unsigned int vbe_mode_info;
    unsigned short vbe_mode;
    unsigned short vbe_interface_seg;
    unsigned short vbe_interface_off;
    unsigned short vbe_interface_len;
    unsigned long long framebuffer_addr;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;
    unsigned char color_info[6];
} __attribute__((packed)) multiboot_info_t;

void fb_init(multiboot_info_t* mbi);
void fb_putpixel(int x, int y, unsigned int color);
unsigned int fb_getpixel(int x, int y);
void fb_fillrect(int x, int y, int w, int h, unsigned int color);
void fb_drawrect(int x, int y, int w, int h, unsigned int color);
void fb_draw_char(int x, int y, char c, unsigned int color, unsigned int bg_color);
void fb_draw_string(int x, int y, const char* str, unsigned int color, unsigned int bg_color);
void fb_clear(unsigned int color);

unsigned int fb_get_width(void);
unsigned int fb_get_height(void);

#endif
3. drivers/fb.c
C
#include "fb.h"

static volatile unsigned int* fb_ptr = 0;
static unsigned int fb_width = 1920;
static unsigned int fb_height = 1080;
static unsigned int fb_pitch = 1920 * 4;

static void fb_outw(unsigned short port, unsigned short data) {
    __asm__ volatile ("outw %0, %1" : : "a"(data), "Nd"(port));
}

// 8x8 font
static const unsigned char font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, 
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, 
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, {0x00,0x63,0x66,0x0C,0x18,0x33,0x63,0x00}, 
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, {0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00}, 
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, 
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, 
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, 
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, 
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, 
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}, {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, 
    {0x0C,0x1C,0x34,0x64,0x7E,0x0C,0x0E,0x00}, {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, 
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, 
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, 
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, 
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, 
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, {0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00}, 
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, 
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, 
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, 
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, 
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, 
    {0x0E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00}, {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, 
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, 
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, 
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, {0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00}, 
    {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, 
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, 
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, 
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, 
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, 
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, 
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00}, 
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, 
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, 
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, 
    {0x0E,0x18,0x7E,0x18,0x18,0x18,0x18,0x00}, {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, 
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, 
    {0x06,0x00,0x0E,0x06,0x06,0x66,0x3C,0x00}, {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, 
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, 
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, 
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, 
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, 
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, 
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, {0x00,0x00,0x63,0x6B,0x7F,0x3E,0x36,0x00}, 
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, 
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, 
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, 
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}
};

void fb_init(multiboot_info_t* mbi) {
    if (mbi && (mbi->flags & (1 << 12)) && mbi->framebuffer_addr != 0) {
        fb_ptr = (volatile unsigned int*)(unsigned int)mbi->framebuffer_addr;
        fb_width = mbi->framebuffer_width;
        fb_height = mbi->framebuffer_height;
        fb_pitch = mbi->framebuffer_pitch;
    } else {
        fb_outw(0x01CE, 4); fb_outw(0x01CF, 0); 
        fb_outw(0x01CE, 1); fb_outw(0x01CF, 1920); 
        fb_outw(0x01CE, 2); fb_outw(0x01CF, 1080); 
        fb_outw(0x01CE, 3); fb_outw(0x01CF, 32);   
        fb_outw(0x01CE, 4); fb_outw(0x01CF, 0x41); 

        fb_ptr = (volatile unsigned int*)0xFD000000; 
        fb_width = 1920;
        fb_height = 1080;
        fb_pitch = 1920 * 4;
    }
}

void fb_putpixel(int x, int y, unsigned int color) {
    if (!fb_ptr) return;
    if (x < 0 || (unsigned int)x >= fb_width || y < 0 || (unsigned int)y >= fb_height) return;
    *(fb_ptr + (y * (fb_pitch / 4)) + x) = color;
}

unsigned int fb_getpixel(int x, int y) {
    if (!fb_ptr) return 0;
    if (x < 0 || (unsigned int)x >= fb_width || y < 0 || (unsigned int)y >= fb_height) return 0;
    return *(fb_ptr + (y * (fb_pitch / 4)) + x);
}

void fb_fillrect(int x, int y, int w, int h, unsigned int color) {
    if (!fb_ptr) return;
    for (int r = y; r < y + h; r++) {
        if (r < 0 || (unsigned int)r >= fb_height) continue;
        volatile unsigned int* line = fb_ptr + (r * (fb_pitch / 4));
        for (int c = x; c < x + w; c++) {
            if (c >= 0 && (unsigned int)c < fb_width) {
                line[c] = color;
            }
        }
    }
}

void fb_drawrect(int x, int y, int w, int h, unsigned int color) {
    for (int c = x; c < x + w; c++) {
        fb_putpixel(c, y, color);
        fb_putpixel(c, y + h - 1, color);
    }
    for (int r = y; r < y + h; r++) {
        fb_putpixel(x, r, color);
        fb_putpixel(x + w - 1, r, color);
    }
}

void fb_draw_char(int x, int y, char c, unsigned int color, unsigned int bg_color) {
    if (c < 32 || c > 126) c = ' ';
    const unsigned char* glyph = font8x8[c - 32];

    for (int cy = 0; cy < 8; cy++) {
        unsigned char row = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            if (row & (1 << (7 - cx))) {
                fb_putpixel(x + cx, y + cy, color);
            } else if (bg_color != 0x00FFFFFF) {
                fb_putpixel(x + cx, y + cy, bg_color);
            }
        }
    }
}

void fb_draw_string(int x, int y, const char* str, unsigned int color, unsigned int bg_color) {
    int cur_x = x;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            y += 14; 
        } else {
            fb_draw_char(cur_x, y, *str, color, bg_color);
            cur_x += 8; 
        }
        str++;
    }
}

void fb_clear(unsigned int color) {
    fb_fillrect(0, 0, fb_width, fb_height, color);
}

unsigned int fb_get_width(void) { return fb_width; }
unsigned int fb_get_height(void) { return fb_height; }
4. drivers/ui.h
C
#ifndef UI_H
#define UI_H

#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

typedef enum {
    BOX_NONE = 0,
    BOX_SHELL,
    BOX_FILES,
    BOX_PERF,
    BOX_LOGIC
} active_box_t;

void ui_init(void);
void ui_draw_desktop(void);
void ui_update_telemetry(unsigned int ticks);
void ui_draw_cursor(int x, int y);
active_box_t ui_handle_click(int mouse_x, int mouse_y);
active_box_t ui_get_active_box(void);

#endif
5. drivers/ui.c
C
#include "ui.h"
#include "fb.h"

#define COLOR_DESKTOP_BG  0x001B2234
#define COLOR_PANEL_BG    0x00121722
#define COLOR_PANEL_EDGE  0x002E384D
#define COLOR_HEADER_BG   0x00242D3E
#define COLOR_TEXT_WHITE  0x00E0E6ED
#define COLOR_TEXT_DIM    0x0078889B
#define COLOR_ACCENT_CYAN 0x0000D2FF
#define COLOR_ACCENT_GRN  0x0000E676
#define COLOR_TRANSPARENT 0x00FFFFFF

typedef struct {
    int x, y, w, h;
    const char* title;
} gui_box_t;

static gui_box_t boxes[4];
static active_box_t current_active_box = BOX_NONE;

static const unsigned short cursor_mask[16] = {
    0b1000000000000000, 0b1100000000000000, 0b1110000000000000, 0b1111000000000000,
    0b1111100000000000, 0b1111110000000000, 0b1111111000000000, 0b1111111100000000,
    0b1111100000000000, 0b1101110000000000, 0b1000110000000000, 0b0000011000000000,
    0b0000011000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000
};

static unsigned int cursor_back_buf[16][16];
static int last_mx = 960, last_my = 540;
static int cursor_first_draw = 1;

void ui_draw_cursor(int x, int y) {
    if (!cursor_first_draw) {
        for (int r = 0; r < 16; r++) {
            for (int c = 0; c < 16; c++) {
                fb_putpixel(last_mx + c, last_my + r, cursor_back_buf[r][c]);
            }
        }
    }
    cursor_first_draw = 0;
    last_mx = x;
    last_my = y;

    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {
            cursor_back_buf[r][c] = fb_getpixel(x + c, y + r);
            if (cursor_mask[r] & (1 << (15 - c))) {
                fb_putpixel(x + c, y + r, 0x00FFFFFF);
            }
        }
    }
}

static void draw_window_card(int x, int y, int w, int h, const char* title, int active) {
    fb_fillrect(x + 6, y + 6, w, h, 0x000B0E14); 
    fb_fillrect(x, y, w, h, COLOR_PANEL_BG);
    fb_drawrect(x, y, w, h, active ? COLOR_ACCENT_CYAN : COLOR_PANEL_EDGE);

    fb_fillrect(x, y, w, 24, COLOR_HEADER_BG);
    fb_drawrect(x, y, w, 24, active ? COLOR_ACCENT_CYAN : COLOR_PANEL_EDGE);
    fb_draw_string(x + 12, y + 8, title, active ? COLOR_ACCENT_CYAN : COLOR_TEXT_WHITE, COLOR_HEADER_BG);
}

void ui_draw_desktop(void) {
    cursor_first_draw = 1;
    fb_clear(COLOR_DESKTOP_BG);

    fb_fillrect(0, 0, 1920, 24, 0x000F141E);
    fb_draw_string(16, 8, "WORKBENCH OS [1920x1080 32BPP]", COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
    fb_draw_string(1600, 8, "CLICK A TILE TO MAXIMIZE / RESTORE", COLOR_TEXT_DIM, COLOR_TRANSPARENT);

    int margin = 24;
    int top = 48;
    int w = (1920 - (margin * 3)) / 2;
    int h = (1080 - top - (margin * 2)) / 2;

    boxes[0] = (gui_box_t){ margin, top, w, h, "Quadrant 1: Visual Action & Command Builder" };
    boxes[1] = (gui_box_t){ margin * 2 + w, top, w, h, "Quadrant 2: Dual-Pane File / Object Navigator" };
    boxes[2] = (gui_box_t){ margin, top + h + margin, w, h, "Quadrant 3: Performance Telemetry & Gauges" };
    boxes[3] = (gui_box_t){ margin * 2 + w, top + h + margin, w, h, "Quadrant 4: Visual Logic Engine" };

    if (current_active_box == BOX_NONE) {
        for (int i = 0; i < 4; i++) {
            draw_window_card(boxes[i].x, boxes[i].y, boxes[i].w, boxes[i].h, boxes[i].title, 0);
        }

        fb_draw_string(boxes[0].x + 16, boxes[0].y + 40, "[BLOCK PALETTE]", COLOR_TEXT_DIM, COLOR_TRANSPARENT);
        fb_draw_string(boxes[0].x + 24, boxes[0].y + 60, ">> PING_BUS", COLOR_TEXT_WHITE, COLOR_TRANSPARENT);
        fb_draw_string(boxes[0].x + 24, boxes[0].y + 80, ">> DUMP_MEMORY", COLOR_TEXT_WHITE, COLOR_TRANSPARENT);

        fb_draw_string(boxes[1].x + 16, boxes[1].y + 40, "NAME           SIZE     TYPE", COLOR_TEXT_DIM, COLOR_TRANSPARENT);
        fb_draw_string(boxes[1].x + 16, boxes[1].y + 60, "kernel.bin     42 KB    ELF32", COLOR_TEXT_WHITE, COLOR_TRANSPARENT);
        fb_draw_string(boxes[1].x + 16, boxes[1].y + 80, "drivers/       <DIR>    TREE", COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);

        fb_draw_string(boxes[3].x + 16, boxes[3].y + 40, "[ACTIVE PIPELINE]", COLOR_TEXT_DIM, COLOR_TRANSPARENT);
        fb_draw_string(boxes[3].x + 24, boxes[3].y + 60, "[IRQ0: PIT Timer] ===> [Event Pump]", COLOR_ACCENT_GRN, COLOR_TRANSPARENT);
        fb_draw_string(boxes[3].x + 24, boxes[3].y + 80, "[IRQ12: Mouse]   ===> [Cursor Render]", COLOR_ACCENT_GRN, COLOR_TRANSPARENT);

    } else {
        int idx = (int)current_active_box - 1;
        draw_window_card(margin, top, 1920 - (margin * 2), 1080 - top - margin, boxes[idx].title, 1);
        fb_draw_string(margin + 16, top + 40, "MAXIMIZED VIEW -- Click to restore", COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
    }
}

void ui_update_telemetry(unsigned int ticks) {
    if (current_active_box != BOX_NONE && current_active_box != BOX_PERF) return;

    int bx = (current_active_box == BOX_PERF) ? 40 : boxes[2].x + 16;
    int by = (current_active_box == BOX_PERF) ? 60 : boxes[2].y + 40;

    fb_fillrect(bx, by, 200, 24, COLOR_PANEL_BG);
    fb_draw_string(bx, by, "PIT Clock Ticks:", COLOR_TEXT_WHITE, COLOR_TRANSPARENT);

    char buf[16];
    int n = ticks, i = 0;
    if (n == 0) buf[i++] = '0';
    else {
        char tmp[16]; int t = 0;
        while (n > 0) { tmp[t++] = '0' + (n % 10); n /= 10; }
        while (t > 0) buf[i++] = tmp[--t];
    }
    buf[i] = '\0';
    fb_draw_string(bx + 140, by, buf, COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
}

active_box_t ui_handle_click(int mouse_x, int mouse_y) {
    if (mouse_y < 24) return current_active_box;

    if (current_active_box != BOX_NONE) {
        current_active_box = BOX_NONE;
        ui_draw_desktop();
        return current_active_box;
    }

    for (int i = 0; i < 4; i++) {
        if (mouse_x >= boxes[i].x && mouse_x <= (boxes[i].x + boxes[i].w) &&
            mouse_y >= boxes[i].y && mouse_y <= (boxes[i].y + boxes[i].h)) {
            current_active_box = (active_box_t)(i + 1);
            ui_draw_desktop();
            return current_active_box;
        }
    }
    return current_active_box;
}

active_box_t ui_get_active_box(void) {
    return current_active_box;
}

void ui_init(void) {
    current_active_box = BOX_NONE;
    ui_draw_desktop();
}
6. drivers/mouse.c
C
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
7. kernel/kernel.c
C
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

    idt_install();
    isrs_install();
    irq_install();

    timer_install(100);
    keyboard_install();
    mouse_install();

    fb_init(mb_info);

    __asm__ volatile ("sti");

    ui_init();

    unsigned char last_left = 0;
    unsigned int last_ticks = 0;

    while (1) {
        mouse_state_t ms = mouse_get_state();

        if (ms.left_button && !last_left) {
            ui_handle_click(ms.x, ms.y);
        }
        last_left = ms.left_button;

        unsigned int cur_ticks = timer_get_ticks();
        if (cur_ticks != last_ticks) {
            ui_update_telemetry(cur_ticks);
            last_ticks = cur_ticks;
        }

        __asm__ volatile ("hlt");
    }
}
8. build.ps1
PowerShell
$ErrorActionPreference = "Stop"

$INCLUDE_FLAGS = "-Iarch -Idrivers -Ikernel -Ilib"
$CLANG_FLAGS = "-target i386-unknown-none-elf -ffreestanding -mno-sse -mno-mmx -O2 -Wall -Wextra $INCLUDE_FLAGS"

Write-Host "Assembling boot.asm..."
nasm -f elf32 arch/boot.asm -o arch/boot.o

Write-Host "Assembling interrupts.asm..."
nasm -f elf32 arch/interrupts.asm -o arch/interrupts.o

Write-Host "Compiling lib..."
clang $CLANG_FLAGS.Split() -c lib/string.c -o lib/string.o

Write-Host "Compiling arch..."
clang $CLANG_FLAGS.Split() -c arch/idt.c -o arch/idt.o
clang $CLANG_FLAGS.Split() -c arch/isr.c -o arch/isr.o
clang $CLANG_FLAGS.Split() -c arch/pic.c -o arch/pic.o
clang $CLANG_FLAGS.Split() -c arch/irq.c -o arch/irq.o

Write-Host "Compiling drivers..."
clang $CLANG_FLAGS.Split() -c drivers/fb.c -o drivers/fb.o
clang $CLANG_FLAGS.Split() -c drivers/vga.c -o drivers/vga.o
clang $CLANG_FLAGS.Split() -c drivers/timer.c -o drivers/timer.o
clang $CLANG_FLAGS.Split() -c drivers/keyboard.c -o drivers/keyboard.o
clang $CLANG_FLAGS.Split() -c drivers/mouse.c -o drivers/mouse.o
clang $CLANG_FLAGS.Split() -c drivers/ui.c -o drivers/ui.o

Write-Host "Compiling kernel..."
clang $CLANG_FLAGS.Split() -c kernel/kernel.c -o kernel/kernel.o

Write-Host "Linking kernel..."
clang -target i386-unknown-none-elf -fuse-ld=lld -nostdlib "-Wl,-T,linker.ld" -o kernel.bin `
    arch/boot.o arch/interrupts.o arch/idt.o arch/isr.o arch/pic.o arch/irq.o `
    drivers/fb.o drivers/vga.o drivers/timer.o drivers/keyboard.o drivers/mouse.o drivers/ui.o `
    lib/string.o kernel/kernel.o

Write-Host "Launching QEMU..."
qemu-system-i386 -M pc -vga std -kernel kernel.bin