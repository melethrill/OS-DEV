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