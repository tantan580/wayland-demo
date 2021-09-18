#ifndef __render__h
#define __render__h

#include <stdint.h>

struct wl_callback;
void redraw(void *data, struct wl_callback *callback, uint32_t time);

#endif