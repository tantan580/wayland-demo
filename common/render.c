#include "render.h"
#include "window.h"
#include "buffer.h"
#include "wayland-client-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct buffer *window_next_buffer(struct window *window)
{
    struct buffer *buffer;
    int ret = 0;

    if (!window->buffers[0].busy) {
        buffer = &window->buffers[0];
        printf("buffer is 0:%d\n", &buffer);        
    }    
    else if(!window->buffers[1].busy) {
        buffer = &window->buffers[1];
        printf("buffe is 1:%d\n", &buffer);
    }
    
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

void redraw(void *data, struct wl_callback *callback, uint32_t time)
{
    struct window *window = data;
    struct buffer *buffer;

    buffer = window_next_buffer(window);
    if (!buffer) {
        fprintf(stderr,
            !callback ? "Failed to create the first buffer.\n":
            "Both buffers busy at redraw(). Server bug?\n");
        abort();
    }
    //向buffer中绘制数据，直接修改像素值
    paint_pixels(buffer->shm_data, 20, window->width, window->height, time);
    //将有绘制数据的buffer attach到当前windows的surface上，便于后续显示
    wl_surface_attach(window->surface, buffer->buffer, 0, 0);
    //向服务端发送damage请求，实际上就是根据之前绘图操作的区域，更新相应damage区域。
    wl_surface_damage(window->surface, 20, 20, window->width - 40, window->height - 40);

    if (callback)
        wl_callback_destroy(callback);
    //设置回调
    window->callback = wl_surface_frame(window->surface);
    //wl_callback由wl_surface_frame()创建，每当服务器显示下一帧使会给wl_callback发送一条消息。
    wl_callback_add_listener(window->callback, &frame_listener, window);
    wl_surface_commit(window->surface);
    buffer->busy = 1;
    printf("buffer_busy 1:%d\n",&data);
}

static const struct wl_callback_listener frame_listener = {
	.done = redraw
};