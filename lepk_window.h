/* Version: 1.1 */

/*
 * MIT License
 * 
 * Copyright (c) 2022 Linus Erik Pontus Kåreblom
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Windowing, single header library.
 * Currently supporting windows and linux, with xcb.
 *
 * Add:
 *     #define LEPK_WINDOW_IMPLEMENTATION
 * in one C or C++ file, before including this file, to create the implementation.
 * 
 * For vulkan support add:
 *     #define LEPK_WINDOW_VULKAN
 * before including this file. Then the function 'lepk_window_get_surface' will be available.
 *
 * When compiling, link to xcb if your compiling for linux.
 */

/*
 * === Documentation ===
 * Usage:
 * LepkWindow *window = lepk_window_create(800, 600, "Window", true);
 *  
 * while (lepk_window_is_open(window)) {
 *     // Do graphics stuff.
 *     lepk_window_poll_events(window);
 * }
 *
 * lepk_window_destroy(window);
 */

#ifndef LEPK_WINDOW_H
#define LEPK_WINDOW_H

#include <stdbool.h>

#ifdef LEPK_WINDOW_STATIC
#define LEPKWINDOW static
#else /* LEPK_WINDOW_STATIC */
#define LEPKWINDOW extern
#endif /* LEPK_WINDOW_STATIC */

/*
 * Window object. Definition is based on OS.
 */
typedef struct LepkWindow LepkWindow;

/* Create window. */
LEPKWINDOW LepkWindow *lepk_window_create(int width, int height, const char *title, bool resizable);
/* Free window. */
LEPKWINDOW void lepk_window_destroy(LepkWindow *window);
/* Check if window is still open or a close event has happened. */
LEPKWINDOW bool lepk_window_is_open(const LepkWindow *window);
/* Poll window events. */
LEPKWINDOW void lepk_window_poll_events(LepkWindow *window);

#ifdef LEPK_WINDOW_VULKAN
#ifdef __linux__
#define VK_USE_PLATFORM_XCB_KHR
#endif /* __linux__ */
#ifdef _WIN32
#define VK_UES_PLATFORM_WIN32_KHR
#endif /* _WIN32 */
#include <vulkan/vulkan.h>
/* Get vulkan surface for window. */
LEPKWINDOW VkSurfaceKHR lepk_window_get_surface(const LepkWindow *window, VkInstance instance);
#endif /* LEPK_WINDOW_VULKAN */

#ifdef LEPK_WINDOW_IMPLEMENTATION
/*
 * Linux
 */
#ifdef __linux__

#include <xcb/xcb.h>
#include <malloc.h>
#include <stddef.h>
#include <string.h>

struct LepkWindow {
	xcb_connection_t *connection;
	xcb_window_t window;
	xcb_screen_t *screen;
	xcb_atom_t wm_protocols;
	xcb_atom_t wm_delete_win;
	bool is_open;
};

LepkWindow *lepk_window_create(int width, int height, const char *title, bool resizable) {
	LepkWindow *window = malloc(sizeof(LepkWindow));
	window->is_open = 1;

	int screenp = 0;
	window->connection = xcb_connect(NULL, &screenp);
	if (xcb_connection_has_error(window->connection)) {
		return NULL;
	}

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(window->connection));
	for (int i = screenp; i > 0; i--) {
		xcb_screen_next(&iter);
	}
	window->screen = iter.data;

	/* Create window. */
	window->window = xcb_generate_id(window->connection);
	int event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	int value_list[] = { window->screen->black_pixel, 0 };

	xcb_create_window(
		window->connection,
		XCB_COPY_FROM_PARENT,
		window->window,
		window->screen->root,
		0, 0, /* Position */
		width, height,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		window->screen->root_visual,
		event_mask,
		value_list
	);

	/* Set title. */
	xcb_change_property(
		window->connection,
		XCB_PROP_MODE_REPLACE,
		window->window,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		strlen(title),
		title
	);

	/* Change resizability. */
	if (!resizable) {
		typedef struct {
			unsigned int flags;
			int x, y;
			int width, height;
			int min_width, min_height;
			int max_width, max_height;
			int width_inc, height_inc;
			int min_apsect_num, min_aspec_den;
			int max_apsect_num, max_aspec_den;
			int base_width, base_height;
			unsigned int win_gravity;
		} WMSizeHints;
		enum {
			WM_SIZE_HINT_US_POSITION   = 1U << 0,
			WM_SIZE_HINT_US_SIZE       = 1U << 1,
			WM_SIZE_HINT_P_POSITION    = 1U << 2,
			WM_SIZE_HINT_P_SIZE        = 1U << 3,
			WM_SIZE_HINT_P_MIN_SIZE    = 1U << 4,
			WM_SIZE_HINT_P_MAX_SIZE    = 1U << 5,
			WM_SIZE_HINT_P_RESIZE_INC  = 1U << 6,
			WM_SIZE_HINT_P_ASPECT      = 1U << 7,
			WM_SIZE_HINT_BASE_SIZE     = 1U << 8,
			WM_SIZE_HINT_P_WIN_GRAVITY = 1U << 9
		};

		WMSizeHints hints = {
			.flags = WM_SIZE_HINT_P_WIN_GRAVITY,
			.win_gravity = XCB_GRAVITY_STATIC
		};

		hints.flags |= WM_SIZE_HINT_P_MIN_SIZE | WM_SIZE_HINT_P_MAX_SIZE;
		hints.min_width = width;
		hints.min_height = height;
		hints.max_width = width;
		hints.max_height = height;

		xcb_change_property(window->connection, XCB_PROP_MODE_REPLACE, window->window, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32, sizeof(WMSizeHints) >> 2, &hints);
	}

	/* Close button. */
	xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(window->connection, 0, strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
	xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(window->connection, 0, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
	xcb_intern_atom_reply_t *wm_delete_reply = xcb_intern_atom_reply(window->connection, wm_delete_cookie, NULL);
	xcb_intern_atom_reply_t *wm_protocols_reply = xcb_intern_atom_reply(window->connection, wm_protocols_cookie, NULL);
	window->wm_delete_win = wm_delete_reply->atom;
	window->wm_protocols = wm_protocols_reply->atom;

	xcb_change_property(window->connection, XCB_PROP_MODE_REPLACE, window->window, wm_protocols_reply->atom, 4, 32, 1, &wm_delete_reply->atom);

	/* Send window to the X server. */
	xcb_map_window(window->connection, window->window);
	xcb_flush(window->connection);

	return window;
}

void lepk_window_destroy(LepkWindow *window) {
	xcb_destroy_window(window->connection, window->window);
	free(window);
}

bool lepk_window_is_open(const LepkWindow *window) {
	return window->is_open;
}

void lepk_window_poll_events(LepkWindow *window) {
	xcb_generic_event_t *event;
	xcb_client_message_event_t *cm;

	while ((event = xcb_poll_for_event(window->connection))) {
		switch (event->response_type & ~0x80) {
			case XCB_CLIENT_MESSAGE:
				cm = (xcb_client_message_event_t *) event;
				/* Window close event. */
				if (cm->data.data32[0] == window->wm_delete_win) {
					window->is_open = false;
				}
				break;
			default:
				break;
		}
		free(event);
	}
}

#ifdef LEPK_WINDOW_VULKAN
VkSurfaceKHR lepk_window_get_surface(const LepkWindow *window, VkInstance instance) {
	VkSurfaceKHR surface;

	VkXcbSurfaceCreateInfoKHR surface_create_info = {0};
	surface_create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surface_create_info.pNext = NULL;
	surface_create_info.flags = 0;
	surface_create_info.connection = window->connection;
	surface_create_info.window = window->window;

	if (vkCreateXcbSurfaceKHR(instance, &surface_create_info, NULL, &surface) != VK_SUCCESS) {
		return NULL;
	}

	return surface;
}
#endif /* LEPK_WINDOW_VULKAN */

#endif /* __linux__ */

/*
 * Windows
 */
#ifdef _WIN32

#include <Windows.h>

struct LepkWindow {
	HINSTANCE instance;
	HWND window;
	bool is_open;
};

static LRESULT CALLBACK lepk__process_message(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
	switch (msg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			break;
	}

	return DefWindowProcA(hwnd, msg, w_param, l_param);
}

LepkWindow *lepk_window_create(int width, int height, const char *title, bool resizable) {
	LepkWindow *window = malloc(sizeof(LepkWindow));
	window->is_open = true;

	/* Get instance. */
	window->instance = GetModuleHandleA(0);

	const char *class_name = "lepk_window_class";
	WNDCLASSA wc = {0};
	wc.lpfnWndProc = lepk__process_message;
	wc.hInstance = window->instance;
	wx.lpszClassName = class_name;
	wc.hCuesor = LoadCursor(NULL, IDC_ARROW);

	if (!RegisterClassA(&wc)) {
		return NULL;
	}

	DWORD style = WS_OVERLAPPED | WS_SYS_MENU | WS_CAPTION | WS_MINIMIZEBOX;
	if (resizable) {
		style |= WS_MAXIMIZEBOX;
		style |= WS_THICKFRAME;
	}

	/* Correct window sizing. */
	RECT border_rect = {0};
	AdjustWindowRectEx(&border_rect, style, 0, 0);
	int border_margin_x = border_rect.right - border_rect.left;
	int border_margin_y = border_rect.bottom - border_rect.top;

	/* Create window. */
	window->window = CreateWindowExA(
		0,
		class_name,
		title,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT, /* Position */
		width + border_margin_x, height + border_margin_y,
		0,
		0,
		window->instance,
		0
	);
	if (window->window == NULL) {
		return NULL;
	}

	/* Present window. */
	ShowWindow(window->window, SW_SHOW);

	return window;
}

void lepk_window_destroy(LepkWindow *window) {
	DestroyWindow(window->window);
	free(window);
}

bool lepk_window_is_open(const LepkWindow *window) {
	return window->is_open;
}

void lepk_window_poll_events(LepkWindow *window) {
	MSG msg = {0};
	while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			window->closed = true;
		} else {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}
}

VkSurfaceKHR lepk_window_get_surface(const LepkWindow *window, VkInstance instance) {
	VkSurfaceKHR surface;

	VkWin32SurfaceCreateInfoKHR surface_create_info = {0};
	surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_create_info.pNext = NULL;
	surface_create_info.flags = 0;
	surface_create_info.hinstance = window->instance;
	surface_create_info.hwnd = window->window;

	if (vkCreateWin32SurfaceKHR(instance, &surface_create_info, NULL, &surface) != VK_SUCCESS) {
		return NULL;
	}

	return surface;
}

#endif /* _WIN32 */

#endif /* LEPK_WINDOW_IMPLEMENTATION */
#endif /* LEPK_WINDOW_H */
