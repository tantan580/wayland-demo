#ifndef __window__h
#define __window__h

#include <stdbool.h>
#include "buffer.h"

static int running = 1;

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
};

struct window *create_window(struct display *display, int width, int height);

void destroy_window(struct window *window);

#endif