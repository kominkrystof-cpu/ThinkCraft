#include "thinkcraft/window.h"
#include "thinkcraft/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <stdio.h>
#include <string.h>

static void glfw_error_callback(int error, const char *description) {
    tc_log_error("GLFW error %d: %s", error, description);
}

static int map_key(int key) {
    switch (key) {
        case GLFW_KEY_W:
            return TC_KEY_W;
        case GLFW_KEY_A:
            return TC_KEY_A;
        case GLFW_KEY_S:
            return TC_KEY_S;
        case GLFW_KEY_D:
            return TC_KEY_D;
        case GLFW_KEY_SPACE:
            return TC_KEY_SPACE;
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            return TC_KEY_CTRL;
        case GLFW_KEY_ESCAPE:
            return TC_KEY_ESCAPE;
        default:
            return -1;
    }
}

static void key_callback(GLFWwindow *handle, int key, int scancode, int action, int mods) {
    tc_window *window = glfwGetWindowUserPointer(handle);
    int mapped = map_key(key);

    (void)scancode;
    (void)mods;

    if (window == NULL || mapped < 0) {
        return;
    }

    if (action == GLFW_PRESS) {
        window->keys[mapped] = 1;
        window->key_pressed[mapped] = 1;
    } else if (action == GLFW_RELEASE) {
        window->keys[mapped] = 0;
    }
}

static void mouse_button_callback(GLFWwindow *handle, int button, int action, int mods) {
    tc_window *window = glfwGetWindowUserPointer(handle);

    (void)mods;

    if (window == NULL || button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }

    if (action == GLFW_PRESS) {
        window->mouse_buttons[TC_MOUSE_LEFT] = 1;
        window->mouse_pressed[TC_MOUSE_LEFT] = 1;
    } else if (action == GLFW_RELEASE) {
        window->mouse_buttons[TC_MOUSE_LEFT] = 0;
    }
}

static void cursor_callback(GLFWwindow *handle, double x, double y) {
    tc_window *window = glfwGetWindowUserPointer(handle);
    double now;

    if (window == NULL || !window->mouse_captured) {
        return;
    }

    now = glfwGetTime();
    if (!window->cursor_ready || now - window->capture_time < 0.2) {
        window->last_cursor_x = x;
        window->last_cursor_y = y;
        window->cursor_ready = 1;
        return;
    }

    {
        double delta_x = x - window->last_cursor_x;
        double delta_y = y - window->last_cursor_y;

        if (delta_x > -200.0 && delta_x < 200.0
            && delta_y > -200.0 && delta_y < 200.0) {
            window->mouse_dx += (float)delta_x;
            window->mouse_dy += (float)delta_y;
        }
    }
    window->last_cursor_x = x;
    window->last_cursor_y = y;
}

int tc_window_create(tc_window *window, int width, int height, int vsync, const char *title) {
    if (window == NULL) {
        return 0;
    }

    memset(window, 0, sizeof(*window));
    window->width = width;
    window->height = height;

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        tc_log_error("failed to initialize GLFW");
        return 0;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window->handle = glfwCreateWindow(width, height, title, NULL, NULL);
    if (window->handle == NULL) {
        tc_log_error("failed to create GLFW window");
        glfwTerminate();
        return 0;
    }

    glfwMakeContextCurrent(window->handle);
    glfwSetWindowUserPointer(window->handle, window);
    glfwSetKeyCallback(window->handle, key_callback);
    glfwSetMouseButtonCallback(window->handle, mouse_button_callback);
    glfwSetCursorPosCallback(window->handle, cursor_callback);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        tc_log_error("failed to load OpenGL functions with Glad");
        glfwDestroyWindow(window->handle);
        window->handle = NULL;
        glfwTerminate();
        return 0;
    }

    glfwSwapInterval(vsync ? 1 : 0);

    tc_log_info("OpenGL %s", (const char *)glGetString(GL_VERSION));
    tc_log_info("Renderer %s", (const char *)glGetString(GL_RENDERER));
    tc_log_info("GLSL %s", (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
    tc_log_info("vsync %s", vsync ? "on" : "off");

    return 1;
}

void tc_window_destroy(tc_window *window) {
    if (window == NULL) {
        return;
    }

    if (window->handle != NULL) {
        glfwDestroyWindow(window->handle);
        window->handle = NULL;
    }

    glfwTerminate();
    memset(window, 0, sizeof(*window));
}

int tc_window_should_close(const tc_window *window) {
    return window == NULL || window->handle == NULL
        || glfwWindowShouldClose(window->handle);
}

void tc_window_poll(void) {
    glfwPollEvents();
}

void tc_window_end_frame(tc_window *window) {
    if (window == NULL) {
        return;
    }

    memset(window->key_pressed, 0, sizeof(window->key_pressed));
    memset(window->mouse_pressed, 0, sizeof(window->mouse_pressed));
    window->mouse_dx = 0.0f;
    window->mouse_dy = 0.0f;
}

void tc_window_swap(const tc_window *window) {
    if (window != NULL && window->handle != NULL) {
        glfwSwapBuffers(window->handle);
    }
}

void tc_window_framebuffer_size(const tc_window *window, int *width, int *height) {
    if (window != NULL && window->handle != NULL) {
        glfwGetFramebufferSize(window->handle, width, height);
    }
}

void tc_window_set_title(const tc_window *window, const char *title) {
    if (window != NULL && window->handle != NULL) {
        glfwSetWindowTitle(window->handle, title);
    }
}

double tc_window_time(void) {
    return glfwGetTime();
}

int tc_window_key_down(const tc_window *window, tc_key key) {
    if (window == NULL || key < 0 || key >= TC_KEY_COUNT) {
        return 0;
    }

    return window->keys[key] != 0;
}

int tc_window_key_pressed(const tc_window *window, tc_key key) {
    if (window == NULL || key < 0 || key >= TC_KEY_COUNT) {
        return 0;
    }

    return window->key_pressed[key] != 0;
}

int tc_window_mouse_down(const tc_window *window, tc_mouse_button button) {
    if (window == NULL || button < 0 || button >= TC_MOUSE_BUTTON_COUNT) {
        return 0;
    }

    return window->mouse_buttons[button] != 0;
}

int tc_window_mouse_pressed(const tc_window *window, tc_mouse_button button) {
    if (window == NULL || button < 0 || button >= TC_MOUSE_BUTTON_COUNT) {
        return 0;
    }

    return window->mouse_pressed[button] != 0;
}

void tc_window_set_mouse_captured(tc_window *window, int captured) {
    if (window == NULL || window->handle == NULL) {
        return;
    }

    captured = captured != 0;
    if (window->mouse_captured == captured) {
        return;
    }

    window->mouse_captured = captured;
    window->cursor_ready = 0;
    window->capture_time = glfwGetTime();
    window->mouse_dx = 0.0f;
    window->mouse_dy = 0.0f;
    glfwSetInputMode(window->handle, GLFW_CURSOR,
                     captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window->handle, GLFW_RAW_MOUSE_MOTION,
                         captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

int tc_window_mouse_captured(const tc_window *window) {
    return window != NULL && window->mouse_captured;
}

void tc_window_consume_mouse_delta(tc_window *window, float *dx, float *dy) {
    if (dx != NULL) {
        *dx = 0.0f;
    }
    if (dy != NULL) {
        *dy = 0.0f;
    }

    if (window == NULL) {
        return;
    }

    if (dx != NULL) {
        *dx = window->mouse_dx;
    }
    if (dy != NULL) {
        *dy = window->mouse_dy;
    }

    window->mouse_dx = 0.0f;
    window->mouse_dy = 0.0f;
}
