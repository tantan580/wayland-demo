#include "opengl/egl.h"
#include "common/window.h"
#include "common/display.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-egl-core.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif

static bool check_egl_extension(const char *extensions, const char *extension)
{
	size_t extlen = strlen(extension);
	const char *end = extensions + strlen(extensions);

	while (extensions < end) {
		size_t n = 0;

		/* Skip whitespaces, if any */
		if (*extensions == ' ') {
			extensions++;
			continue;
		}

		n = strcspn(extensions, " ");

		/* Compare strings */
		if (n == extlen && strncmp(extension, extensions, n) == 0)
			return true; /* Found */

		extensions += n;
	}

	/* Not found */
	return false;
}

static inline void *get_egl_proc_address(const char *address)
{
    const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("extensions: %s /n", extensions);

    if (extensions && 
        (check_egl_extension(extensions, "EGL_EXT_platform_wayland") ||
	     check_egl_extension(extensions, "EGL_KHR_platform_wayland"))) {
         return (void *) eglGetProcAddress(address);
    }

    return NULL;
}

static inline EGLDisplay get_egl_display(EGLenum platform, void *native_display,
				const EGLint *attrib_list)
{
	static PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;

	if (!get_platform_display) {
		get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
			get_egl_proc_address(
				"eglGetPlatformDisplayEXT");
	}

	if (get_platform_display)
		return get_platform_display(platform,
					    native_display, attrib_list);

	return eglGetDisplay((EGLNativeDisplayType) native_display);
}

static inline EGLSurface create_egl_surface(EGLDisplay dpy, EGLConfig config,
				   void *native_window,
				   const EGLint *attrib_list)
{
	static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC
		create_platform_window = NULL;

	if (!create_platform_window) {
		create_platform_window = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)
			get_egl_proc_address(
				"eglCreatePlatformWindowSurfaceEXT");
	}

	if (create_platform_window)
		return create_platform_window(dpy, config,
					      native_window,
					      attrib_list);

	return eglCreateWindowSurface(dpy, config,
				      (EGLNativeWindowType) native_window,
				      attrib_list);
}

static inline EGLBoolean destroy_egl_surface(EGLDisplay display, EGLSurface surface)
{
	return eglDestroySurface(display, surface);
}

void init_egl(struct display *display, struct window *window)
{
    static const struct {
        char *extension, *entrypoint;
    } swap_damage_ext_to_entrypoint[] = {
        {
            .extension = "EGL_EXT_swap_buffers_with_damage",
            .entrypoint = "eglSwapBuffersWithDamageEXT",
        },
        {
			.extension = "EGL_KHR_swap_buffers_with_damage",
			.entrypoint = "eglSwapBuffersWithDamageKHR",           
        },
    };

    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
	const char *extensions;

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint major, minor, n, count, i, size;
	EGLConfig *configs;
	EGLBoolean ret;

    if (window->opaque || window->buffer_size == 16)
        config_attribs[9] = 0;

    display->egl.dpy = get_egl_display(EGL_PLATFORM_WAYLAND_KHR,
						display->display, NULL);
    assert(display->egl.dpy);

    ret = eglInitialize(display->egl.dpy, &major, &minor);
    assert(ret == EGL_TRUE);
    ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);
    //这个函数的功能是用于获取某 display 支持的配置信息 ， display代表屏幕
    if (!eglGetConfigs(display->egl.dpy, NULL, 0, &count) || count <1)
        assert(0);

	configs = calloc(count, sizeof *configs);
	assert(configs);
    //这个函数的功能是用于获取与需求匹配,且某 display 支持的配置信息。
	ret = eglChooseConfig(display->egl.dpy, config_attribs,
			      configs, count, &n);  
    
    assert(ret && n >= 1);

	for (i = 0; i < n; i++) {
		eglGetConfigAttrib(display->egl.dpy,
				   configs[i], EGL_BUFFER_SIZE, &size);
		if (window->buffer_size == size) {
			display->egl.conf = configs[i];
			break;
		}
	}
	free(configs);
	if (display->egl.conf == NULL) {
		fprintf(stderr, "did not find config with buffer size %d\n",
			window->buffer_size);
		exit(EXIT_FAILURE);
	}
 	display->egl.ctx = eglCreateContext(display->egl.dpy,
					    display->egl.conf,
					    EGL_NO_CONTEXT, context_attribs);
	assert(display->egl.ctx);

	display->swap_buffers_with_damage = NULL;
	extensions = eglQueryString(display->egl.dpy, EGL_EXTENSIONS);
	if (extensions &&
	    check_egl_extension(extensions, "EGL_EXT_buffer_age")) {
		for (i = 0; i < (int) ARRAY_LENGTH(swap_damage_ext_to_entrypoint); i++) {
			if (check_egl_extension(extensions,
						       swap_damage_ext_to_entrypoint[i].extension)) {
				/* The EXTPROC is identical to the KHR one */
				display->swap_buffers_with_damage =
					(PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)
					eglGetProcAddress(swap_damage_ext_to_entrypoint[i].entrypoint);
				break;
			}
		}
	}

	if (display->swap_buffers_with_damage)
		printf("has EGL_EXT_buffer_age and %s\n", swap_damage_ext_to_entrypoint[i].extension);
    
    //window
    window->native = wl_egl_window_create(window->surface, window->width, window->height);
    window->egl_surface = create_egl_surface(display->egl.dpy, display->egl.conf, 
                                window->native, NULL);

    ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
                window->egl_surface, window->display->egl.ctx);
    assert(ret == EGL_TRUE);

    if (!window->frame_sync)
        eglSwapInterval(display->egl.dpy, 0);
    
    if (!display->wm_base)
        return;

    if (window->fullscreen)
        xdg_toplevel_set_fullscreen(window->xdg_toplevel, NULL);   
}

void fini_egl(struct display *display, struct window *window)
{
    eglTerminate(display->egl.dpy);
    eglReleaseThread();

	/* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
	 * on eglReleaseThread(). */
	eglMakeCurrent(display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);
    destroy_egl_surface(display->egl.dpy, window->egl_surface);

    wl_egl_window_destroy(window->native);
}