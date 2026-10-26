#include "thinkcraft/window.h"
#include "thinkcraft/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <stdio.h>

static void glfw_error_callback(int error, const char *description) {
    tc_log_error("GLFW error %d: %s", error, description);
}

static void key_callback(GLFWwindow *handle, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(handle, GLFW_TRUE);
    }
}

int tc_window_create(tc_window *window, int width, int height, int vsync, const char *title) {
    if (window == NULL) {
        return 0;
    }

    window->handle = NULL;
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
    glfwSetKeyCallback(window->handle, key_callback);

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
}

int tc_window_should_close(const tc_window *window) {
    return window == NULL || window->handle == NULL
        || glfwWindowShouldClose(window->handle);
}

void tc_window_poll(void) {
    glfwPollEvents();
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
