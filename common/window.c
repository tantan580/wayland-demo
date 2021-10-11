#include "common/window.h"
#include "common/display.h"
#include "common/render.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "shared/zalloc.h"
#include <stdlib.h>
#include <assert.h>

static void handle_xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
    struct window *window = data;

    xdg_surface_ack_configure(surface, serial);

    if (window->wait_for_configure) {
        redraw(window, NULL, 0);
        window->wait_for_configure = false;
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = handle_xdg_surface_configure,
};

static void
handle_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
			      int32_t width, int32_t height,
			      struct wl_array *state)
{
}

static void handle_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    struct window *window = data;
    window->running = 0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = handle_xdg_toplevel_configure,
    .close = handle_xdg_toplevel_close,
};

struct window *create_window(struct display *display, int width, int height, const char *title)
{
    struct window *window;

    window = zalloc(sizeof *window);
    if (!window)
        return NULL;
    
    window->running = 0;
    window->callback = NULL;
    window->display = display;
    window->width = width;
    window->height = height;
    //通过刚才绑定的wl_compositor服务创建Server端的weston_surface，返回代理对象 wl_surface。
    window->surface = wl_compositor_create_surface(display->compositor);

    if (display->wm_base) {
        //通过刚才绑定的xdg_shell服务创建Server端的shell_surface，返回代理对象 xdg_surface
        //xdg_surface是作为wl_shell_surface将来的替代品，但还没进Wayland核心协议
        //为什么一个窗口要创建两个surface呢？因为Wayland协议假设Server端对Surface的管理分两个层次。
        //wl_surface.Compositor只负责合成（代码主要在compositor.c）
        //而xdg_surface负责窗口管理。这样，合成渲染和窗口管理的模块既可以方便地相互访问又保证了较低的耦合度。
        //wl-surface--- window->surface，负责合成
        //shell_surface--- window->egl_surface，负责窗口管理）
        window->xdg_surface = xdg_wm_base_get_xdg_surface(display->wm_base, window->surface);
        
        assert(window->xdg_surface);    
        xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);

        window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
        assert(window->xdg_toplevel);
        xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);

        xdg_toplevel_set_title(window->xdg_toplevel, title);
        wl_surface_commit(window->surface);
        window->wait_for_configure = true;
     } else if (display->fshell) {
        zwp_fullscreen_shell_v1_present_surface(display->fshell,
		 					window->surface,
							ZWP_FULLSCREEN_SHELL_V1_PRESENT_METHOD_DEFAULT,
							NULL);
     }
    else {
        assert(0);
    }
    
    return window;
}

void destroy_window(struct window *window) 
{
    if (window->callback)
        wl_callback_destroy(window->callback);

    if (window->buffers[0].buffer)
        wl_buffer_destroy(window->buffers[0].buffer);
    if (window->buffers[1].buffer)
        wl_buffer_destroy(window->buffers[1].buffer);

    if (window->xdg_toplevel)
        xdg_toplevel_destroy(window->xdg_toplevel);
    if (window->xdg_surface)
        xdg_surface_destroy(window->xdg_surface);
    wl_surface_destroy(window->surface);
    free(window);
}