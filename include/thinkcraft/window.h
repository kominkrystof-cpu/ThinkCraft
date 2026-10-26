#ifndef THINKCRAFT_WINDOW_H
#define THINKCRAFT_WINDOW_H

struct GLFWwindow;

typedef struct {
    struct GLFWwindow *handle;
    int width;
    int height;
} tc_window;

int tc_window_create(tc_window *window, int width, int height, int vsync, const char *title);
void tc_window_destroy(tc_window *window);
int tc_window_should_close(const tc_window *window);
void tc_window_poll(void);
void tc_window_swap(const tc_window *window);
void tc_window_framebuffer_size(const tc_window *window, int *width, int *height);
void tc_window_set_title(const tc_window *window, const char *title);
double tc_window_time(void);

#endif
