#ifndef __egl__h
#define __egl__h

#include <EGL/egl.h>
#include <EGL/eglext.h>

struct  display;
struct window;

struct egl {
		EGLDisplay dpy;
		EGLContext ctx;
		EGLConfig conf;
};

void init_egl(struct display *display, struct window *window);

void fini_egl(struct display *display, struct window *window);

#endif