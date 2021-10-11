
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include "opengl/opengl.h"
#include "common/window.h"
#include "common/display.h"
#include <wayland-client.h>

static const char *vs = 
    "uniform mat4 rotation;\n"
	"attribute vec4 pos;\n"
	"attribute vec4 color;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_Position = rotation * pos;\n"
	"  v_color = color;\n"
	"}\n";

static const char *fs = 
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_FragColor = v_color;\n"
	"}\n";

static GLuint create_shader(const char *source, GLenum shader_type)
{
	GLuint shader;
	GLint status;
   	shader = glCreateShader(shader_type);
	assert(shader != 0); 

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling %s: %.*s\n",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len, log);
		exit(1);
	}

	return shader;
}

GLuint init_gl(struct window *window)
{
   	GLuint frag, vert;
	GLuint program;
	GLint status;

	frag = create_shader(fs, GL_FRAGMENT_SHADER);
	vert = create_shader(vs, GL_VERTEX_SHADER);
 
	program = glCreateProgram();
	glAttachShader(program, frag);
	glAttachShader(program, vert);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%.*s\n", len, log);
		exit(1);
	}   

	glUseProgram(program);

	window->gl.pos = 0;
	window->gl.col = 1;

	glBindAttribLocation(program, window->gl.pos, "pos");
	glBindAttribLocation(program, window->gl.col, "color");
	glLinkProgram(program);

	window->gl.rotation_uniform =
		glGetUniformLocation(program, "rotation");
}

void draw_opengl(void *data, struct wl_callback *callback, uint32_t time)
{
    struct window *window = data;
	struct display *display = window->display;
	static const GLfloat verts[3][2] = {
		{ -0.5, -0.5 },
		{  0.5, -0.5 },
		{  0,    0.5 }
	};
	static const GLfloat colors[3][3] = {
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 }
	}; 
	GLfloat angle;
	GLfloat rotation[4][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};
	static const uint32_t speed_div = 5, benchmark_interval = 5;
	struct wl_region *region;
	EGLint rect[4];
	EGLint buffer_age = 0;
	struct timeval tv;

	assert(window->callback == callback);
	window->callback = NULL;

    if (callback)
        wl_callback_destroy(callback);

 	gettimeofday(&tv, NULL);
	time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	if (window->frames == 0)
		window->benchmark_time = time;  

	if (time - window->benchmark_time > (benchmark_interval * 1000)) {
		printf("%d frames in %d seconds: %f fps\n",
		       window->frames,
		       benchmark_interval,
		       (float) window->frames / benchmark_interval);
		window->benchmark_time = time;
		window->frames = 0;
	}

	angle = (time / speed_div) % 360 * M_PI / 180.0;
	rotation[0][0] =  cos(angle);
	rotation[0][2] =  sin(angle);
	rotation[2][0] = -sin(angle);
	rotation[2][2] =  cos(angle);

	if (display->swap_buffers_with_damage)
		eglQuerySurface(display->egl.dpy, window->egl_surface,
				EGL_BUFFER_AGE_EXT, &buffer_age);

	glViewport(0, 0, window->width, window->height);

	glUniformMatrix4fv(window->gl.rotation_uniform, 1, GL_FALSE,
			   (GLfloat *) rotation);

	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(window->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(window->gl.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
	glEnableVertexAttribArray(window->gl.pos);
	glEnableVertexAttribArray(window->gl.col);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(window->gl.pos);
	glDisableVertexAttribArray(window->gl.col);

	usleep(window->delay);

	if (window->opaque || window->fullscreen) {
		region = wl_compositor_create_region(window->display->compositor);
		wl_region_add(region, 0, 0,
			      window->width,
			      window->height);
		wl_surface_set_opaque_region(window->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(window->surface, NULL);
	}

	if (display->swap_buffers_with_damage && buffer_age > 0) {
		rect[0] = window->width / 4 - 1;
		rect[1] = window->height / 4 - 1;
		rect[2] = window->width / 2 + 2;
		rect[3] = window->height / 2 + 2;
		display->swap_buffers_with_damage(display->egl.dpy,
						  window->egl_surface,
						  rect, 1);
	} else {
		eglSwapBuffers(display->egl.dpy, window->egl_surface);
	}
}