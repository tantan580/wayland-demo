#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <wayland-client-protocol.h>
#include <unistd.h>
#include <string.h>
#include "buffer.h"
#include "display.h"
#include "shared/os-compatibility.h"

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    struct buffer *mybuf = data;
    mybuf->busy = 0;
    printf("buffer_release 0:%d\n",data);
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_release
};

int create_shm_buffer(struct display *display, struct buffer *buffer, 
    int width, int height, uint32_t format)
{
    struct wl_shm_pool *pool;
    int fd, size, stride;
    void *data;

    stride = width * 4;
    size = stride * height;

    // fd = open("image.bin", O_RDWR);
    fd = os_create_anonymous_file(size);//为了创建共享内存，先创建一个临时文件作为内存映射的backing file

    if (fd < 0 ) {
        fprintf(stderr, "creating a buffer file for %d B failed: %s\n",
			size, strerror(errno));
		return -1;
    }

    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);//将该文件先映射到client端的空间
    if (data == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    pool = wl_shm_create_pool(display->shm, fd, size);
    buffer->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
    wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);//监听这个buffer代理对象，  
    //当Server端不再用这个buffer时，会发送release事件。这样，Client就可以重用这个buffer作下一帧的绘制。
    wl_shm_pool_destroy(pool);
    close(fd);

    buffer->shm_data = data;
    return 0;
}
