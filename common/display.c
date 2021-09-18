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
#include "xdg-shell-client-protocol.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"

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
	.format = shm_format
};

static void registry_handle_global(void *data, struct wl_registry *registry,
             uint32_t id, const char *interface, uint32_t version)
{
    struct display *d = data;

	fprintf("global object interface :%s\n", interface);
    if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry,
					 id, &wl_compositor_interface, 1);
	} else if(strcmp(interface, "xdg_wm_base") == 0) {
        d->wm_base = wl_registry_bind(registry,
					      id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(d->wm_base, &xdg_wm_base_listener, d);
    } else if(strcmp(interface,  "zwp_fullscreen_shell_v1") == 0) {
		d->fshell = wl_registry_bind(registry,
					     id, &zwp_fullscreen_shell_v1_interface, 1);
	} else if(strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_registry_bind(registry,
					  id, &wl_shm_interface, 1);
		wl_shm_add_listener(d->shm, &shm_listener, d);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

struct display *create_display(void)
{
    struct display *display;

    display = malloc(sizeof *display);
    if (display == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    //通过socket建立与Server端的连接，得到wl_display
    display->display = wl_display_connect(NULL);
    assert(display->display);

    display->has_xrgb = false;
    //可以通过registry拿到所有server端创建的global对象,
	//Server端有一系列的Global对象，如wl_compositor, wl_shm等，串在display->global_list链表里。
    display->registry = wl_display_get_registry(display->display);
    //client监听wl_registry代理对象。这样，当client调用wl_display_get_registry函数或者
	//新的Global对象加入到Server端时，Client会收到event通知
    wl_registry_add_listener(display->registry, &registry_listener,display);

    //等待前面的请求全被Server端处理完，它同步了Client和Server端。
    //这意味着到这个函数返回时，Server端有几个Global对象，
    //回调处理函数registry_handle_global()应该就已经被调用过几次了。
    //registry_handle_global()中会推断是当前这次event代表何种Global对象，然后调用wl_registry_bind()进行绑定，
    //得到远程服务对象的本地代理对象。
    //这些代理对象类型能够是wl_shm, wl_compositor等，但本质上都是wl_proxy类型。
    wl_display_roundtrip(display->display);
    if (display->shm == NULL) {
        fprintf(stderr, "No wl_shm global\n");
        exit(1);
    }

    wl_display_roundtrip(display->display);
    /*
	 * Why do we need two roundtrips here?
	 *
	 * wl_display_get_registry() sends a request to the server, to which
	 * the server replies by emitting the wl_registry.global events.
	 * The first wl_display_roundtrip() sends wl_display.sync. The server
	 * first processes the wl_display.get_registry which includes sending
	 * the global events, and then processes the sync. Therefore when the
	 * sync (roundtrip) returns, we are guaranteed to have received and
	 * processed all the global events.
	 *
	 * While we are inside the first wl_display_roundtrip(), incoming
	 * events are dispatched, which causes registry_handle_global() to
	 * be called for each global. One of these globals is wl_shm.
	 * registry_handle_global() sends wl_registry.bind request for the
	 * wl_shm global. However, wl_registry.bind request is sent after
	 * the first wl_display.sync, so the reply to the sync comes before
	 * the initial events of the wl_shm object.
	 *
	 * The initial events that get sent as a reply to binding to wl_shm
	 * include wl_shm.format. These tell us which pixel formats are
	 * supported, and we need them before we can create buffers. They
	 * don't change at runtime, so we receive them as part of init.
	 *
	 * When the reply to the first sync comes, the server may or may not
	 * have sent the initial wl_shm events. Therefore we need the second
	 * wl_display_roundtrip() call here.
	 *
	 * The server processes the wl_registry.bind for wl_shm first, and
	 * the second wl_display.sync next. During our second call to
	 * wl_display_roundtrip() the initial wl_shm events are received and
	 * processed. Finally, when the reply to the second wl_display.sync
	 * arrives, it guarantees we have processed all wl_shm initial events.
	 *
	 * This sequence contains two examples on how wl_display_roundtrip()
	 * can be used to guarantee, that all reply events to a request
	 * have been received and processed. This is a general Wayland
	 * technique.
	 */
    
    if (!display->has_xrgb) {
        fprintf(stderr, "WL_SHM_FORMAT_XRGB32 not available\n");
        exit(1);
    }

    return display;
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
    
    wl_registry_destroy(display->registry);
    wl_display_flush(display->display);
    wl_display_disconnect(display->display);
    free(display);
}