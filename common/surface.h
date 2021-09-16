#ifndef __surface__h
#define __surface__h

#include "display.h"
#include "window.h"
#include "shared/zalloc.h"

static int running = 1;

static void redraw(void *data, struct wl_callback *callback, uint32_t time);

static void paint_pixels(void *image, int padding, int width, int height, uint32_t time)
{
	const int halfh = padding + (height - padding * 2) / 2;
	const int halfw = padding + (width  - padding * 2) / 2;
	int ir, or;
	uint32_t *pixel = image;
	int y;

	/* squared radii thresholds */
	or = (halfw < halfh ? halfw : halfh) - 8;
	ir = or - 32;
	or *= or;
	ir *= ir;

	pixel += padding * width;
	for (y = padding; y < height - padding; y++) {
		int x;
		int y2 = (y - halfh) * (y - halfh);

		pixel += padding;
		for (x = padding; x < width - padding; x++) {
			uint32_t v;

			/* squared distance from center */
			int r2 = (x - halfw) * (x - halfw) + y2;

			if (r2 < ir)
				v = (r2 / 32 + time / 64) * 0x0080401;
			else if (r2 < or)
				v = (y + time / 32) * 0x0080401;
			else
				v = (x + time / 16) * 0x0080401;
			v &= 0x00ffffff;

			/* cross if compositor uses X from XRGB as alpha */
			if (abs(x - y) > 6 && abs(x + y - height) > 6)
				v |= 0xff000000;

			*pixel++ = v;
		}

		pixel += padding;
	} 
}

static const struct wl_callback_listener frame_listener;

static void redraw(void *data, struct wl_callback *callback, uint32_t time)
{
    struct window *window = data;
    struct buffer *buffer;

    buffer = window_next_buffer(window);
	if (!buffer) {
		fprintf(stderr,
			!callback ? "Failed to create the first buffer.\n" :
			"Both buffers busy at redraw(). Server bug?\n");
		abort();
	}

    paint_pixels(buffer->shm_data, 20, window->width, window->height, time);

    wl_surface_attach(window->surface, buffer->buffer, 0, 0);
    wl_surface_damage(window->surface, 20, 20, window->width - 40, window->height - 40);

    if (callback) 
        wl_callback_destroy(callback);
    
    window->callback = wl_surface_frame(window->surface);
    //每当绘制完一帧就发送wl_callback::done消息
    wl_callback_add_listener(window->callback, &frame_listener, window);
    wl_surface_commit(window->surface);
    buffer->busy = 1;
}

static const struct wl_callback_listener frame_listener = {
	redraw
};


#endif