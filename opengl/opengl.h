#ifndef __opengl__h
#define __opengl__h

#include <GLES2/gl2.h>
#include <stdint.h>

struct window;
struct wl_callback;
struct render_opengl {
    GLuint rotation_uniform;
    GLuint pos;
    GLuint col;
};

GLuint init_gl(struct window *window);

void draw_opengl(void *data, struct wl_callback *callback, uint32_t time);

#endif

