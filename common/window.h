#ifndef __window__h
#define __window__h

#include <stdbool.h>
#include "buffer.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "opengl/opengl.h"

struct display;

struct window { 
    struct display *display;
    int width, height;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct buffer buffers[2];
    struct buffer *prev_buffer;
    struct wl_callback *callback;
    bool wait_for_configure;
    int running;

    struct render_opengl gl;
    uint32_t benchmark_time, frames;
    struct wl_egl_window *native;
    EGLSurface egl_surface;
    int fullscreen, maxmized, opaque, buffer_size, frame_sync, delay;
};

struct window *create_window(struct display *display, int width, int height,
                const char *title);

void destroy_window(struct window *window);

#endif