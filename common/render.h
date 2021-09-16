#ifndef __render__h
#define __render__h

#include "window.h"

void redraw(void *data, struct wl_callback *callback, uint32_t time) {
    struct window *window = data;
    struct buffer *buffer = window_next_buffer(window);
}

#endif