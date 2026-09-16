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
void ui_handle_key(char c);
void ui_handle_mouse_down(int mouse_x, int mouse_y);
void ui_handle_mouse_up(int mouse_x, int mouse_y);
void ui_handle_mouse_move(int mouse_x, int mouse_y);
active_box_t ui_get_active_box(void);

#endif