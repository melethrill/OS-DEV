#include "ui.h"
#include "fb.h"
#include "fs.h"
#include "string.h"
#include "keyboard.h"

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

#define FS_ROW_H   20
#define FS_BTN_W   140
#define FS_BTN_H   26
#define FS_BTN_GAP 12
static const char* const fs_btn_labels[3] = { "NEW", "DELETE", "RENAME" };

#define NC_BTN_W    120
#define NC_BTN_GAP  10
#define NC_FOOTER_H 26
#define NC_PANE_GAP 16
static const char* const nc_btn_labels[5] = { "NEW", "DELETE", "COPY", "MOVE", "RENAME" };

static int fs_selected_left = -1;
static int fs_selected_right = -1;
static int fs_focus_pane = 0; /* 0 = left, 1 = right */

static int drag_active = 0;
static int drag_source_pane = -1;
static int drag_source_index = -1;
static int drag_last_over_pane = -2;

#define WIN_BTN_SIZE 18

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

/* --- Window chrome: a maximize ("+") / restore ("-") button in the top-right
 * corner of every quadrant header, replacing the old click-anywhere-to-maximize
 * model so quadrant content stays clickable while not maximized. --- */

static void draw_win_button(int x, int y, int w, int maximized) {
    int bx = x + w - WIN_BTN_SIZE - 5;
    int by = y + 3;
    fb_fillrect(bx, by, WIN_BTN_SIZE, WIN_BTN_SIZE, COLOR_PANEL_BG);
    fb_drawrect(bx, by, WIN_BTN_SIZE, WIN_BTN_SIZE, COLOR_ACCENT_CYAN);
    fb_draw_char(bx + 5, by + 5, maximized ? '-' : '+', COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
}

static int hit_win_button(int x, int y, int w, int mx, int my) {
    int bx = x + w - WIN_BTN_SIZE - 5;
    int by = y + 3;
    return mx >= bx && mx <= bx + WIN_BTN_SIZE && my >= by && my <= by + WIN_BTN_SIZE;
}

static void draw_window_card(int x, int y, int w, int h, const char* title, int active) {
    fb_fillrect(x + 6, y + 6, w, h, 0x000B0E14);
    fb_fillrect(x, y, w, h, COLOR_PANEL_BG);
    fb_drawrect(x, y, w, h, active ? COLOR_ACCENT_CYAN : COLOR_PANEL_EDGE);

    fb_fillrect(x, y, w, 24, COLOR_HEADER_BG);
    fb_drawrect(x, y, w, 24, active ? COLOR_ACCENT_CYAN : COLOR_PANEL_EDGE);
    fb_draw_string(x + 12, y + 8, title, active ? COLOR_ACCENT_CYAN : COLOR_TEXT_WHITE, COLOR_HEADER_BG);

    draw_win_button(x, y, w, active);
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

static void format_row(char* line, const fs_entry_t* f) {
    char num[12];
    uint_to_str(f->size_kb, num);

    strcpy(line, f->name);
    strcat(line, "  ");
    strcat(line, f->is_dir ? "<DIR>" : num);
    if (!f->is_dir) strcat(line, "KB");
    strcat(line, " ");
    strcat(line, f->type);
}

/* Deleting shifts every later slot down by one, which can silently
 * invalidate the OTHER pane's stored selection since both panes share
 * one flat table. Clearing both selections keeps things correct. */
static void ui_fs_delete(int index) {
    fs_delete(index);
    fs_selected_left = -1;
    fs_selected_right = -1;
}

/* --- Quadrant 1: Command Builder input line --- */

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

/* --- Quadrant 2, small-tile view: a single list (the LEFT location) with
 * NEW/DELETE/RENAME buttons, usable without maximizing. --- */

static void draw_files_mini(void) {
    int bx = boxes[1].x, by = boxes[1].y, bw = boxes[1].w, bh = boxes[1].h;

    fb_fillrect(bx + 1, by + 25, bw - 2, bh - 26, COLOR_PANEL_BG);
    fb_draw_string(bx + 16, by + 40, "NAME           SIZE     TYPE", COLOR_TEXT_DIM, COLOR_TRANSPARENT);

    int btn_y = by + bh - FS_BTN_H - 12;
    int row_y = by + 60;
    int count = fs_count(FS_LOC_LEFT);

    for (int n = 0; n < count && row_y < btn_y - FS_ROW_H; n++) {
        int idx = fs_get_index(FS_LOC_LEFT, n);
        const fs_entry_t* f = fs_get(idx);
        if (!f) continue;

        int selected = (idx == fs_selected_left);
        fb_fillrect(bx + 12, row_y - 2, bw - 24, FS_ROW_H, selected ? COLOR_HEADER_BG : COLOR_PANEL_BG);

        char line[FS_NAME_MAX + 24];
        format_row(line, f);
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

static void files_mini_handle_click(int mx, int my) {
    int bx = boxes[1].x, by = boxes[1].y, bw = boxes[1].w, bh = boxes[1].h;

    int btn_y = by + bh - FS_BTN_H - 12;
    for (int i = 0; i < 3; i++) {
        int btnx = bx + 16 + i * (FS_BTN_W + FS_BTN_GAP);
        if (mx >= btnx && mx <= btnx + FS_BTN_W && my >= btn_y && my <= btn_y + FS_BTN_H) {
            if (i == 0) {
                int idx = fs_create(FS_LOC_LEFT, input_len > 0 ? input_buf : 0);
                if (idx >= 0) fs_selected_left = idx;
            } else if (i == 1) {
                if (fs_selected_left >= 0) ui_fs_delete(fs_selected_left);
            } else if (i == 2) {
                if (fs_selected_left >= 0 && input_len > 0) fs_rename(fs_selected_left, input_buf);
            }
            draw_files_mini();
            return;
        }
    }

    int row_y = by + 60;
    int count = fs_count(FS_LOC_LEFT);
    for (int n = 0; n < count && row_y < btn_y - FS_ROW_H; n++) {
        if (my >= row_y - 2 && my < row_y - 2 + FS_ROW_H && mx >= bx + 12 && mx <= bx + bw - 12) {
            fs_selected_left = fs_get_index(FS_LOC_LEFT, n);
            draw_files_mini();
            return;
        }
        row_y += FS_ROW_H;
    }
}

/* --- Quadrant 2, maximized view: a Norton-Commander-style dual pane with
 * drag-and-drop copy/move between LEFT and RIGHT. --- */

static void dual_pane_layout(int* lx, int* rx, int* py, int* pw, int* ph, int* footer_y, int* hint_y) {
    int bx = 24, by = 48, bw = 1920 - 48, bh = 1080 - 48 - 24;
    int content_x0 = bx + 16;
    int content_w = bw - 32;
    int pane_w = (content_w - NC_PANE_GAP) / 2;

    *lx = content_x0;
    *rx = content_x0 + pane_w + NC_PANE_GAP;
    *pw = pane_w;

    int content_top = by + 30;
    *hint_y = by + bh - 24;
    *footer_y = *hint_y - NC_FOOTER_H - 8;
    *py = content_top;
    *ph = *footer_y - content_top - 8;
}

static int pane_hit(int px, int py, int pw, int ph, int mx, int my) {
    return mx >= px - 6 && mx <= px + pw + 6 && my >= py - 6 && my <= py + ph + 6;
}

static int pane_row_at(int px, int py, int pw, int ph, int location, int mx, int my) {
    int count = fs_count(location);
    int row_y = py + 36;
    for (int n = 0; n < count && row_y < py + ph - FS_ROW_H; n++) {
        if (my >= row_y - 2 && my < row_y - 2 + FS_ROW_H && mx >= px && mx <= px + pw) {
            return fs_get_index(location, n);
        }
        row_y += FS_ROW_H;
    }
    return -1;
}

static void draw_pane(int px, int py, int pw, int ph, int location, int focused, int selected, int drag_highlight) {
    unsigned int edge_color = drag_highlight ? COLOR_ACCENT_GRN : (focused ? COLOR_ACCENT_CYAN : COLOR_PANEL_EDGE);
    fb_drawrect(px - 6, py - 6, pw + 12, ph + 12, edge_color);
    fb_fillrect(px, py, pw, ph, COLOR_PANEL_BG);

    const char* label = (location == FS_LOC_LEFT) ? "LEFT" : "RIGHT";
    fb_draw_string(px, py, label, focused ? COLOR_ACCENT_CYAN : COLOR_TEXT_DIM, COLOR_TRANSPARENT);
    fb_draw_string(px, py + 16, "NAME           SIZE     TYPE", COLOR_TEXT_DIM, COLOR_TRANSPARENT);

    int count = fs_count(location);
    int row_y = py + 36;
    for (int n = 0; n < count && row_y < py + ph - FS_ROW_H; n++) {
        int idx = fs_get_index(location, n);
        const fs_entry_t* f = fs_get(idx);
        if (!f) continue;

        int is_sel = (idx == selected);
        fb_fillrect(px, row_y - 2, pw, FS_ROW_H, is_sel ? COLOR_HEADER_BG : COLOR_PANEL_BG);

        char line[FS_NAME_MAX + 24];
        format_row(line, f);
        fb_draw_string(px + 4, row_y, line, f->is_dir ? COLOR_ACCENT_CYAN : COLOR_TEXT_WHITE, COLOR_TRANSPARENT);
        row_y += FS_ROW_H;
    }
}

static void draw_dual_panel(void) {
    int lx, rx, py, pw, ph, footer_y, hint_y;
    dual_pane_layout(&lx, &rx, &py, &pw, &ph, &footer_y, &hint_y);

    fb_fillrect(24 + 1, 48 + 25, (1920 - 48) - 2, (1080 - 48 - 24) - 26, COLOR_PANEL_BG);

    int left_drag_hl = (drag_active && drag_source_pane == 1 && drag_last_over_pane == 0);
    int right_drag_hl = (drag_active && drag_source_pane == 0 && drag_last_over_pane == 1);

    draw_pane(lx, py, pw, ph, FS_LOC_LEFT, fs_focus_pane == 0, fs_selected_left, left_drag_hl);
    draw_pane(rx, py, pw, ph, FS_LOC_RIGHT, fs_focus_pane == 1, fs_selected_right, right_drag_hl);

    int total_w = 5 * NC_BTN_W + 4 * NC_BTN_GAP;
    int start_x = lx + (((rx + pw) - lx) - total_w) / 2;
    for (int i = 0; i < 5; i++) {
        int bxx = start_x + i * (NC_BTN_W + NC_BTN_GAP);
        fb_fillrect(bxx, footer_y, NC_BTN_W, NC_FOOTER_H, COLOR_HEADER_BG);
        fb_drawrect(bxx, footer_y, NC_BTN_W, NC_FOOTER_H, COLOR_ACCENT_CYAN);
        fb_draw_string(bxx + 12, footer_y + 9, nc_btn_labels[i], COLOR_ACCENT_CYAN, COLOR_HEADER_BG);
    }

    char hint[80];
    if (fs_focus_pane == 0) {
        strcpy(hint, "ACTIVE: LEFT -> RIGHT  |  DRAG A FILE TO COPY, HOLD CTRL TO MOVE");
    } else {
        strcpy(hint, "ACTIVE: RIGHT -> LEFT  |  DRAG A FILE TO COPY, HOLD CTRL TO MOVE");
    }
    fb_draw_string(lx, hint_y, hint, COLOR_TEXT_DIM, COLOR_TRANSPARENT);
}

static void dual_pane_run_action(int action) {
    int* sel = (fs_focus_pane == 0) ? &fs_selected_left : &fs_selected_right;
    int loc = (fs_focus_pane == 0) ? FS_LOC_LEFT : FS_LOC_RIGHT;
    int other_loc = (fs_focus_pane == 0) ? FS_LOC_RIGHT : FS_LOC_LEFT;

    if (action == 0) { /* NEW */
        int idx = fs_create(loc, input_len > 0 ? input_buf : 0);
        if (idx >= 0) *sel = idx;
    } else if (action == 1) { /* DELETE */
        if (*sel >= 0) ui_fs_delete(*sel);
    } else if (action == 2) { /* COPY */
        if (*sel >= 0) {
            int new_idx = fs_copy(*sel, other_loc);
            if (new_idx >= 0) {
                if (fs_focus_pane == 0) fs_selected_right = new_idx;
                else fs_selected_left = new_idx;
            }
        }
    } else if (action == 3) { /* MOVE */
        if (*sel >= 0) {
            int moved = *sel;
            fs_move(moved, other_loc);
            if (fs_focus_pane == 0) {
                fs_selected_left = -1;
                fs_selected_right = moved;
            } else {
                fs_selected_right = -1;
                fs_selected_left = moved;
            }
        }
    } else if (action == 4) { /* RENAME */
        if (*sel >= 0 && input_len > 0) {
            fs_rename(*sel, input_buf);
        }
    }
}

static void dual_pane_mouse_down(int mx, int my) {
    int lx, rx, py, pw, ph, footer_y, hint_y;
    dual_pane_layout(&lx, &rx, &py, &pw, &ph, &footer_y, &hint_y);
    (void)hint_y;

    int total_w = 5 * NC_BTN_W + 4 * NC_BTN_GAP;
    int start_x = lx + (((rx + pw) - lx) - total_w) / 2;
    for (int i = 0; i < 5; i++) {
        int bxx = start_x + i * (NC_BTN_W + NC_BTN_GAP);
        if (mx >= bxx && mx <= bxx + NC_BTN_W && my >= footer_y && my <= footer_y + NC_FOOTER_H) {
            dual_pane_run_action(i);
            draw_dual_panel();
            return;
        }
    }

    int hit_idx = pane_row_at(lx, py, pw, ph, FS_LOC_LEFT, mx, my);
    if (hit_idx >= 0) {
        fs_selected_left = hit_idx;
        fs_focus_pane = 0;
        drag_active = 1;
        drag_source_pane = 0;
        drag_source_index = hit_idx;
        draw_dual_panel();
        return;
    }

    hit_idx = pane_row_at(rx, py, pw, ph, FS_LOC_RIGHT, mx, my);
    if (hit_idx >= 0) {
        fs_selected_right = hit_idx;
        fs_focus_pane = 1;
        drag_active = 1;
        drag_source_pane = 1;
        drag_source_index = hit_idx;
        draw_dual_panel();
        return;
    }

    if (pane_hit(lx, py, pw, ph, mx, my)) {
        fs_focus_pane = 0;
        draw_dual_panel();
    } else if (pane_hit(rx, py, pw, ph, mx, my)) {
        fs_focus_pane = 1;
        draw_dual_panel();
    }
}

static void dual_pane_mouse_move(int mx, int my) {
    if (!drag_active) return;

    int lx, rx, py, pw, ph, footer_y, hint_y;
    dual_pane_layout(&lx, &rx, &py, &pw, &ph, &footer_y, &hint_y);
    (void)footer_y;
    (void)hint_y;

    int over_pane = -1;
    if (pane_hit(lx, py, pw, ph, mx, my)) over_pane = 0;
    else if (pane_hit(rx, py, pw, ph, mx, my)) over_pane = 1;

    if (over_pane != drag_last_over_pane) {
        drag_last_over_pane = over_pane;
        draw_dual_panel();
    }
}

static void dual_pane_mouse_up(int mx, int my) {
    if (!drag_active) return;

    int lx, rx, py, pw, ph, footer_y, hint_y;
    dual_pane_layout(&lx, &rx, &py, &pw, &ph, &footer_y, &hint_y);
    (void)footer_y;
    (void)hint_y;

    int drop_pane = -1;
    if (pane_hit(lx, py, pw, ph, mx, my)) drop_pane = 0;
    else if (pane_hit(rx, py, pw, ph, mx, my)) drop_pane = 1;

    if (drop_pane >= 0 && drop_pane != drag_source_pane) {
        int dest_location = (drop_pane == 0) ? FS_LOC_LEFT : FS_LOC_RIGHT;

        if (keyboard_ctrl_held()) {
            fs_move(drag_source_index, dest_location);
            if (drop_pane == 0) {
                fs_selected_left = drag_source_index;
                fs_selected_right = -1;
            } else {
                fs_selected_right = drag_source_index;
                fs_selected_left = -1;
            }
        } else {
            int new_idx = fs_copy(drag_source_index, dest_location);
            if (new_idx >= 0) {
                if (drop_pane == 0) fs_selected_left = new_idx;
                else fs_selected_right = new_idx;
            }
        }
        fs_focus_pane = drop_pane;
    }

    drag_active = 0;
    drag_source_pane = -1;
    drag_source_index = -1;
    drag_last_over_pane = -2;
    draw_dual_panel();
}

/* --- Desktop / window management --- */

void ui_draw_desktop(void) {
    cursor_first_draw = 1;
    fb_clear(COLOR_DESKTOP_BG);

    fb_fillrect(0, 0, 1920, 24, 0x000F141E);
    fb_draw_string(16, 8, "WORKBENCH OS [1920x1080 32BPP]", COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
    fb_draw_string(1520, 8, "[+] MAXIMIZE  /  [-] RESTORE", COLOR_TEXT_DIM, COLOR_TRANSPARENT);

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

        draw_files_mini();

        fb_draw_string(boxes[3].x + 16, boxes[3].y + 40, "[ACTIVE PIPELINE]", COLOR_TEXT_DIM, COLOR_TRANSPARENT);
        fb_draw_string(boxes[3].x + 24, boxes[3].y + 60, "[IRQ0: PIT Timer] ===> [Event Pump]", COLOR_ACCENT_GRN, COLOR_TRANSPARENT);
        fb_draw_string(boxes[3].x + 24, boxes[3].y + 80, "[IRQ12: Mouse]   ===> [Cursor Render]", COLOR_ACCENT_GRN, COLOR_TRANSPARENT);

    } else {
        int idx = (int)current_active_box - 1;
        draw_window_card(margin, top, 1920 - (margin * 2), 1080 - top - margin, boxes[idx].title, 1);

        if (current_active_box == BOX_SHELL) {
            fb_draw_string(margin + 16, top + 40, "MAXIMIZED VIEW", COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
            draw_input_line();
        } else if (current_active_box == BOX_FILES) {
            draw_dual_panel();
        } else {
            fb_draw_string(margin + 16, top + 40, "MAXIMIZED VIEW", COLOR_ACCENT_CYAN, COLOR_TRANSPARENT);
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

void ui_handle_mouse_down(int mx, int my) {
    if (my < 24) return;

    if (current_active_box == BOX_NONE) {
        for (int i = 0; i < 4; i++) {
            if (hit_win_button(boxes[i].x, boxes[i].y, boxes[i].w, mx, my)) {
                current_active_box = (active_box_t)(i + 1);
                ui_draw_desktop();
                return;
            }
        }

        if (mx >= boxes[1].x && mx <= boxes[1].x + boxes[1].w &&
            my >= boxes[1].y && my <= boxes[1].y + boxes[1].h) {
            files_mini_handle_click(mx, my);
        }
        return;
    }

    int box_x = 24, box_y = 48, box_w = 1920 - (24 * 2);
    if (hit_win_button(box_x, box_y, box_w, mx, my)) {
        current_active_box = BOX_NONE;
        drag_active = 0;
        drag_source_pane = -1;
        drag_source_index = -1;
        drag_last_over_pane = -2;
        ui_draw_desktop();
        return;
    }

    if (current_active_box == BOX_FILES) {
        dual_pane_mouse_down(mx, my);
    }
}

void ui_handle_mouse_move(int mx, int my) {
    if (current_active_box == BOX_FILES) {
        dual_pane_mouse_move(mx, my);
    }
}

void ui_handle_mouse_up(int mx, int my) {
    if (current_active_box == BOX_FILES) {
        dual_pane_mouse_up(mx, my);
    }
}

active_box_t ui_get_active_box(void) {
    return current_active_box;
}

void ui_init(void) {
    current_active_box = BOX_NONE;
    fs_init();
    ui_draw_desktop();
}
