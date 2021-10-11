#include "common/display.h"
#include "common/window.h"
#include "opengl/egl.h"
#include "opengl/opengl.h"
#include <signal.h>
#include <stdio.h>
#include <wayland-egl-core.h>
#include <wayland-client.h>

static void signal_int(int signum)
{
    
}

int main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display *display = { 0 };
	struct window  *window  = { 0 };

	int ret = 0;
	
	display = create_display_egl();
	window = create_window(display, 250, 250, "simple-egl");
	window->buffer_size = 32;
	window->frame_sync = 1;
	window->delay = 0;

	init_egl(display,window);
	init_gl(window);

	display->seat.cursor_surface = wl_compositor_create_surface(display->compositor);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	/* The mainloop here is a little subtle.  Redrawing will cause
	 * EGL to read events so we can just call
	 * wl_display_dispatch_pending() to handle any events that got
	 * queued up as a side effect. */
	while (window->running && ret != -1) {
		if (window->wait_for_configure) {
			ret = wl_display_dispatch(display->display);
		} else {
			ret = wl_display_dispatch_pending(display->display);
			draw_opengl(window, NULL, 0);
		}
	}

	fprintf(stderr, "simple-egl exiting\n");

	fini_egl(display, window);
	destroy_window(window);
	destroy_display(display);
	return 0;
}