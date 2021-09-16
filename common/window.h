#ifndef __window__h
#define __window__h

#include "buffer.h"

struct window { //这个可能需要改成surface
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

static void handle_xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
    struct window *window = data;

    xdg_surface_ack_configure(surface, serial);

    if (window->wait_for_configure) {
        redraw(window, nullptr, 0);
        window->wait_for_configure = false;
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    handle_xdg_surface_configure,
}

static void handle_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                            int32_t width, int32_t height,
                            struct wl_array *state)
{

}

static void handle_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    running = 0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    handle_xdg_toplevel_configure,
    handle_xdg_toplevel_close,
};

static struct window *create_window(struct display *display, int width, int height)
{
    struct window *window;

    window = zalloc(sizeof *window);
    if (!window)
        return nullptr;
    
    window->callback = nullptr;
    window->display = display;
    window->width = width;
    window->height = height;
    window->surface = wl_compositor_create_surface(display->compositor);

    if (display->wm_base) {
        window->xdg_surface = xdg_wm_base_get_xdg_surface(display->wm_base, window->surface);
        
        assert(window->xdg_surface);    
        xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);

        window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
        assert(window->xdg_toplevel);
        xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);

        xdg_toplevel_set_title(window->xdg_toplevel, "simple-shm");
        wl_surface_commit(window->surface);
        window->wait_for_configure = true;
     } //else if (display->fshell) {
        // zwp_fullscreen_shell_v1_present_surface(display->fshell,
		// 					window->surface,
		// 					ZWP_FULLSCREEN_SHELL_V1_PRESENT_METHOD_DEFAULT,
		// 					NULL);
    // }
    else {
        assert(0);
    }
    
    return window;
}

static void destroy_window(struct window *window) 
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

static struct buffer *window_next_buffer(struct window *window)
{
    struct buffer *buffer;
    int ret = 0;

    if (!window->buffers[0].busy)
        buffer = &window->buffers[0];
    else if(!window->buffers[1].busy)
        buffer = &window->buffers[1];
    else
        return NULL;

    if (!buffer->buffer) {
        ret = create_shm_buffer(window->display, buffer,
            window->width, window->height, WL_SHM_FORMAT_XRGB8888);

        if (ret < 0)
            return NULL;
        
        /* paint the padding */
        memset(buffer->shm_data, 0xff, window->width * window->height * 4);
    }

    return buffer;
}

#endif