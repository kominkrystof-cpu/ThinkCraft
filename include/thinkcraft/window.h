#ifndef THINKCRAFT_WINDOW_H
#define THINKCRAFT_WINDOW_H

struct GLFWwindow;

typedef enum {
    TC_KEY_W,
    TC_KEY_A,
    TC_KEY_S,
    TC_KEY_D,
    TC_KEY_SPACE,
    TC_KEY_CTRL,
    TC_KEY_ESCAPE,
    TC_KEY_ENTER,
    TC_KEY_UP,
    TC_KEY_DOWN,
    TC_KEY_F5,
    TC_KEY_F9,
    TC_KEY_1,
    TC_KEY_2,
    TC_KEY_3,
    TC_KEY_4,
    TC_KEY_COUNT
} tc_key;

typedef enum {
    TC_MOUSE_LEFT,
    TC_MOUSE_RIGHT,
    TC_MOUSE_BUTTON_COUNT
} tc_mouse_button;

typedef struct {
    struct GLFWwindow *handle;
    int width;
    int height;
    unsigned char keys[TC_KEY_COUNT];
    unsigned char key_pressed[TC_KEY_COUNT];
    unsigned char mouse_buttons[TC_MOUSE_BUTTON_COUNT];
    unsigned char mouse_pressed[TC_MOUSE_BUTTON_COUNT];
    int mouse_captured;
    int cursor_ready;
    double capture_time;
    double last_cursor_x;
    double last_cursor_y;
    float mouse_dx;
    float mouse_dy;
} tc_window;

int tc_window_create(tc_window *window, int width, int height, int vsync, const char *title);
void tc_window_destroy(tc_window *window);
int tc_window_should_close(const tc_window *window);
void tc_window_poll(void);
void tc_window_end_frame(tc_window *window);
void tc_window_swap(const tc_window *window);
void tc_window_framebuffer_size(const tc_window *window, int *width, int *height);
void tc_window_set_title(const tc_window *window, const char *title);
double tc_window_time(void);

int tc_window_key_down(const tc_window *window, tc_key key);
int tc_window_key_pressed(const tc_window *window, tc_key key);
int tc_window_mouse_down(const tc_window *window, tc_mouse_button button);
int tc_window_mouse_pressed(const tc_window *window, tc_mouse_button button);
void tc_window_set_mouse_captured(tc_window *window, int captured);
int tc_window_mouse_captured(const tc_window *window);
void tc_window_consume_mouse_delta(tc_window *window, float *dx, float *dy);

#endif
