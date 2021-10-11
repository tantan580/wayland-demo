#include "common/display.h"
#include "common/window.h"
#include "common/render.h"
#include <wayland-client.h>
#include <signal.h>
#include <stdio.h>

//因为Client需要监听Server传过来的事件，执行对应的回调。
//所以在开发Client程序时，需要去 add_listener，实现收到事件之后要做的工作
static void signal_int(int signum)
{
    
}

int main(int argc, char *argv[])
{
    printf("begin \n");
    struct sigaction sigint;
    struct display *display;
    struct window *window;
    int ret = 0;

    display = create_display();
    window = create_window(display,250, 250, "simple-shm");
    if (!window)
        return 1;
    
    sigint.sa_handler = signal_int;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESETHAND;

    // Initialise damage to full surface, so the padding gets painted
    wl_surface_damage(window->surface, 0, 0, window->width, window->height);

    if (!window->wait_for_configure)
        redraw(window, NULL, 0);
    
    while (window->running && ret != -1)
        ret = wl_display_dispatch(display->display);

    fprintf(stderr, "simple-shm exiting\n");

    destroy_window(window);
    destroy_display(display);
    return 0;   
}