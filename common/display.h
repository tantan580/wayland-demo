#ifndef __display__h
#define __display__h

#include <stdbool.h>
#include "opengl/egl.h"
#include "seat.h"

//一个wl_display就是一个客户端
struct display {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *wm_base;
    struct zwp_fullscreen_shell_v1 *fshell;
    struct wl_shm *shm;
    bool has_xrgb;

    struct seat seat;
    struct egl egl;
    bool useEGL;

	PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;   
};

//create display for shm buffer
struct display *create_display();

//create display for egl
struct display *create_display_egl();

void destroy_display(struct display *display);

#endif