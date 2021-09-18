#ifndef __buffer__h
#define __buffer__h

#include <stdint.h>

struct display;
struct buffer {
    struct wl_buffer *buffer;
    void *shm_data;
    int busy;
};

int create_shm_buffer(struct display *display, struct buffer *buffer, 
    int width, int height, uint32_t format);

#endif