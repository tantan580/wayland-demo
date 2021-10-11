#include "display.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include "xdg-shell-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"

//seat

//xdg_wm_base
static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
    struct display *d = data;

    if (format == WL_SHM_FORMAT_XRGB8888)
        d->has_xrgb = true;
}

struct wl_shm_listener shm_listener = {
    .format = shm_format};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t id, const char *interface, uint32_t version)
{
    struct display *d = data;

    printf("global object interface :%s\n", interface);
    if (strcmp(interface, "wl_compositor") == 0)
    {
        d->compositor =
            wl_registry_bind(registry,
                             id, &wl_compositor_interface, 1);
    }
    else if (strcmp(interface, "xdg_wm_base") == 0)
    {
        d->wm_base = wl_registry_bind(registry,
                                      id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(d->wm_base, &xdg_wm_base_listener, d);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        d->seat.seat = wl_registry_bind(registry, id,
                                   &wl_seat_interface, 1);
        //wl_seat_add_listener(d->seat, &seat_listener, d);
    }
    else if (strcmp(interface, "zwp_fullscreen_shell_v1") == 0)
    {
        d->fshell = wl_registry_bind(registry,
                                     id, &zwp_fullscreen_shell_v1_interface, 1);
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        d->shm = wl_registry_bind(registry,
                                  id, &wl_shm_interface, 1);
        if (!d->useEGL)
            wl_shm_add_listener(d->shm, &shm_listener, d);

        //cursor
        d->seat.cursor_theme = wl_cursor_theme_load(NULL, 32, d->shm);
        if (!d->seat.cursor_theme)
        {
            fprintf(stderr, "unable to load default theme\n");
            return;
        }
        d->seat.default_cursor =
            wl_cursor_theme_get_cursor(d->seat.cursor_theme, "left_ptr");
        if (!d->seat.default_cursor)
        {
            fprintf(stderr, "unable to load default left pointer\n");
            // TODO: abort ?
        }
    }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
                              uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove};

struct display *create_display(void)
{
    struct display *display;

    display = malloc(sizeof *display);
    if (display == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    display->useEGL = false;
    //通过socket建立与Server端的连接，得到wl_display
    display->display = wl_display_connect(NULL);
    assert(display->display);

    display->has_xrgb = false;
    //可以通过registry拿到所有server端创建的global对象,
    //Server端有一系列的Global对象，如wl_compositor, wl_shm等，串在display->global_list链表里。
    display->registry = wl_display_get_registry(display->display);
    //client监听wl_registry代理对象。这样，当client调用wl_display_get_registry函数或者
    //新的Global对象加入到Server端时，Client会收到event通知
    wl_registry_add_listener(display->registry, &registry_listener, display);

    //等待前面的请求全被Server端处理完，它同步了Client和Server端。
    //这意味着到这个函数返回时，Server端有几个Global对象，
    //回调处理函数registry_handle_global()应该就已经被调用过几次了。
    //registry_handle_global()中会推断是当前这次event代表何种Global对象，然后调用wl_registry_bind()进行绑定，
    //得到远程服务对象的本地代理对象。
    //这些代理对象类型能够是wl_shm, wl_compositor等，但本质上都是wl_proxy类型。
    wl_display_roundtrip(display->display);
    if (display->shm == NULL)
    {
        fprintf(stderr, "No wl_shm global\n");
        exit(1);
    }

    wl_display_roundtrip(display->display);

    if (!display->has_xrgb)
    {
        fprintf(stderr, "WL_SHM_FORMAT_XRGB32 not available\n");
        exit(1);
    }

    return display;
}

struct display *create_display_egl()
{
    struct display *display;

    display = malloc(sizeof *display);
    if (display == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    display->useEGL = true;
    display->display = wl_display_connect(NULL);
    assert(display->display);

    display->registry = wl_display_get_registry(display->display);
    wl_registry_add_listener(display->registry,
                             &registry_listener, &display);

    wl_display_roundtrip(display->display);
}

void destroy_display(struct display *display)
{
    if (display->shm)
        wl_shm_destroy(display->shm);

    if (display->wm_base)
        xdg_wm_base_destroy(display->wm_base);

    if (display->fshell)
        zwp_fullscreen_shell_v1_release(display->fshell);

    if (display->compositor)
        wl_compositor_destroy(display->compositor);

    if (display->seat.cursor_surface)
        wl_surface_destroy(display->seat.cursor_surface);
    if (display->seat.cursor_theme)
        wl_cursor_theme_destroy(display->seat.cursor_theme);
    if (display->seat.pointer)
        wl_pointer_destroy(display->seat.pointer);
    if (display->seat.keyboard)
        wl_keyboard_destroy(display->seat.keyboard);
    if (display->seat.touch)
        wl_touch_destroy(display->seat.touch);
    if (display->seat.seat)    
        wl_seat_destroy(display->seat.seat);

    wl_registry_destroy(display->registry);
    wl_display_flush(display->display);
    wl_display_disconnect(display->display);
    free(display);
}
