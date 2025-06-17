#ifndef LINUX_X11_PLATFORM_H
#define LINUX_X11_PLATFORM_H

// C standard lib
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

// X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>

// Opengl
#include <glad/glad.h>
#include <GL/gl.h>
#include <GL/glx.h>

// time
#include <time.h>
#include <errno.h>
// dlopen/dlsym/dlclose
#include <dlfcn.h>

pal_window platform_init_window() {
	pal_window window = { 0 };
	// 1. Connect to X11 and get the XCB connection
	window.display = XOpenDisplay(NULL);
	if (!window.display) printf("ERROR: Failed to open X display");

	window.xcb_conn = XGetXCBConnection(window.display);
	int default_screen = DefaultScreen(window.display);

	// 2. Get XCB screen
	const xcb_setup_t* setup = xcb_get_setup(window.xcb_conn);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	for (int i = 0; i < default_screen; ++i) xcb_screen_next(&iter);
	xcb_screen_t* screen = iter.data;
	// 3. Create XCB window
	window.window = xcb_generate_id(window.xcb_conn);
	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	uint32_t values[] = {
		screen->black_pixel,
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS
	};

	xcb_create_window(
		window.xcb_conn,
		XCB_COPY_FROM_PARENT,
		window.window,
		screen->root,
		0, 0, 800, 600,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		screen->root_visual,
		mask, values
	);

	xcb_map_window(window.xcb_conn, window.window);
	xcb_flush(window.xcb_conn);
	XVisualInfo vinfo_template;
	int nitems;
	vinfo_template.screen = default_screen;
	XVisualInfo* vinfo = XGetVisualInfo(window.display, mask, &vinfo_template, &nitems);
	// 4. Create OpenGL context using GLX
	window.ctx = glXCreateContext(
		window.display,
		vinfo,
		NULL,
		GL_TRUE
	);
	if (!window.ctx) printf("ERROR: Failed to create GLX context");

	// 5. Create X11 window from XCB window
	return window;
}

void platform_makecurrent(pal_window window) {
	glXMakeCurrent(window.display, window.window, window.ctx);
}

void platform_swapbuffers(pal_window window) {
	glXSwapBuffers(window.display, window.window);
}

void make_context_current(pal_window window) {
	platform_makecurrent(window);
}

void swap_buffers(pal_window window) {
	platform_swapbuffers(window);
}

uint8_t platform_poll_events(pal_event* event, pal_window* window) {
	xcb_generic_event_t* event;
	while ((event = xcb_poll_for_event(window->xcb_conn))) {
		switch (event->response_type & ~0x80) {
		case XCB_KEY_PRESS:
			free(event);
			//          window->window_should_close = true;
		}
	}
}

uint8_t pal_poll_events(pal_window* window) {
	platform_poll_events(window);
}

void* platform_gl_get_proc_address(const char* procname) {
	return glXGetProcAddress(procname);
}

void* platform_load_dynamic_library(char* so_name) {
	void* hwnd = dlopen(so_name, RTLD_NOW | RTLD_LOCAL);
	assert(hwnd && "Failed to load shared library");
	return hwnd;
}

void* platform_load_dynamic_function(void* so_handle, char* func_name) {
	void* symbol = dlsym(so_handle, func_name);
	assert(symbol && "Failed to load function from shared library");
	return symbol;
}

uint8_t platform_free_dynamic_library(void* so_handle) {
	int result = dlclose(so_handle);
	assert(result == 0 && "Failed to unload shared library");
	return (uint8_t)(result == 0);
}

void platform_sleep(double milliseconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)(milliseconds / 1000);
    ts.tv_nsec = (long)((milliseconds - (ts.tv_sec * 1000)) * 1000000);
    
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
}
#endif // LINUX_X11_PLATFORM_H