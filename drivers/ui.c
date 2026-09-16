#include "ui.h"
#include "fb.h"
#include "fs.h"
#include "string.h"

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

#define INPUT_BUF_MAX 64
static char input_buf[INPUT_BUF_MAX + 1];
static int input_len = 0;

#define FS_ROW_H  20
#define FS_BTN_W  140
#define FS_BTN_H  26
#define FS_BTN_GAP 12
static int fs_selected_index = -1;
static const char* const fs_btn_labels[3] = { "NEW", "DELETE", "RENAME" };

static const unsigned short cursor_mask[16] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111100000000000,
    0b1101110000000000,
    0b1000110000000000,
    0b0000011000000000,
    0b0000011000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
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

static void shell_content_rect(int* x, int* y, int* w, int* h) {
    if (current_active_box == BOX_SHELL) {
        *x = 24;
        *y = 48;
        *w = 1920 - (24 * 2);
        *h = 1080 - 48 - 24;
    } else {
        *x = boxes[0].x;
        *y = boxes[0].y;
        *w = boxes[0].w;
        *h = boxes[0].h;
    }
}

static void draw_input_line(void) {
    int bx, by, bw, bh;
    shell_content_rect(&bx, &by, &bw, &bh);
    (void)bh;

    int ly = (current_active_box == BOX_SHELL) ? by + 64 : by + 100;

    fb_fillrect(bx + 16, ly, bw - 32, 10, COLOR_PANEL_BG);

    char line[INPUT_BUF_MAX + 3];
    line[0] = '>';
    line[1] = ' ';
    for (int i = 0; i < input_len; i++) line[2 + i] = input_buf[i];
    line[2 + input_len] = '\0';

    fb_draw_string(bx + 16, ly, line, COLOR_ACCENT_GRN, COLOR_TRANSPARENT);
}

void ui_handle_key(char c) {
    if (current_active_box != BOX_NONE && current_active_box != BOX_SHELL) return;

    if (c == '\b') {
        if (input_len > 0) input_len--;
    } else if (c == '\n') {
        input_len = 0;
    } else if (c >= 32 && c < 127 && input_len < INPUT_BUF_MAX) {
        input_buf[input_len++] = c;
    } else {
        return;
    }
    input_buf[input_len] = '\0';

    draw_input_line();
}

static void files_content_rect(int* x, int* y, int* w, int* h) {
    if (current_active_box == BOX_FILES) {
        *x = 24;
        *y = 48;
        *w = 1920 - (24 * 2);
        *h = 1080 - 48 - 24;
    } else {
        *x = boxes[1].x;
        *y = boxes[1].y;
        *w = boxes[1].w;
        *h = boxes[1].h;
    }
}

static void uint_to_str(unsigned int n, char* buf) {
    char tmp[12];
    int t = 0, i = 0;

    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (n > 0) { tmp[t++] = '0' + (n % 10); n /= 10; }
    while (t > 0) buf[i++] = tmp[--t];
    buf[i] = '\0';
}

static void draw_files_panel(void) {
    int bx, by, bw, bh;
    files_content_rect(&bx, &by, &bw, &bh);

    fb_fillrect(bx + 1, by + 25, bw - 2, bh - 26, COLOR_PANEL_BG);
    fb_draw_string(bx + 16, by + 40, "NAME           SIZE     TYPE", COLOR_TEXT_DIM, COLOR_TRANSPARENT);

    int btn_y = by + bh - FS_BTN_H - 12;
    int row_y = by + 60;
    int count = fs_count();

    for (int i = 0; i < count && row_y < btn_y - FS_ROW_H; i++) {
        const fs_entry_t* f = fs_get(i);
        if (!f) continue;

        int selected = (i == fs_selected_index);
        fb_fillrect(bx + 12, row_y - 2, bw - 24, FS_ROW_H, selected ? COLOR_HEADER_BG : COLOR_PANEL_BG);

        char line[FS_NAME_MAX + 24];
        char num[12];
        uint_to_str(f->size_kb, num);

        strcpy(line, f->name);
        strcat(line, "  ");
        strcat(line, f->is_dir ? "<DIR>" : num);
        if (!f->is_dir) strcat(line, "KB");
        strcat(line, " ");
        strcat(line, f->type);

        fb_draw_string(bx + 16, row_y, line, f->is_dir ? COLOR_ACCENT_CYAN : COLOR_TEXT_WHITE, COLOR_TRANSPARENT);
        row_y += FS_ROW_H;
    }

    for (int i = 0; i < 3; i++) {
        int btnx = bx + 16 + i * (FS_BTN_W + FS_BTN_GAP);
        fb_fillrect(btnx, btn_y, FS_BTN_W, FS_BTN_H, COLOR_HEADER_BG);
        fb_drawrect(btnx, btn_y, FS_BTN_W, FS_BTN_H, COLOR_ACCENT_CYAN);
        fb_draw_string(btnx + 16, btn_y + 9, fs_btn_labels[i], COLOR_ACCENT_CYAN, COLOR_HEADER_BG);
    }
}

static void files_run_action(int action) {
    if (action == 0) {
        int idx = fs_create(input_len > 0 ? input_buf : 0);
        if (idx >= 0) fs_selected_index = idx;
    } else if (action == 1) {
        if (fs_selected_index >= 0) {
            fs_delete(fs_selected_index);
            fs_selected_index = -1;
        }
    } else if (action == 2) {
        if (fs_selected_index >= 0 && input_len > 0) {
            fs_rename(fs_selected_index, input_buf);
        }
    }
}

static int files_handle_click(int mx, int my) {
    int bx, by, bw, bh;
    files_content_rect(&bx, &by, &bw, &bh);

    int btn_y = by + bh - FS_BTN_H - 12;
    for (int i = 0; i < 3; i++) {
        int btnx = bx + 16 + i * (FS_BTN_W + FS_BTN_GAP);
        if (mx >= btnx && mx <= btnx + FS_BTN_W && my >= btn_y && my <= btn_y + FS_BTN_H) {
            files_run_action(i);
            draw_files_panel();
            return 1;
        }
    }

    int row_y = by + 60;
    int count = fs_count();
    for (int i = 0; i < count && row_y < btn_y - FS_ROW_H; i++) {
        if (my >= row_y - 2 && my < row_y - 2 + FS_ROW_H && mx >= bx + 12 && mx <= bx + bw - 12) {
            fs_selected_index = i;
            draw_files_panel();
            return 1;
        }
        row_y += FS_ROW_H;
    }

    return 0;
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
        draw_input_line();

        draw_files_panel();

        fb_draw_string(boxes[3].x + 16, boxes[3].y + 40, "[ACTIVE PIPELINE]", COLOR_TEXT_DIM, COLOR_TRANSPARENT);
        fb_draw_string(boxes[3].x + 24, boxes[3].y + 60, "[IRQ0: PIT Timer] ===> [Event Pump]", COLOR_ACCENT_GRN, COLOR_TRANSPARENT);
        fb_draw_string(boxes[3].x + 24, boxes[3].y + 80, "[IRQ12: Mouse]   ===> [Cursor Render]", COLOR_ACCENT_GRN, COLOR_TRANSPARENT);

    } else {
        int idx = (int)current_active_box - 1;
        draw_window_card(margin, top, 1920 - (margin * 2), 1080 - top - margin, boxes[idx].title, 1);

        if (current_active_box == BOX_SHELL) {
            fb_draw_string(margin + 16, top + 40, "MAXIMIZED VIEW -- Click to restore", COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
            draw_input_line();
        } else if (current_active_box == BOX_FILES) {
            draw_files_panel();
        } else {
            fb_draw_string(margin + 16, top + 40, "MAXIMIZED VIEW -- Click to restore", COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
        }
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

    if (current_active_box == BOX_FILES) {
        if (files_handle_click(mouse_x, mouse_y)) {
            return current_active_box;
        }
        current_active_box = BOX_NONE;
        ui_draw_desktop();
        return current_active_box;
    }

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
    fs_init();
    ui_draw_desktop();
}