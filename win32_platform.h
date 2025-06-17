#ifndef WIN32_PLATFORM_H
#define WIN32_PLATFORM_H

// Windows system headers
#include <Windows.h>
#include <windowsx.h>         // Useful macros (e.g., GET_X_LPARAM)
#include <xaudio2.h>          // XAudio2
#include <mmreg.h>            // WAVEFORMATEX


#define PAL_XINPUT_ENABLED 1  // Set to 0 to disable XInput

#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#if PAL_XINPUT_ENABLED
#include <XInput.h>
#pragma comment(lib, "XInput.lib")
#endif

// C Standard Library
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

// OpenGL
#include <gl/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>

// Project headers (always last)
#include "pal_platform.h"

#define KEY_DOWN_BIT 0x80
#define KEY_REPEAT_BIT 0b1

#define MB(x)((size_t)(x) * 1024 * 1024)

// Configuration
#define PAL_MAX_GAMEPADS 16
#define PAL_MAX_BUTTONS 32
#define PAL_MAX_AXES 16
#define PAL_MAX_MAPPINGS 256

typedef unsigned __int64 QWORD;

static HDC s_fakeDC = { 0 };
static int s_glVersionMajor = 3;
static int s_glVersionMinor = 3;
static int s_glProfile = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
static int s_resizable = WS_OVERLAPPEDWINDOW;
static int s_floating = 0;
static int s_doubleBuffer = PFD_DOUBLEBUFFER;

IXAudio2* g_xaudio2 = NULL;
IXAudio2MasteringVoice* g_mastering_voice = NULL;

struct pal_window {
	uint32_t id;
	HWND hwnd;
	HDC hdc;
	HGLRC hglrc;
    DWORD windowedStyle;
    RECT windowedRect;
};

struct pal_monitor {
	HMONITOR handle;
};

// Keyboard & Mouse Input

#define MAX_KEYS 256
#define MAX_MOUSEBUTTONS 32
#define MAX_CONTROLLERS 4

typedef struct Input {
	uint8_t keys[MAX_KEYS];
	uint8_t keys_processed[MAX_KEYS];
	uint8_t mouse_buttons[MAX_MOUSEBUTTONS];
	uint8_t mouse_buttons_processed[MAX_MOUSEBUTTONS];
	v2 mouse_position;
	v2 mouse;
	uint8_t controller_connected[MAX_CONTROLLERS];
	XINPUT_STATE controller_state[MAX_CONTROLLERS];
	XINPUT_STATE controller_prev_state[MAX_CONTROLLERS];
}Input;
Input input = { 0 };

typedef struct {
    uint8_t usage;
    float value;
} win32_gamepad_button;

typedef struct {
    uint8_t usage;
    float value;
    int32_t min, max;
    pal_bool inverted;
} win32_gamepad_axis;

typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    char name[128];
    uint8_t button_map[15];
    struct {
        uint8_t usage;
        pal_bool inverted;
    } axis_map[6];
} win32_gamepad_mapping;

typedef struct {
    uint8_t report[64];
    pal_bool has_report;
    OVERLAPPED overlapped;
} win32_dualsense_state;

// Global State
static struct {
#if PAL_XINPUT_ENABLED
    pal_bool win32_xinput_connected[4];
    XINPUT_STATE win32_xinput_states[4];
#endif

    struct {
        HANDLE handle;
        PHIDP_PREPARSED_DATA pp_data;
        uint16_t vendor_id;
        uint16_t product_id;
        char name[128];
        win32_gamepad_button buttons[PAL_MAX_BUTTONS];
        uint8_t button_count;
        win32_gamepad_axis axes[PAL_MAX_AXES];
        uint8_t axis_count;
        pal_bool connected;
    } win32_raw_devices[PAL_MAX_GAMEPADS];

    struct {
        HANDLE handle;
        win32_dualsense_state state;
        pal_bool connected;
    } win32_dualsense_devices[PAL_MAX_GAMEPADS];

    win32_gamepad_mapping win32_mappings[PAL_MAX_MAPPINGS];
    int win32_mapping_count;
    pal_bool win32_initialized;
    HWND win32_hwnd;
    uint8_t raw_input_buffer[1024];
} win32_gamepad_ctx;

#pragma pack(push, 1)
typedef struct {
    uint16_t idReserved;   // Must be 0
    uint16_t idType;       // Must be 1 for icons
    uint16_t idCount;      // Number of images
} ICONDIR;

typedef struct {
    uint8_t  bWidth;        // Width in pixels
    uint8_t  bHeight;       // Height in pixels
    uint8_t  bColorCount;   // 0 if >= 8bpp
    uint8_t  bReserved;     // Must be 0
    uint16_t wPlanes;       // Should be 1
    uint16_t wBitCount;     // Usually 32
    uint32_t dwBytesInRes;  // Size of PNG data
    uint32_t dwImageOffset; // Offset to PNG data (after header)
} ICONDIRENTRY;
#pragma pack(pop)
static LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
pal_bool platform_make_window_fullscreen(pal_window* window) {
    window->windowedStyle = GetWindowLongA(window->hwnd, GWL_STYLE);
    GetWindowRect(window->hwnd, &window->windowedRect);

    DEVMODE dm = {0};
    dm.dmSize = sizeof(dm);

    HMONITOR monitor = MonitorFromWindow(window->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXA mi = { .cbSize = sizeof(mi) };
    if (!GetMonitorInfoA(monitor, (MONITORINFO*)&mi)) {
        MessageBoxA(window->hwnd, "Failed to get monitor info.", "Error", MB_OK);
        return FALSE;
    }

    if (!EnumDisplaySettingsA(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
        MessageBoxA(window->hwnd, "Failed to get current monitor settings.", "Error", MB_OK);
        return FALSE;
    }

    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

    if (ChangeDisplaySettingsExA(NULL, &dm, NULL, CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL) {
        MessageBoxA(window->hwnd, "Failed to switch display mode", "Error", MB_OK);
        return FALSE;
    }

    SetWindowLongA(window->hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(window->hwnd, HWND_TOP, 0, 0,
        dm.dmPelsWidth, dm.dmPelsHeight,
        SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

    return TRUE;
}
pal_bool platform_make_window_fullscreen_ex(pal_window* window, int width, int height, int refreshRate) {
    window->windowedStyle = GetWindowLongA(window->hwnd, GWL_STYLE);
    GetWindowRect(window->hwnd, &window->windowedRect);

    DEVMODE dm = {0};
    dm.dmSize = sizeof(dm);
    dm.dmPelsWidth = width;
    dm.dmPelsHeight = height;
    dm.dmBitsPerPel = 32;
    dm.dmDisplayFrequency = refreshRate;
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

    if (ChangeDisplaySettingsExA(NULL, &dm, NULL, CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL) {
        MessageBoxA(window->hwnd, "Failed to switch display mode", "Error", MB_OK);
        return FALSE;
    }

    SetWindowLongA(window->hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(window->hwnd, HWND_TOP, 0, 0,
        width, height,
        SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

    return TRUE;
}
pal_bool platform_make_window_fullscreen_windowed(pal_window* window) {
    // Save the current window style and rect
    window->windowedStyle = GetWindowLongA(window->hwnd, GWL_STYLE);
    GetWindowRect(window->hwnd, &window->windowedRect);

    // Get the monitor bounds
    HMONITOR monitor = MonitorFromWindow(window->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { .cbSize = sizeof(mi) };
    if (!GetMonitorInfo(monitor, &mi)) {
        MessageBoxA(window->hwnd, "Failed to get monitor info.", "Error", MB_OK);
        return FALSE;
    }

    // Set the window to borderless fullscreen
    SetWindowLongA(window->hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    if (!SetWindowPos(window->hwnd, HWND_TOP,
        mi.rcMonitor.left, mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_NOOWNERZORDER)) {
        MessageBoxA(window->hwnd, "Failed to resize window.", "Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}
pal_bool platform_make_window_windowed(pal_window* window) {
    // Restore display mode (in case exclusive mode was used)
    ChangeDisplaySettings(NULL, 0);

    // Restore the window style
    if (SetWindowLongA(window->hwnd, GWL_STYLE, window->windowedStyle) == 0) {
        MessageBoxA(window->hwnd, "Failed to restore window style.", "Error", MB_OK);
        return FALSE;
    }

    // Restore the window's size and position
    if (!SetWindowPos(window->hwnd, NULL,
        window->windowedRect.left, window->windowedRect.top,
        window->windowedRect.right - window->windowedRect.left,
        window->windowedRect.bottom - window->windowedRect.top,
        SWP_NOZORDER | SWP_FRAMECHANGED)) {
        MessageBoxA(window->hwnd, "Failed to restore window position.", "Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}

void platform_set_cursor(pal_window *window, const char *filepath, int size) {
    // Clamp size to reasonable max, e.g. 256 (you can adjust)
    if (size <= 0) size = 32;
    if (size > 256) size = 256;

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        MessageBoxA(window->hwnd, "Failed to open cursor file.", "SetCustomCursor Error", MB_ICONERROR);
        return;
    }

    unsigned char header[12] = {0};
    fread(header, 1, sizeof(header), f);
    fclose(f);

    HCURSOR hCursor = NULL;

    if (memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "ACON", 4) == 0) {
        hCursor = (HCURSOR)LoadImageA(NULL, filepath, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);
        if (!hCursor) {
            MessageBoxA(window->hwnd, "Failed to load .ani cursor.", "SetCustomCursor Error", MB_ICONERROR);
            return;
        }
    }
    else if (header[0] == 0x00 && header[1] == 0x00 && header[2] == 0x02 && header[3] == 0x00) {
        hCursor = (HCURSOR)LoadImageA(NULL, filepath, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);
        if (!hCursor) {
            MessageBoxA(window->hwnd, "Failed to load .cur cursor.", "SetCustomCursor Error", MB_ICONERROR);
            return;
        }
    }
    else {
        int width, height, channels;
        unsigned char *pixels = stbi_load(filepath, &width, &height, &channels, 4);
        if (!pixels) {
            MessageBoxA(window->hwnd, "Failed to load image with stb_image.", "SetCustomCursor Error", MB_ICONERROR);
            return;
        }

        unsigned char *resized = malloc(size * size * 4);
        if (!resized) {
            stbi_image_free(pixels);
            MessageBoxA(window->hwnd, "Failed to allocate memory for resized image.", "SetCustomCursor Error", MB_ICONERROR);
            return;
        }

        stbir_resize_uint8_srgb(
            pixels, width, height, width * 4,
            resized, size, size, size * 4,
            STBIR_RGBA
        );
        stbi_image_free(pixels);

        HDC hdc = GetDC(NULL);

        BITMAPV5HEADER bi = {0};
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = size;
        bi.bV5Height = -size;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask   = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask  = 0x000000FF;
        bi.bV5AlphaMask = 0xFF000000;

        void *bitmapData = NULL;
        HBITMAP hBitmap = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bitmapData, NULL, 0);
        ReleaseDC(NULL, hdc);

        if (!hBitmap || !bitmapData) {
            free(resized);
            MessageBoxA(window->hwnd, "Failed to create DIB section.", "SetCustomCursor Error", MB_ICONERROR);
            return;
        }

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                unsigned char *src = &resized[(y * size + x) * 4];
                unsigned char *dst = (unsigned char *)bitmapData + (y * size + x) * 4;
                dst[0] = src[2]; // B
                dst[1] = src[1]; // G
                dst[2] = src[0]; // R
                dst[3] = src[3]; // A
            }
        }

        free(resized);

        ICONINFO ii = {0};
        ii.fIcon = FALSE;
        ii.xHotspot = 0;
        ii.yHotspot = 0;
        ii.hbmColor = hBitmap;
        ii.hbmMask = CreateBitmap(size, size, 1, 1, NULL);

        hCursor = CreateIconIndirect(&ii);

        DeleteObject(ii.hbmMask);
        DeleteObject(hBitmap);

        if (!hCursor) {
            MessageBoxA(window->hwnd, "Failed to create cursor from image.", "SetCustomCursor Error", MB_ICONERROR);
            return;
        }
    }

    SetClassLongPtr(window->hwnd, GCLP_HCURSOR, (LONG_PTR)hCursor);
    SetCursor(hCursor);
}

static HICON load_icon_from_file(const char* image_path, BOOL legacy) {
    FILE* file = fopen(image_path, "rb");
    if (!file) return NULL;

    uint8_t header[8];
    if (fread(header, 1, sizeof(header), file) < sizeof(header)) {
        fclose(file);
        return NULL;
    }

    // Check for ICO header
    if (header[0] == 0x00 && header[1] == 0x00 &&
        header[2] == 0x01 && header[3] == 0x00) {
        fclose(file);
        return (HICON)LoadImageA(NULL, image_path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
    }

    // Check for PNG header
    const unsigned char png_sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    BOOL is_png = (memcmp(header, png_sig, 8) == 0);
    fseek(file, 0, SEEK_SET);

    if (is_png && !legacy) {
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);
        if (size <= 0) {
            fclose(file);
            return NULL;
        }

        uint8_t* png_data = (uint8_t*)malloc(size);
        if (!png_data) {
            fclose(file);
            return NULL;
        }
        fread(png_data, 1, size, file);
        fclose(file);

        size_t header_size = sizeof(ICONDIR) + sizeof(ICONDIRENTRY);
        size_t total_size = header_size + size;
        uint8_t* ico_data = (uint8_t*)malloc(total_size);
        if (!ico_data) {
            free(png_data);
            return NULL;
        }

        ICONDIR* icon_dir = (ICONDIR*)ico_data;
        icon_dir->idReserved = 0;
        icon_dir->idType = 1;
        icon_dir->idCount = 1;

        ICONDIRENTRY* entry = (ICONDIRENTRY*)(ico_data + sizeof(ICONDIR));
        entry->bWidth = 0;
        entry->bHeight = 0;
        entry->bColorCount = 0;
        entry->bReserved = 0;
        entry->wPlanes = 1;
        entry->wBitCount = 32;
        entry->dwBytesInRes = (uint32_t)size;
        entry->dwImageOffset = (uint32_t)header_size;

        memcpy(ico_data + header_size, png_data, size);
        free(png_data);

        HICON hIcon = CreateIconFromResourceEx(
            ico_data + entry->dwImageOffset,
            entry->dwBytesInRes,
            TRUE,
            0x00030000,
            0, 0,
            LR_DEFAULTCOLOR
        );

        free(ico_data);
        return hIcon;
    }

    // Fallback: decode image with stb_image (PNG in legacy mode, JPEG, BMP)
    fclose(file);
    int width, height, channels;
    uint8_t* rgba = stbi_load(image_path, &width, &height, &channels, 4);
    if (!rgba) return NULL;

    // Convert RGBA to BGRA
    for (int i = 0; i < width * height; ++i) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];
        rgba[i * 4 + 0] = b;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = r;
        rgba[i * 4 + 3] = a;
    }

    BITMAPV5HEADER bi = {0};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = width;
    bi.bV5Height = -height;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* dib_pixels = NULL;
    HDC hdc = GetDC(NULL);
    HBITMAP color_bitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &dib_pixels, NULL, 0);
    ReleaseDC(NULL, hdc);

    if (!color_bitmap || !dib_pixels) {
        stbi_image_free(rgba);
        return NULL;
    }

    memcpy(dib_pixels, rgba, width * height * 4);
    stbi_image_free(rgba);

    HBITMAP mask_bitmap = CreateBitmap(width, height, 1, 1, NULL);

    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmMask = mask_bitmap;
    ii.hbmColor = color_bitmap;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(color_bitmap);
    DeleteObject(mask_bitmap);

    return hIcon;
}

int platform_translate_message(MSG msg, pal_window* window, pal_event* event) {
    switch (msg.message) {
        case WM_DESTROY:
        case WM_QUIT:
        case WM_CLOSE:
            event->type = PAL_QUIT;
            event->quit = (pal_quit_event){ .code = 0 };
            break;
		case WM_MOVE:
			event->type = PAL_WINDOW_EVENT;
			event->window = (pal_window_event){
				.windowid = window->id,
				.event_code = WM_MOVE,
				.x = LOWORD(msg.lParam),
				.y = HIWORD(msg.lParam),
				.width = 0,
				.height = 0,
				.focused = 1,
				.visible = 1
			};
			break;
		case WM_SIZE:
			event->type = PAL_WINDOW_EVENT;
			event->window = (pal_window_event){
				.windowid = window->id,
				.event_code = WM_SIZE,
				.x = 0,
				.y = 0,
				.width = LOWORD(msg.lParam),
				.height = HIWORD(msg.lParam),
				.focused = 1,
				.visible = 1
			};
			break;
        case WM_WINDOWPOSCHANGED:
        case WM_WINDOWPOSCHANGING:
            event->type = PAL_WINDOW_EVENT;
            WINDOWPOS* pos = (WINDOWPOS*)msg.lParam;
            event->window = (pal_window_event){
                .windowid = window->id,
                .event_code = msg.message,
                .x = pos->x,
                .y = pos->y,
                .width = pos->cx,
                .height = pos->cy,
                .focused = 1, // guess; could adjust later
                .visible = 1
            };
            break;

        case WM_MOUSEMOVE:
            event->type = PAL_MOUSE_MOTION;
            event->motion = (pal_mouse_motion_event){
                .x = GET_X_LPARAM(msg.lParam),
                .y = GET_Y_LPARAM(msg.lParam),
                .delta_x = 0, // you could track previous pos elsewhere
                .delta_y = 0,
                .buttons = msg.wParam
            };
            break;

        case WM_LBUTTONDOWN: 
            event->type = PAL_MOUSE_BUTTON_DOWN;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam), // xpos of the mouse
                .y = GET_Y_LPARAM(msg.lParam), // ypos of the mouse.
                .pressed = 1, // pressed state of the mouse, might remove.
                .button = LEFT_MOUSE_BUTTON, // optionally remap to your enum
				.clicks = 1,
                .modifiers = msg.wParam
            };
            break;

        case WM_RBUTTONDOWN: 
            event->type = PAL_MOUSE_BUTTON_DOWN;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam), // xpos of the mouse
                .y = GET_Y_LPARAM(msg.lParam), // ypos of the mouse.
                .pressed = 1, // pressed state of the mouse, might remove.
                .button = RIGHT_MOUSE_BUTTON, // optionally remap to your enum
				.clicks = 1,
                .modifiers = msg.wParam
            };
            break;

        case WM_MBUTTONDOWN: 

            event->type = PAL_MOUSE_BUTTON_DOWN;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam), // xpos of the mouse
                .y = GET_Y_LPARAM(msg.lParam), // ypos of the mouse.
                .pressed = 1, // pressed state of the mouse, might remove.
                .button = MIDDLE_MOUSE_BUTTON, // optionally remap to your enum
				.clicks = 1,
                .modifiers = msg.wParam
            };
            break;

        case WM_XBUTTONDOWN: 

            event->type = PAL_MOUSE_BUTTON_DOWN;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam), // xpos of the mouse
                .y = GET_Y_LPARAM(msg.lParam), // ypos of the mouse.
                .pressed = 1, // pressed state of the mouse, might remove.
				.clicks = 1,
                .modifiers = msg.wParam
            };
			if (GET_XBUTTON_WPARAM(msg.wParam) == MK_XBUTTON1)
				event->button.button = SIDE_MOUSE_BUTTON1;
			else 
				event->button.button = SIDE_MOUSE_BUTTON2;
			break;

        case WM_LBUTTONDBLCLK:
            event->type = PAL_MOUSE_BUTTON_DOWN;
			event->button = (pal_mouse_button_event){
				.x = GET_X_LPARAM(msg.lParam), // xpos of the mouse
				.y = GET_Y_LPARAM(msg.lParam), // ypos of the mouse.
				.pressed = 1, // pressed state of the mouse, might remove.
				.button = LEFT_MOUSE_BUTTON, // optionally remap to your enum
				.clicks = 2,
                .modifiers = msg.wParam
            };
            break;
		case WM_RBUTTONDBLCLK:
            event->type = PAL_MOUSE_BUTTON_DOWN;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam), // xpos of the mouse
                .y = GET_Y_LPARAM(msg.lParam), // ypos of the mouse.
                .pressed = 1, // pressed state of the mouse, might remove.
                .button = RIGHT_MOUSE_BUTTON, // optionally remap to your enum
				.clicks = 2,
                .modifiers = msg.wParam
            };
            break;
		case WM_MBUTTONDBLCLK:
            event->type = PAL_MOUSE_BUTTON_DOWN;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam), // xpos of the mouse
                .y = GET_Y_LPARAM(msg.lParam), // ypos of the mouse.
                .pressed = 1, // pressed state of the mouse, might remove.
                .button = MIDDLE_MOUSE_BUTTON, // optionally remap to your enum
				.clicks = 2,
                .modifiers = msg.wParam
            };
            break;

		case WM_XBUTTONDBLCLK:
            event->type = PAL_MOUSE_BUTTON_DOWN;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam), // xpos of the mouse
                .y = GET_Y_LPARAM(msg.lParam), // ypos of the mouse.
                .pressed = 1, // pressed state of the mouse, might remove.
				.clicks = 2,
                .modifiers = msg.wParam
            };

			if (GET_XBUTTON_WPARAM(msg.wParam) == MK_XBUTTON1)
				event->button.button = SIDE_MOUSE_BUTTON1;
			else 
				event->button.button = SIDE_MOUSE_BUTTON2;
            break;

        case WM_LBUTTONUP:
            event->type = PAL_MOUSE_BUTTON_UP;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam),
                .y = GET_Y_LPARAM(msg.lParam),
                .pressed = 0,
                .button = LEFT_MOUSE_BUTTON,
                .modifiers = msg.wParam
            };
            break;
        case WM_RBUTTONUP:
            event->type = PAL_MOUSE_BUTTON_UP;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam),
                .y = GET_Y_LPARAM(msg.lParam),
                .pressed = 0,
                .button = RIGHT_MOUSE_BUTTON,
                .modifiers = msg.wParam
            };
            break;
        case WM_MBUTTONUP:
            event->type = PAL_MOUSE_BUTTON_UP;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam),
                .y = GET_Y_LPARAM(msg.lParam),
                .pressed = 0,
                .button = MIDDLE_MOUSE_BUTTON,
                .modifiers = msg.wParam
            };
            break;

        case WM_XBUTTONUP:
            event->type = PAL_MOUSE_BUTTON_UP;
            event->button = (pal_mouse_button_event){
                .x = GET_X_LPARAM(msg.lParam),
                .y = GET_Y_LPARAM(msg.lParam),
                .pressed = 0,
                .modifiers = msg.wParam
            };
			if (GET_XBUTTON_WPARAM(msg.wParam) == MK_XBUTTON1)
				event->button.button = SIDE_MOUSE_BUTTON1;
			else 
				event->button.button = SIDE_MOUSE_BUTTON2;
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
            event->type = PAL_MOUSE_WHEEL;
            event->wheel = (pal_mouse_wheel_event){
                .x = GET_X_LPARAM(msg.lParam),
                .y = GET_Y_LPARAM(msg.lParam),
                .delta_x = (msg.message == WM_MOUSEHWHEEL) ? (float)delta / WHEEL_DELTA : 0.0f,
                .delta_y = (msg.message == WM_MOUSEWHEEL) ? (float)delta / WHEEL_DELTA : 0.0f,
                .modifiers = GET_KEYSTATE_WPARAM(msg.wParam)
            };
            break;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            event->type = PAL_KEY_DOWN;
            event->key = (pal_keyboard_event){
                .virtual_key = (uint32_t)msg.wParam,
                .scancode = (uint32_t)((msg.lParam >> 16) & 0xFF),
                .pressed = 1,
                .repeat = (msg.lParam >> 30) & 1,
                .modifiers = GetKeyState(VK_SHIFT) < 0 ? 1 : 0 // or more bits
            };
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            event->type = PAL_KEY_UP;
            event->key = (pal_keyboard_event){
                .virtual_key = (uint32_t)msg.wParam,
                .scancode = (uint32_t)((msg.lParam >> 16) & 0xFF),
                .pressed = 0,
                .repeat = 0,
                .modifiers = GetKeyState(VK_SHIFT) < 0 ? 1 : 0
            };
            break;

        case WM_CHAR:
        case WM_UNICHAR:
            event->type = PAL_TEXT_INPUT;
            event->text = (pal_text_input_event){
                .utf8_text = {0}
            };
            {
                char utf8[8] = {0};
                int len = WideCharToMultiByte(CP_UTF8, 0, (WCHAR*)&msg.wParam, 1, utf8, sizeof(utf8), NULL, NULL);
                memcpy(event->text.utf8_text, utf8, len);
            }
            break;

        case WM_INPUT:
            event->type = PAL_SENSOR_UPDATE;
            event->sensor = (pal_sensor_event){
                .device_id = 0,
                .x = 0, .y = 0, .z = 0,
                .sensor_type = 0
            };
            break;

        case WM_DROPFILES: {
            event->type = PAL_DROP_FILE;
            HDROP hDrop = (HDROP)msg.wParam;
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            const char** paths = malloc(sizeof(char*) * count);
            for (UINT i = 0; i < count; ++i) {
                WCHAR buffer[MAX_PATH];
                DragQueryFileW(hDrop, i, buffer, MAX_PATH);
                int len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
                char* utf8 = malloc(len);
                WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8, len, NULL, NULL);
                paths[i] = utf8;
            }
            event->drop = (pal_drop_event){
                .paths = paths,
                .count = count
            };
            DragFinish(hDrop);
            break;
        }

        default:
            event->type = PAL_NONE;
            DispatchMessage(&msg);
            break;
    }

    return 0;
}

void platform_set_window_icon(pal_window* window, const char* image_path) {
    HICON hIcon = load_icon_from_file(image_path, FALSE);
    if (hIcon) {
        SendMessage(window->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(window->hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    } else {
        MessageBoxA(window->hwnd, "Failed to load window icon", "Error", MB_OK | MB_ICONERROR);
    }
}

void platform_set_window_icon_legacy(pal_window* window, const char* image_path) {
    HICON hIcon = load_icon_from_file(image_path, TRUE);
    if (hIcon) {
        SendMessage(window->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(window->hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    } else {
        MessageBoxA(window->hwnd, "Failed to load window icon", "Error", MB_OK | MB_ICONERROR);
    }
}

void platform_set_taskbar_icon(pal_window* window, const char* image_path) {
    HICON hIcon = load_icon_from_file(image_path, FALSE);
    if (hIcon) {
        SetClassLongPtr(window->hwnd, GCLP_HICONSM, (LONG_PTR)hIcon);
        SetClassLongPtr(window->hwnd, GCLP_HICON,   (LONG_PTR)hIcon);
    } else {
        MessageBoxA(window->hwnd, "Failed to load taskbar icon", "Error", MB_OK | MB_ICONERROR);
    }
}

void platform_set_taskbar_icon_legacy(pal_window* window, const char* image_path) {
    HICON hIcon = load_icon_from_file(image_path, TRUE);
    if (hIcon) {
        SetClassLongPtr(window->hwnd, GCLP_HICONSM, (LONG_PTR)hIcon);
        SetClassLongPtr(window->hwnd, GCLP_HICON,   (LONG_PTR)hIcon);
    } else {
        MessageBoxA(window->hwnd, "Failed to load legacy taskbar icon", "Error", MB_OK | MB_ICONERROR);
    }
}


LRESULT CALLBACK win32_fake_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void win32_handle_raw_input(HRAWINPUT raw_input);
static void win32_handle_device_change(HANDLE hDevice, DWORD dwChange);

static LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

    switch (msg) {
        case WM_CLOSE:
            DestroyWindow(hwnd); // This is required to get WM_DESTROY
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0); // Posts WM_QUIT to the message queue
            return 0;
        case WM_INPUT:
            win32_handle_raw_input((HRAWINPUT)lparam);
            return 0;
            
        case WM_INPUT_DEVICE_CHANGE:
            win32_handle_device_change((HANDLE)lparam, (DWORD)wparam);
            return 0;
    }
      return DefWindowProc(hwnd, msg, wparam, lparam);
}

// Window Hints
static void platform_set_window_hint(int type, int value) {
    // This is very jank-tastic. There is probably a better way of doing this. 
    // maybe some of this should become window flags that are passed into the create_window() function?
	switch (type) {
    case GL_VERSION_MAJOR: s_glVersionMajor = value; break;
    case GL_VERSION_MINOR: s_glVersionMinor = value; break;
	case RESIZABLE:
		if (value)
			s_resizable = WS_OVERLAPPEDWINDOW;
		else
			s_resizable = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
		break;
	case FLOATING:
		if (value)
			s_floating = WS_EX_TOPMOST;
		else
			s_floating = 0;
		break;
	case DOUBLE_BUFFER:
		if (value)
			s_doubleBuffer = PFD_DOUBLEBUFFER;
		else
			s_doubleBuffer = 0x00000000;
		break;
	case GL_PROFILE:
		if (value == GL_PROFILE_CORE)
			s_glProfile = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
		if (value == GL_PROFILE_COMPAT)
			s_glProfile = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
		break;
	}
}

static pal_window* platform_init_window(int width, int height, const char* windowTitle) {
	pal_window* fakewindow = (pal_window*)malloc(sizeof(pal_window));
    WNDCLASSEXA fakewc = { 0 };

	fakewc.cbSize = sizeof(WNDCLASSEXA);
	fakewc.lpfnWndProc = win32_fake_window_proc;
	fakewc.hInstance = GetModuleHandleA(0);
	fakewc.lpszClassName = "Win32 Fake Window Class";
	fakewc.hCursor = LoadCursorA(NULL, IDC_ARROW);

	RegisterClassExA(&fakewc);

	fakewindow->hwnd = CreateWindowExA(
		0,                              // Optional window styles.
		fakewc.lpszClassName,                     // Window class
		"Fake Ass Window.",          // Window text
		WS_OVERLAPPEDWINDOW,            // Window style

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

		NULL,       // Parent window    
		NULL,       // Menu
		fakewc.hInstance,  // Instance handle
		NULL        // Additional application data
	);

	if (fakewindow->hwnd == NULL)
	{
		return fakewindow;
	}

	s_fakeDC = GetDC(fakewindow->hwnd);

	PIXELFORMATDESCRIPTOR fakePFD;
	ZeroMemory(&fakePFD, sizeof(fakePFD));
	fakePFD.nSize = sizeof(fakePFD);
	fakePFD.nVersion = 1;
	fakePFD.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	fakePFD.iPixelType = PFD_TYPE_RGBA;
	fakePFD.cColorBits = 32;
	fakePFD.cAlphaBits = 8;
	fakePFD.cDepthBits = 24;

	int fakePFDID = ChoosePixelFormat(s_fakeDC, &fakePFD);

	if (fakePFDID == 0) {
		MessageBoxA(fakewindow->hwnd, "ChoosePixelFormat() failed.", "Try again later", MB_ICONERROR);
		return fakewindow;
	}
	if (SetPixelFormat(s_fakeDC, fakePFDID, &fakePFD) == 0) {
		MessageBoxA(fakewindow->hwnd, "SetPixelFormat() failed.", "Try again later", MB_ICONERROR);
		return fakewindow;
	}

	HGLRC fakeRC = wglCreateContext(s_fakeDC);
	if (fakeRC == 0) {
		MessageBoxA(fakewindow->hwnd, "wglCreateContext() failed.", "Try again later", MB_ICONERROR);
		return fakewindow;
	}
	if (wglMakeCurrent(s_fakeDC, fakeRC) == 0) {
		MessageBoxA(fakewindow->hwnd, "wglMakeCurrent() failed.", "Try again later", MB_ICONERROR);
		return fakewindow;
	}
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;
	wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)(wglGetProcAddress("wglChoosePixelFormatARB"));
	if (wglChoosePixelFormatARB == NULL) {
		MessageBoxA(fakewindow->hwnd, "wglGetProcAddress() failed.", "Try again later", MB_ICONERROR);
		return fakewindow;
	}
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;
	wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)(wglGetProcAddress("wglCreateContextAttribsARB"));
	if (wglCreateContextAttribsARB == NULL) {
		MessageBoxA(fakewindow->hwnd, "wglGetProcAddress() failed.", "Try again later", MB_ICONERROR);
		return fakewindow;
	}
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	if (wglSwapIntervalEXT == NULL) {
		MessageBoxA(fakewindow->hwnd, "wglGetProcAddress() failed.", "Try again later", MB_ICONERROR);
		return fakewindow;
	}

    WNDCLASSEXA wc = {0};

	wc.cbSize = sizeof(WNDCLASSEXA);
	wc.lpfnWndProc = win32_window_proc;
	wc.hInstance = GetModuleHandleA(0);
	wc.lpszClassName = "Win32 Window Class";
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);

	RegisterClassExA(&wc);

	pal_window* window = (pal_window*)malloc(sizeof(pal_window));
	window->hwnd = CreateWindowExA(
		s_floating,           // Optional window styles.
		wc.lpszClassName,     // Window class
		windowTitle,          // Window text
		s_resizable,          // Window style

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, width, height,

		NULL,       // Parent window    
		NULL,       // Menu
		wc.hInstance,  // Instance handle
		NULL        // Additional application data
	);

	if (window->hwnd == NULL) {
		return window;
	}

	window->hdc = GetDC(window->hwnd);

	const int pixelAttribs[] = {
	WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
	WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
	WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
	WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
	WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
	WGL_COLOR_BITS_ARB, 32,
	WGL_ALPHA_BITS_ARB, 8,
	WGL_DEPTH_BITS_ARB, 24,
	WGL_STENCIL_BITS_ARB, 8,
	WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
	WGL_SAMPLES_ARB, 4, // NOTE: Maybe this is used for multisampling?
	0
	};

	int pixelFormatID; UINT numFormats;
	uint8_t status = wglChoosePixelFormatARB(window->hdc, pixelAttribs, NULL, 1, &pixelFormatID, &numFormats);
	if (status == 0 || numFormats == 0) {
		MessageBoxA(window->hwnd, "wglChoosePixelFormatARB() failed.", "Try again later", MB_ICONERROR);
		return window;
	}

	PIXELFORMATDESCRIPTOR PFD;
	PFD.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | s_doubleBuffer;
	DescribePixelFormat(window->hdc, pixelFormatID, sizeof(PFD), &PFD);
	SetPixelFormat(window->hdc, pixelFormatID, &PFD);

	int contextAttribs[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, s_glVersionMajor,
		WGL_CONTEXT_MINOR_VERSION_ARB, s_glVersionMinor,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB, s_glProfile,
		0
	};

	window->hglrc = wglCreateContextAttribsARB(window->hdc, 0, contextAttribs);
	if (window->hglrc) {

		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(fakeRC);
		ReleaseDC(fakewindow->hwnd, s_fakeDC);
		DestroyWindow(fakewindow->hwnd);

		ShowWindow(window->hwnd, SW_SHOWNORMAL);
		SetForegroundWindow(window->hwnd);
		SetFocus(window->hwnd);
		OutputDebugStringA("INFO: Using modern OpenGL Context.");
        // save the window style and the window rect in case the user sets the window to windowed before setting it to fullscreen.
        // The fullscreen function is supposed to save this state whenever the user calls it,
        // but if the user doesn't, the make_window_windowed() function uses a state that's all zeroes
        //, so we have to save it here. - Abdelrahman june 13, 2024
		window->windowedStyle = GetWindowLongA(window->hwnd, GWL_STYLE); // style of the window.
		GetWindowRect(window->hwnd, &window->windowedRect); // size and pos of the window.
        free(fakewindow);
		return window;
	}
	else {
        // This is supposed to be a fallback in case we can't create the context that we want.
        // Ideally, this should never happen. - Abdelrahman june 13, 2024
		ShowWindow(fakewindow->hwnd, SW_SHOW);
		SetForegroundWindow(fakewindow->hwnd);
		SetFocus(fakewindow->hwnd);
		OutputDebugStringA("INFO: Using old OpenGL Context.");
		return fakewindow;
	}

}

static int platform_make_context_current(pal_window* window) {
	if (!wglMakeCurrent(window->hdc, window->hglrc)) {
		MessageBoxA(window->hwnd, "wglMakeCurrent() failed.", "Try again later", MB_ICONERROR);
		return 1;
	}
	return 0;
}

static uint8_t platform_poll_events(pal_event* event, pal_window* window) {
	//platform_get_raw_input_buffer();

	MSG msg = {0};
	if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE) != 0) {
	
		if (msg.message == WM_DESTROY) {
			PostQuitMessage(0);
		}

		TranslateMessage(&msg);

        platform_translate_message(msg, window, event);

		return 1;
	}
	else {
		return 0;
	}

}

static uint8_t platform_set_window_title(pal_window* window, const char* string) {
	return SetWindowTextA(window->hwnd, string);
}

static VideoMode* platform_get_video_mode(pal_monitor* monitor) {

	MONITORINFO mi = { 0 };
	VideoMode* videoMode = calloc(1, sizeof(VideoMode));
	
	if (monitor->handle) {
		mi.cbSize = sizeof(MONITORINFO);
		if (GetMonitorInfoA(monitor->handle, &mi)) {
			videoMode->width = mi.rcWork.left;
			videoMode->height = mi.rcWork.right;
		}
		else {
			printf("ERROR: Couldn't get monitor info!\n");
		}
	}
	else {
		printf("ERROR: invalid pointer to monitor!\n");

	}
	return videoMode;
}

static pal_monitor* platform_get_primary_monitor() {
	pal_monitor* monitor = malloc(sizeof(pal_monitor));

	// Define a point at the origin (0, 0)
	POINT ptZero = { 0, 0 };

	// Get the handle to the primary monitor
	monitor->handle = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
	
	return monitor;

}

static void* platform_gl_get_proc_address(const char* proc) {
	 
	return wglGetProcAddress(proc);
}

void platform_begin_drawing() {

}

void platform_end_drawing(pal_window* window) {
	SwapBuffers(window->hdc);
}

#define MAX_RAW_INPUTS 16

// Handler function signatures
typedef void (*RawInputHandler)(const RAWINPUT*);
// Helper struct to hold reusable buffers

#define MAX_MOUSE_BUTTONS 64
#define MAX_BUTTON_CAPS 32

typedef struct {
    PHIDP_PREPARSED_DATA prep_data;
    HIDP_BUTTON_CAPS button_caps[MAX_BUTTON_CAPS];
    USHORT num_button_caps;
} MouseHIDBuffers;

MouseHIDBuffers hid_buffer = {0};

pal_bool InitMouseHIDBuffers(HANDLE device_handle) {
    UINT prep_size = 0;
    if (GetRawInputDeviceInfo(device_handle, RIDI_PREPARSEDDATA, NULL, &prep_size) == (UINT)-1)
        return FALSE;

    if (hid_buffer.prep_data) {
        HidD_FreePreparsedData(hid_buffer.prep_data);
        hid_buffer.prep_data = NULL;
    }

    hid_buffer.prep_data = (PHIDP_PREPARSED_DATA)malloc(prep_size);
    if (!hid_buffer.prep_data)
        return FALSE;

    if (GetRawInputDeviceInfo(device_handle, RIDI_PREPARSEDDATA, hid_buffer.prep_data, &prep_size) == (UINT)-1) {
        free(hid_buffer.prep_data);
        hid_buffer.prep_data = NULL;
        return FALSE;
    }

    hid_buffer.num_button_caps = MAX_BUTTON_CAPS;
    NTSTATUS status = HidP_GetButtonCaps(
        HidP_Input,
        hid_buffer.button_caps,
        &hid_buffer.num_button_caps,
        hid_buffer.prep_data
    );

    if (status != HIDP_STATUS_SUCCESS) {
        HidD_FreePreparsedData(hid_buffer.prep_data);
        free(hid_buffer.prep_data);
        hid_buffer.prep_data = NULL;
        return FALSE;
    }

    return TRUE;
}

pal_bool get_device_handle() {
    UINT device_count = 0;
    if (GetRawInputDeviceList(NULL, &device_count, sizeof(RAWINPUTDEVICELIST)) != 0 || device_count == 0)
        return FALSE;

    RAWINPUTDEVICELIST* device_list = (RAWINPUTDEVICELIST*)malloc(sizeof(RAWINPUTDEVICELIST) * device_count);
    if (!device_list)
        return FALSE;

    if (GetRawInputDeviceList(device_list, &device_count, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
        free(device_list);
        return FALSE;
    }

    pal_bool initialized = FALSE;
    for (UINT i = 0; i < device_count; ++i) {
        if (device_list[i].dwType == RIM_TYPEMOUSE)
            continue; // skip traditional mice

        RID_DEVICE_INFO info = {0};
        info.cbSize = sizeof(info);
        UINT size = sizeof(info);
        if (GetRawInputDeviceInfo(device_list[i].hDevice, RIDI_DEVICEINFO, &info, &size) == (UINT)-1)
            continue;

        if (info.dwType == RIM_TYPEHID && info.hid.usUsagePage == 0x01 && info.hid.usUsage == 0x02) {
            if (InitMouseHIDBuffers(device_list[i].hDevice)) {
                initialized = TRUE;
                break;
            }
        }
    }

    free(device_list);
    return initialized;
}

void FreeMouseHIDBuffers() {
    if (hid_buffer.prep_data) {
        HidD_FreePreparsedData(hid_buffer.prep_data);
        free(hid_buffer.prep_data);
        hid_buffer.prep_data = NULL;
    }
    hid_buffer.num_button_caps = 0;
}

void Win32HandleHidMouse(const RAWINPUT* raw) {
    if (raw->header.dwType != RIM_TYPEHID)
        return;

    input.mouse = (v2){ (float)raw->data.mouse.lLastX, (float)raw->data.mouse.lLastY };

    if (!hid_buffer.prep_data)
        return;

    for (int i = 0; i < MAX_MOUSE_BUTTONS; ++i)
        input.mouse_buttons[i] = 0;

    for (ULONG i = 0; i < hid_buffer.num_button_caps; ++i) {
        ULONG usage_count = MAX_MOUSE_BUTTONS;
        USAGE usages[MAX_MOUSE_BUTTONS] = {0};

        NTSTATUS status = HidP_GetUsages(
            HidP_Input,
            hid_buffer.button_caps[i].UsagePage,
            0,
            usages,
            &usage_count,
            hid_buffer.prep_data,
            (PCHAR)raw->data.hid.bRawData,
            raw->data.hid.dwSizeHid
        );

        if (status == HIDP_STATUS_SUCCESS || status == HIDP_STATUS_BUFFER_TOO_SMALL) {
            for (ULONG j = 0; j < usage_count; ++j) {
                USAGE usage_id = usages[j];
                if (usage_id < MAX_MOUSE_BUTTONS)
                    input.mouse_buttons[usage_id] = 1;
            }
        }
    }
}

void Win32HandleMouse(const RAWINPUT* raw) {
	LONG dx = raw->data.mouse.lLastX;
	LONG dy = raw->data.mouse.lLastY;
	USHORT buttons = raw->data.mouse.usButtonFlags;
	input.mouse = (v2){
		(float)dx,
		(float)dy,
	};
 
	for (int i = 0; i < 16; ++i) {
		uint16_t down = (buttons >> (i * 2)) & 1;
		uint16_t up = (buttons >> (i * 2 + 1)) & 1;
 
		// If down is 1, set to 1; if up is 1, set to 0; otherwise leave unchanged
		input.mouse_buttons[i] = (input.mouse_buttons[i] & ~up) | down;
	}
}

v2 platform_get_mouse_position(pal_window* window) {
	POINT cursor_pos = { 0 };
	GetCursorPos(&cursor_pos);

	ScreenToClient(window->hwnd, &cursor_pos);     // Convert to client-area coordinates
	return (v2) {
		(float)cursor_pos.x,
		(float)cursor_pos.y
	};
}

void Win32HandleKeyboard(const RAWINPUT* raw) {
	USHORT key = raw->data.keyboard.VKey;
	input.keys[key] = ~(raw->data.keyboard.Flags) & RI_KEY_BREAK; 
	//printf("Key %u %s\n", key, input.keys[key] ? "up" : "down"); // FOR DEBUGGING NOCHECKIN!

}

// Handles Gamepads, Joysticks, Steering wheels, etc...
void Win32HandleHID(const RAWINPUT* raw) {
	printf("%d", raw->data.hid.dwCount);
}

// Handler function table indexed by dwType (0 = mouse, 1 = keyboard, 2 = HID)
RawInputHandler Win32InputHandlers[3] = {
	Win32HandleMouse,      // RIM_TYPEMOUSE (0)
	Win32HandleKeyboard,   // RIM_TYPEKEYBOARD (1)
	Win32HandleHID        // RIM_TYPEHID (2) This is for joysticks, gamepads, and steering wheels.
};

int platform_register_raw_input_devices(pal_window* window) {
	RAWINPUTDEVICE rid[3];

	// 1. Keyboard
	rid[0].usUsagePage = 0x01; // Generic desktop controls
	rid[0].usUsage = 0x06;     // Keyboard
	rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY; // Receive input even when not focused
	rid[0].hwndTarget = window->hwnd;

	// 2. Mouse
	rid[1].usUsagePage = 0x01; // Generic desktop controls
	rid[1].usUsage = 0x02;     // Mouse
	rid[1].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
	rid[1].hwndTarget = window->hwnd;

	// 3. Joystick/Gamepad (Note: Not all controllers appear as HIDs)
	rid[2].usUsagePage = 0x01; // Generic desktop controls
	rid[2].usUsage = 0x04;     // Joystick
	rid[2].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
	rid[2].hwndTarget = window->hwnd;

	if (!RegisterRawInputDevices(rid, 3, sizeof(RAWINPUTDEVICE))) {
		DWORD error = GetLastError();
		printf("RegisterRawInputDevices failed. Error code: %lu\n", error);
		return -1;
	}

	printf("Raw input devices registered successfully.\n");
	return 0;
}

#define RAW_INPUT_BUFFER_CAPACITY (64 * 1024) // 8 KB

static BYTE g_rawInputBuffer[RAW_INPUT_BUFFER_CAPACITY];

static int platform_get_raw_input_buffer() {
	UINT bufferSize = RAW_INPUT_BUFFER_CAPACITY;
	UINT inputEventCount = GetRawInputBuffer((PRAWINPUT)g_rawInputBuffer, &bufferSize, sizeof(RAWINPUTHEADER));

	PRAWINPUT raw = (PRAWINPUT)g_rawInputBuffer;
	for (UINT i = 0; i < inputEventCount; ++i) {
		UINT type = raw->header.dwType;
        if (type == RIM_TYPEMOUSE) {
            Win32HandleMouse(raw);
        }
        else if (type == RIM_TYPEKEYBOARD) {
            Win32HandleKeyboard(raw);
        }
        else {
            Win32HandleHidMouse(raw);
        }
		raw = NEXTRAWINPUTBLOCK(raw);
	}
	return 0;
}

int platform_init_sound() {
	int hr;

	// Initialize COM (needed for XAudio2)
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		return hr;
	}

	// Initialize XAudio2 engine
	hr = XAudio2Create(&g_xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) {
		return hr;
	}

	// Create mastering voice
	hr = g_xaudio2->lpVtbl->CreateMasteringVoice(g_xaudio2, &g_mastering_voice,
		XAUDIO2_DEFAULT_CHANNELS,
		XAUDIO2_DEFAULT_SAMPLERATE,
		0, NULL, NULL, AudioCategory_GameEffects);

	return hr;
}

// Helper function to compare GUIDs
static int is_equal_guid(const GUID* a, const GUID* b) {
	return memcmp(a, b, sizeof(GUID)) == 0;
}

static int platform_play_sound(const Sound* sound, float volume) {
	if (!g_xaudio2 || !g_mastering_voice) {
		return E_FAIL;
	}

	static const GUID KSDATAFORMAT_SUBTYPE_PCM = {
		0x00000001, 0x0000, 0x0010,
		{ 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
	};

	static const GUID KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {
		0x00000003, 0x0000, 0x0010,
		{ 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
	};

	IXAudio2SourceVoice* source_voice = NULL;

	WAVEFORMATEXTENSIBLE wfex = { 0 };

	wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wfex.Format.nChannels = sound->channels;
	wfex.Format.nSamplesPerSec = sound->sampleRate;
	wfex.Format.wBitsPerSample = sound->bitsPerSample;
	wfex.Format.nBlockAlign = (wfex.Format.nChannels * wfex.Format.wBitsPerSample) / 8;
	wfex.Format.nAvgBytesPerSec = wfex.Format.nSamplesPerSec * wfex.Format.nBlockAlign;
	wfex.Format.cbSize = 22;

	wfex.Samples.wValidBitsPerSample = sound->bitsPerSample;
	wfex.dwChannelMask = (sound->channels == 2) ? SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT : 0;
	wfex.SubFormat = (sound->bitsPerSample == 32)
		? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
		: KSDATAFORMAT_SUBTYPE_PCM;

	HRESULT hr = g_xaudio2->lpVtbl->CreateSourceVoice(
		g_xaudio2, &source_voice, (const WAVEFORMATEX*)&wfex,
		0, XAUDIO2_DEFAULT_FREQ_RATIO,
		NULL, NULL, NULL);
	if (FAILED(hr)) {
		return hr;
	}

	// Set the volume
	source_voice->lpVtbl->SetVolume(source_voice, volume, 0);

	XAUDIO2_BUFFER buffer = {
		.AudioBytes = (UINT32)sound->dataSize,
		.pAudioData = sound->data,
		.Flags = XAUDIO2_END_OF_STREAM
	};

	hr = source_voice->lpVtbl->SubmitSourceBuffer(source_voice, &buffer, NULL);
	if (FAILED(hr)) {
		source_voice->lpVtbl->DestroyVoice(source_voice);
		return hr;
	}

	hr = source_voice->lpVtbl->Start(source_voice, 0, 0);
	if (FAILED(hr)) {
		source_voice->lpVtbl->DestroyVoice(source_voice);
		return hr;
	}

	return S_OK;
}

void* platform_load_dynamic_library(char* dll) {
	HMODULE result = LoadLibraryA(dll);
	assert(result);
	return result;
}

void* platform_load_dynamic_function(void* dll, char* func_name) {
	FARPROC proc = GetProcAddress(dll, func_name);
	assert(proc);
	return (void*)proc;
}

uint8_t platform_free_dynamic_library(void* dll) {
	uint8_t free_result = FreeLibrary(dll);
	assert(free_result);
	return (uint8_t)free_result;
}


// Called from Public API
void platform_gamepad_init(HWND window);
void platform_gamepad_shutdown();
int platform_gamepad_get_count();
pal_bool platform_gamepad_get_state(int index, pal_gamepad_state* out_state);
pal_bool platform_gamepad_set_vibration(int index, float left_motor, float right_motor);
pal_bool platform_gamepad_set_led(int index, uint8_t r, uint8_t g, uint8_t b);
pal_bool platform_gamepad_load_mappings(const char* filename);
// -----------------------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------------------

// Internal Types

// DualSense constants
#define DUALSHOCK_VENDOR_ID  0x054C
#define DUALSHOCK_PRODUCT_ID 0x0CE6
#define DS_INPUT_REPORT_ID 0x01
#define DS_OUTPUT_REPORT_ID 0x31
#define DS_FEATURE_REPORT_ID 0x05

#define DS_BUTTON_MASK_SQUARE     0x10
#define DS_BUTTON_MASK_CROSS      0x20
#define DS_BUTTON_MASK_CIRCLE     0x40
#define DS_BUTTON_MASK_TRIANGLE   0x80
#define DS_BUTTON_MASK_L1         0x01
#define DS_BUTTON_MASK_R1         0x02
#define DS_BUTTON_MASK_L2         0x04
#define DS_BUTTON_MASK_R2         0x08
#define DS_BUTTON_MASK_SHARE      0x10
#define DS_BUTTON_MASK_OPTIONS    0x20
#define DS_BUTTON_MASK_L3         0x40
#define DS_BUTTON_MASK_R3         0x80
#define DS_BUTTON_MASK_PS         0x01
#define DS_BUTTON_MASK_TOUCHPAD   0x02

// Internal Functions new
static pal_bool win32_is_xbox_controller(uint16_t vid, uint16_t pid) {
    return (vid == 0x045E && (pid == 0x02FD || pid == 0x02FF || pid == 0x028E));
}

static pal_bool win32_is_dualsense(uint16_t vid, uint16_t pid) {
    return (vid == DUALSHOCK_VENDOR_ID && pid == DUALSHOCK_PRODUCT_ID);
}

static void win32_start_dualsense_async_read(int index) {
    if (!win32_gamepad_ctx.win32_dualsense_devices[index].connected) return;

    HANDLE hDevice = win32_gamepad_ctx.win32_dualsense_devices[index].handle;
    win32_dualsense_state* state = &win32_gamepad_ctx.win32_dualsense_devices[index].state;
    
    memset(state->report, 0, sizeof(state->report));
    ResetEvent(state->overlapped.hEvent);
    
    if (!ReadFile(hDevice, state->report, sizeof(state->report), NULL, &state->overlapped)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            win32_gamepad_ctx.win32_dualsense_devices[index].connected = FALSE;
            CloseHandle(hDevice);
            CloseHandle(state->overlapped.hEvent);
        }
    }
}

static void win32_update_dualsense(int index) {
    if (!win32_gamepad_ctx.win32_dualsense_devices[index].connected) return;

    win32_dualsense_state* state = &win32_gamepad_ctx.win32_dualsense_devices[index].state;
    
    DWORD bytesRead;
    if (GetOverlappedResult(win32_gamepad_ctx.win32_dualsense_devices[index].handle, 
                          &state->overlapped, &bytesRead, FALSE)) {
        if (bytesRead > 0 && state->report[0] == DS_INPUT_REPORT_ID) {
            state->has_report = TRUE;
            win32_start_dualsense_async_read(index);
        }
    } else if (GetLastError() != ERROR_IO_INCOMPLETE) {
        win32_gamepad_ctx.win32_dualsense_devices[index].connected = FALSE;
        CloseHandle(win32_gamepad_ctx.win32_dualsense_devices[index].handle);
        CloseHandle(state->overlapped.hEvent);
    }
}

static void win32_dualsense_to_unified(int index, pal_gamepad_state* out) {
    if (!out || index < 0 || index >= PAL_MAX_GAMEPADS || 
        !win32_gamepad_ctx.win32_dualsense_devices[index].connected ||
        !win32_gamepad_ctx.win32_dualsense_devices[index].state.has_report) {
        return;
    }

    uint8_t* report = win32_gamepad_ctx.win32_dualsense_devices[index].state.report;

    // Buttons
    out->buttons.a = (report[7] & DS_BUTTON_MASK_CROSS) != 0;
    out->buttons.b = (report[7] & DS_BUTTON_MASK_CIRCLE) != 0;
    out->buttons.x = (report[7] & DS_BUTTON_MASK_SQUARE) != 0;
    out->buttons.y = (report[7] & DS_BUTTON_MASK_TRIANGLE) != 0;
    out->buttons.back = (report[8] & DS_BUTTON_MASK_SHARE) != 0;
    out->buttons.start = (report[8] & DS_BUTTON_MASK_OPTIONS) != 0;
    out->buttons.left_stick = (report[8] & DS_BUTTON_MASK_L3) != 0;
    out->buttons.right_stick = (report[8] & DS_BUTTON_MASK_R3) != 0;
    out->buttons.left_shoulder = (report[7] & DS_BUTTON_MASK_L1) != 0;
    out->buttons.right_shoulder = (report[7] & DS_BUTTON_MASK_R1) != 0;
    out->buttons.guide = (report[9] & DS_BUTTON_MASK_PS) != 0;
    out->buttons.touchpad_button = (report[9] & DS_BUTTON_MASK_TOUCHPAD) != 0;

    // D-pad
    uint8_t dpad = report[6] & 0x0F;
    out->buttons.dpad_up = (dpad == 7 || dpad == 0 || dpad == 1);
    out->buttons.dpad_right = (dpad == 1 || dpad == 2 || dpad == 3);
    out->buttons.dpad_down = (dpad == 3 || dpad == 4 || dpad == 5);
    out->buttons.dpad_left = (dpad == 5 || dpad == 6 || dpad == 7);

    // Axes
    out->axes.left_x = (report[1] - 127) / 127.0f;
    out->axes.left_y = (report[2] - 127) / 127.0f;
    out->axes.right_x = (report[3] - 127) / 127.0f;
    out->axes.right_y = (report[4] - 127) / 127.0f;

    // Triggers
    out->axes.left_trigger = report[5] / 255.0f;
    out->axes.right_trigger = report[6] / 255.0f;

    // Battery (0-10 scale in report[52])
    uint8_t battery = report[52] & 0x0F;
    out->battery_level = (float)battery / 10.0f;
    out->is_charging = (report[52] & 0x10) != 0;

    // Motion sensors
    out->accel_x = (int16_t)(report[16] << 8 | report[15]) / 8192.0f;
    out->accel_y = (int16_t)(report[18] << 8 | report[17]) / 8192.0f;
    out->accel_z = (int16_t)(report[20] << 8 | report[19]) / 8192.0f;
    out->gyro_x = (int16_t)(report[22] << 8 | report[21]) / 1024.0f;
    out->gyro_y = (int16_t)(report[24] << 8 | report[23]) / 1024.0f;
    out->gyro_z = (int16_t)(report[26] << 8 | report[25]) / 1024.0f;

    // Touchpad
    out->touchpad.touch_count = report[35] & 0x03;
    for (int i = 0; i < out->touchpad.touch_count && i < PAL_MAX_TOUCHES; i++) {
        int offset = 36 + (i * 9);
        out->touchpad.touches[i].id = report[offset] & 0x7F;
        out->touchpad.touches[i].x = ((report[offset + 2] & 0x0F) << 8 | report[offset + 1]) / 1920.0f;
        out->touchpad.touches[i].y = (report[offset + 3] << 4 | ((report[offset + 2] & 0xF0) >> 4)) / 1080.0f;
        out->touchpad.touches[i].down = !(report[offset] & 0x80);
    }

    // Metadata
    strncpy(out->name, "DualSense Controller", sizeof(out->name));
    out->vendor_id = DUALSHOCK_VENDOR_ID;
    out->product_id = DUALSHOCK_PRODUCT_ID;
    out->connected = TRUE;
    out->is_xinput = FALSE;
}

// Internal Functions

static pal_bool win32_dualsense_set_vibration(int index, float left_motor, float right_motor) {
    if (index < 0 || index >= PAL_MAX_GAMEPADS || 
        !win32_gamepad_ctx.win32_dualsense_devices[index].connected) {
        return FALSE;
    }

    HANDLE handle = win32_gamepad_ctx.win32_dualsense_devices[index].handle;
    
    uint8_t output_report[48] = {0};
    output_report[0] = DS_OUTPUT_REPORT_ID;
    output_report[1] = 0xFF; // Flags
    output_report[3] = (uint8_t)(right_motor * 255);
    output_report[4] = (uint8_t)(left_motor * 255);

    DWORD bytesWritten;
    return WriteFile(handle, output_report, sizeof(output_report), &bytesWritten, NULL) && 
           bytesWritten == sizeof(output_report);
}

static void win32_handle_dualsense_connection(HANDLE hDevice) {
    UINT path_length = 0;
    if (GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, NULL, &path_length) == (UINT)-1) {
        return;
    }

    char* device_path = (char*)malloc(path_length);
    if (!device_path) return;

    if (GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, device_path, &path_length) == (UINT)-1) {
        free(device_path);
        return;
    }

    for (int i = 0; i < PAL_MAX_GAMEPADS; i++) {
        if (!win32_gamepad_ctx.win32_dualsense_devices[i].connected) {
            HANDLE hDualSense = CreateFileA(
                device_path,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                NULL);

            if (hDualSense != INVALID_HANDLE_VALUE) {
                win32_gamepad_ctx.win32_dualsense_devices[i].handle = hDualSense;
                win32_gamepad_ctx.win32_dualsense_devices[i].connected = TRUE;
                memset(&win32_gamepad_ctx.win32_dualsense_devices[i].state, 0,
                      sizeof(win32_dualsense_state));
                
                win32_gamepad_ctx.win32_dualsense_devices[i].state.overlapped.hEvent = 
                    CreateEvent(NULL, TRUE, FALSE, NULL);
                
                win32_start_dualsense_async_read(i);
                break;
            }
        }
    }

    free(device_path);
}

static void win32_handle_raw_input(HRAWINPUT raw_input) {
    uint8_t stack_buffer[256];
    RAWINPUT* raw = (RAWINPUT*)stack_buffer;
    UINT size = sizeof(stack_buffer);
    UINT result;

    result = GetRawInputData(raw_input, RID_INPUT, stack_buffer, &size, sizeof(RAWINPUTHEADER));
    
    if (result == (UINT)-1 && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        if (size <= sizeof(win32_gamepad_ctx.raw_input_buffer)) {
            raw = (RAWINPUT*)win32_gamepad_ctx.raw_input_buffer;
            result = GetRawInputData(raw_input, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));
            if (result == (UINT)-1) return;
        } else {
            return;
        }
    } else if (result == (UINT)-1) {
        return;
    }

    if (raw->header.dwType == RIM_TYPEHID && raw->header.hDevice != NULL) {
        RID_DEVICE_INFO dev_info = {0};
        dev_info.cbSize = sizeof(dev_info);
        UINT dev_info_size = sizeof(dev_info);
        
        if (GetRawInputDeviceInfo(raw->header.hDevice, RIDI_DEVICEINFO, &dev_info, &dev_info_size) > 0) {
            if (win32_is_dualsense(dev_info.hid.dwVendorId, dev_info.hid.dwProductId)) {
                win32_handle_dualsense_connection(raw->header.hDevice);
                return;
            }
        }

        for (int i = 0; i < PAL_MAX_GAMEPADS; i++) {
            if (win32_gamepad_ctx.win32_raw_devices[i].handle == raw->header.hDevice) {
                PHIDP_PREPARSED_DATA pp_data = win32_gamepad_ctx.win32_raw_devices[i].pp_data;
                PCHAR report = (PCHAR)raw->data.hid.bRawData;
                USHORT report_length = raw->data.hid.dwSizeHid;

                USAGE buttons[PAL_MAX_BUTTONS];
                ULONG button_length = win32_gamepad_ctx.win32_raw_devices[i].button_count;
                if (HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_BUTTON, 0, buttons, &button_length, 
                                  pp_data, report, report_length) == HIDP_STATUS_SUCCESS) {
                    for (int j = 0; j < win32_gamepad_ctx.win32_raw_devices[i].button_count; j++) {
                        win32_gamepad_ctx.win32_raw_devices[i].buttons[j].value = 0.0f;
                    }
                    for (ULONG j = 0; j < button_length; j++) {
                        if (buttons[j] <= win32_gamepad_ctx.win32_raw_devices[i].button_count) {
                            win32_gamepad_ctx.win32_raw_devices[i].buttons[buttons[j]-1].value = 1.0f;
                        }
                    }
                }

                for (int j = 0; j < win32_gamepad_ctx.win32_raw_devices[i].axis_count; j++) {
                    ULONG value;
                    if (HidP_GetUsageValue(HidP_Input, 
                                          win32_gamepad_ctx.win32_raw_devices[i].axes[j].usage >> 8,
                                          0, 
                                          win32_gamepad_ctx.win32_raw_devices[i].axes[j].usage & 0xFF,
                                          &value, 
                                          pp_data, 
                                          report, 
                                          report_length) == HIDP_STATUS_SUCCESS) {
                        float normalized = (2.0f * (value - win32_gamepad_ctx.win32_raw_devices[i].axes[j].min) / 
                                         (win32_gamepad_ctx.win32_raw_devices[i].axes[j].max - win32_gamepad_ctx.win32_raw_devices[i].axes[j].min)) - 1.0f;
                        win32_gamepad_ctx.win32_raw_devices[i].axes[j].value = 
                            win32_gamepad_ctx.win32_raw_devices[i].axes[j].inverted ? -normalized : normalized;
                    }
                }
                break;
            }
        }
    }
}

#if PAL_XINPUT_ENABLED
static void win32_update_xinput() {
    for (DWORD i = 0; i < 4; i++) {
        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            win32_gamepad_ctx.win32_xinput_connected[i] = TRUE;
            win32_gamepad_ctx.win32_xinput_states[i] = state;
        } else {
            win32_gamepad_ctx.win32_xinput_connected[i] = FALSE;
        }
    }
}

static void win32_xinput_to_unified(DWORD index, pal_gamepad_state* out) {
    XINPUT_STATE* xi = &win32_gamepad_ctx.win32_xinput_states[index];
    
    out->axes.left_x = fmaxf(-1.0f, xi->Gamepad.sThumbLX / 32767.0f);
    out->axes.left_y = fmaxf(-1.0f, xi->Gamepad.sThumbLY / 32767.0f);
    out->axes.right_x = fmaxf(-1.0f, xi->Gamepad.sThumbRX / 32767.0f);
    out->axes.right_y = fmaxf(-1.0f, xi->Gamepad.sThumbRY / 32767.0f);
    out->axes.left_trigger = xi->Gamepad.bLeftTrigger / 255.0f;
    out->axes.right_trigger = xi->Gamepad.bRightTrigger / 255.0f;
    
    out->buttons.a = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
    out->buttons.b = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
    out->buttons.x = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
    out->buttons.y = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
    out->buttons.back = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
    out->buttons.start = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
    out->buttons.left_stick = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
    out->buttons.right_stick = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
    out->buttons.left_shoulder = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
    out->buttons.right_shoulder = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
    out->buttons.dpad_up = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    out->buttons.dpad_down = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    out->buttons.dpad_left = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    out->buttons.dpad_right = (xi->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    
    snprintf(out->name, sizeof(out->name), "Xbox Controller %d", index + 1);
    out->vendor_id = 0x045E;
    out->product_id = (index < 2) ? 0x028E : 0x02FD;
    out->connected = TRUE;
    out->is_xinput = TRUE;
}
#endif // PAL_XINPUT_ENABLED

static uint8_t win32_parse_button(const char* token) {
    const struct { const char* sdl; uint8_t hid; } button_map[] = {
        {"a", 1}, {"b", 2}, {"x", 3}, {"y", 4},
        {"back", 5}, {"guide", 6}, {"start", 7},
        {"leftstick", 8}, {"rightstick", 9},
        {"leftshoulder", 10}, {"rightshoulder", 11},
        {"dpup", 12}, {"dpdown", 13}, {"dpleft", 14}, {"dpright", 15},
        {NULL, 0}
    };
    
    for (int i = 0; button_map[i].sdl; i++) {
        if (strcmp(token, button_map[i].sdl) == 0) {
            return button_map[i].hid;
        }
    }
    return 0;
}

static uint8_t win32_parse_axis(const char* token, pal_bool* inverted) {
    *inverted = FALSE;
    if (token[0] == '-') {
        *inverted = TRUE;
        token++;
    }
    
    const struct { const char* sdl; uint8_t hid; } axis_map[] = {
        {"leftx", 0x30}, {"lefty", 0x31},
        {"rightx", 0x32}, {"righty", 0x33},
        {"lefttrigger", 0x34}, {"righttrigger", 0x35},
        {NULL, 0}
    };
    
    for (int i = 0; axis_map[i].sdl; i++) {
        if (strcmp(token, axis_map[i].sdl) == 0) {
            return axis_map[i].hid;
        }
    }
    return 0;
}

pal_bool platform_gamepad_load_mappings(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return FALSE;
    
    char line[512];
    while (fgets(line, sizeof(line), file) && 
           win32_gamepad_ctx.win32_mapping_count < PAL_MAX_MAPPINGS) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        char* tokens[32];
        int token_count = 0;
        char* token = strtok(line, ",");
        while (token && token_count < 32) {
            tokens[token_count++] = token;
            token = strtok(NULL, ",");
        }
        
        if (token_count < 5) continue;
        
        win32_gamepad_mapping* map = &win32_gamepad_ctx.win32_mappings[win32_gamepad_ctx.win32_mapping_count++];
        memset(map, 0, sizeof(win32_gamepad_mapping));
        
        map->vendor_id = (uint16_t)strtoul(tokens[1], NULL, 16);
        map->product_id = (uint16_t)strtoul(tokens[2], NULL, 16);
        strncpy(map->name, tokens[3], sizeof(map->name) - 1);
        
        for (int i = 0; i < 15 && (i+4) < token_count; i++) {
            map->button_map[i] = win32_parse_button(tokens[i+4]);
        }
        
        for (int i = 0; i < 6 && (i+19) < token_count; i++) {
            pal_bool inverted;
            map->axis_map[i].usage = win32_parse_axis(tokens[i+19], &inverted);
            map->axis_map[i].inverted = inverted;
        }
    }
    
    fclose(file);
    return TRUE;
}

void platform_gamepad_init(pal_window* window) {
    memset(&win32_gamepad_ctx, 0, sizeof(win32_gamepad_ctx));
    win32_gamepad_ctx.win32_hwnd = window->hwnd;

    RAWINPUTDEVICE rid[2] = {
        {0x01, 0x05, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, window->hwnd},
        {0x01, 0x04, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, window->hwnd}
    };
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));

    win32_gamepad_ctx.win32_initialized = TRUE;
}

int platform_gamepad_get_count() {
    int count = 0;
    
#if PAL_XINPUT_ENABLED
    win32_update_xinput();
    for (int i = 0; i < 4; i++) {
        if (win32_gamepad_ctx.win32_xinput_connected[i]) count++;
    }
#endif

    for (int i = 0; i < PAL_MAX_GAMEPADS; i++) {
        if (win32_gamepad_ctx.win32_raw_devices[i].connected) count++;
        if (win32_gamepad_ctx.win32_dualsense_devices[i].connected) count++;
    }
    
    return count;
}

pal_bool platform_gamepad_get_state(int index, pal_gamepad_state* out_state) {
    if (!out_state) return FALSE;
    memset(out_state, 0, sizeof(pal_gamepad_state));
    
#if PAL_XINPUT_ENABLED
    // Check XInput devices first (indices 0-3)
    if (index < 4 && win32_gamepad_ctx.win32_xinput_connected[index]) {
        win32_xinput_to_unified(index, out_state);
        return TRUE;
    }
    // Adjust index for non-XInput devices
    index -= 4;
#endif

    // Check DualSense devices
    if (index >= 0 && index < PAL_MAX_GAMEPADS && 
        win32_gamepad_ctx.win32_dualsense_devices[index].connected) {
        win32_update_dualsense(index);
        win32_dualsense_to_unified(index, out_state);
        return TRUE;
    }
    
    // Check raw HID devices
    if (index >= 0 && index < PAL_MAX_GAMEPADS && 
        win32_gamepad_ctx.win32_raw_devices[index].connected) {
        
        // Find matching mapping
        const win32_gamepad_mapping* map = NULL;
        for (int i = 0; i < win32_gamepad_ctx.win32_mapping_count; i++) {
            if (win32_gamepad_ctx.win32_mappings[i].vendor_id == win32_gamepad_ctx.win32_raw_devices[index].vendor_id &&
                win32_gamepad_ctx.win32_mappings[i].product_id == win32_gamepad_ctx.win32_raw_devices[index].product_id) {
                map = &win32_gamepad_ctx.win32_mappings[i];
                break;
            }
        }
        
        // Use default mapping if none found
        if (!map) {
            // Default Xbox-like mapping for unrecognized devices
            static const win32_gamepad_mapping default_mapping = {
                0, 0, "Generic Gamepad",
                // Button map (matches XInput order)
                {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                // Axis map
                {
                    {0x30, FALSE}, // leftx
                    {0x31, TRUE},  // lefty (inverted)
                    {0x32, FALSE}, // rightx
                    {0x33, TRUE}, // righty (inverted)
                    {0x34, FALSE}, // lefttrigger
                    {0x35, FALSE}  // righttrigger
                }
            };
            map = &default_mapping;
        }
        
        // Map buttons
        for (int i = 0; i < 15; i++) {
            if (map->button_map[i] == 0) continue;
            
            pal_bool* dest = NULL;
            switch (i) {
                case 0: dest = &out_state->buttons.a; break;
                case 1: dest = &out_state->buttons.b; break;
                case 2: dest = &out_state->buttons.x; break;
                case 3: dest = &out_state->buttons.y; break;
                case 4: dest = &out_state->buttons.back; break;
                case 5: dest = &out_state->buttons.guide; break;
                case 6: dest = &out_state->buttons.start; break;
                case 7: dest = &out_state->buttons.left_stick; break;
                case 8: dest = &out_state->buttons.right_stick; break;
                case 9: dest = &out_state->buttons.left_shoulder; break;
                case 10: dest = &out_state->buttons.right_shoulder; break;
                case 11: dest = &out_state->buttons.dpad_up; break;
                case 12: dest = &out_state->buttons.dpad_down; break;
                case 13: dest = &out_state->buttons.dpad_left; break;
                case 14: dest = &out_state->buttons.dpad_right; break;
            }
            
            if (dest) {
                for (int j = 0; j < win32_gamepad_ctx.win32_raw_devices[index].button_count; j++) {
                    if (win32_gamepad_ctx.win32_raw_devices[index].buttons[j].usage == map->button_map[i]) {
                        *dest = (win32_gamepad_ctx.win32_raw_devices[index].buttons[j].value > 0.5f);
                        break;
                    }
                }
            }
        }
        
        // Map axes
        for (int i = 0; i < 6; i++) {
            if (map->axis_map[i].usage == 0) continue;
            
            float* dest = NULL;
            switch (i) {
                case 0: dest = &out_state->axes.left_x; break;
                case 1: dest = &out_state->axes.left_y; break;
                case 2: dest = &out_state->axes.right_x; break;
                case 3: dest = &out_state->axes.right_y; break;
                case 4: dest = &out_state->axes.left_trigger; break;
                case 5: dest = &out_state->axes.right_trigger; break;
            }
            
            if (dest) {
                for (int j = 0; j < win32_gamepad_ctx.win32_raw_devices[index].axis_count; j++) {
                    if (win32_gamepad_ctx.win32_raw_devices[index].axes[j].usage == map->axis_map[i].usage) {
                        float value = win32_gamepad_ctx.win32_raw_devices[index].axes[j].value;
                        *dest = map->axis_map[i].inverted ? -value : value;
                        break;
                    }
                }
            }
        }
        
        // Set metadata
        strncpy(out_state->name, win32_gamepad_ctx.win32_raw_devices[index].name, sizeof(out_state->name));
        out_state->vendor_id = win32_gamepad_ctx.win32_raw_devices[index].vendor_id;
        out_state->product_id = win32_gamepad_ctx.win32_raw_devices[index].product_id;
        out_state->connected = TRUE;
        out_state->is_xinput = FALSE;
        
        return TRUE;
    }
    
    return FALSE;
}
// Additional utility functions for device enumeration
static void win32_enumerate_devices() {
    UINT num_devices = 0;
    GetRawInputDeviceList(NULL, &num_devices, sizeof(RAWINPUTDEVICELIST));
    if (num_devices == 0) return;

    PRAWINPUTDEVICELIST devices = (PRAWINPUTDEVICELIST)malloc(num_devices * sizeof(RAWINPUTDEVICELIST));
    if (!devices) return;

    if (GetRawInputDeviceList(devices, &num_devices, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
        free(devices);
        return;
    }

    for (UINT i = 0; i < num_devices; i++) {
        if (devices[i].dwType == RIM_TYPEHID) {
            RID_DEVICE_INFO dev_info = {0};
            dev_info.cbSize = sizeof(dev_info);
            UINT dev_info_size = sizeof(dev_info);

            if (GetRawInputDeviceInfo(devices[i].hDevice, RIDI_DEVICEINFO, &dev_info, &dev_info_size) > 0) {
                // Check for DualSense
                if (win32_is_dualsense(dev_info.hid.dwVendorId, dev_info.hid.dwProductId)) {
                    win32_handle_dualsense_connection(devices[i].hDevice);
                    continue;
                }

                // Handle other HID devices
                for (int j = 0; j < PAL_MAX_GAMEPADS; j++) {
                    if (!win32_gamepad_ctx.win32_raw_devices[j].connected) {
                        char name[128] = {0};
                        UINT name_size = sizeof(name);
                        GetRawInputDeviceInfo(devices[i].hDevice, RIDI_DEVICENAME, name, &name_size);

                        HANDLE hDevice = CreateFileA(
                            name,
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);

                        if (hDevice != INVALID_HANDLE_VALUE) {
                            PHIDP_PREPARSED_DATA pp_data = NULL;
                            if (HidD_GetPreparsedData(hDevice, &pp_data)) {
                                HIDP_CAPS caps;
                                if (HidP_GetCaps(pp_data, &caps) == HIDP_STATUS_SUCCESS) {
                                    // Store basic device info
                                    win32_gamepad_ctx.win32_raw_devices[j].handle = hDevice;
                                    win32_gamepad_ctx.win32_raw_devices[j].pp_data = pp_data;
                                    win32_gamepad_ctx.win32_raw_devices[j].vendor_id = dev_info.hid.dwVendorId;
                                    win32_gamepad_ctx.win32_raw_devices[j].product_id = dev_info.hid.dwProductId;
                                    strncpy(win32_gamepad_ctx.win32_raw_devices[j].name, name, sizeof(win32_gamepad_ctx.win32_raw_devices[j].name) - 1);
                                    win32_gamepad_ctx.win32_raw_devices[j].name[sizeof(win32_gamepad_ctx.win32_raw_devices[j].name) - 1] = '\0';

                                    // Initialize buttons
                                    USHORT button_caps_length = caps.NumberInputButtonCaps;
                                    HIDP_BUTTON_CAPS* button_caps = (HIDP_BUTTON_CAPS*)malloc(button_caps_length * sizeof(HIDP_BUTTON_CAPS));
                                    if (button_caps) {
                                        if (HidP_GetButtonCaps(HidP_Input, button_caps, &button_caps_length, pp_data) == HIDP_STATUS_SUCCESS) {
                                            // Process all button caps, not just the first one
                                            for (USHORT btn_idx = 0; btn_idx < button_caps_length; btn_idx++) {
                                                if (button_caps[btn_idx].IsRange) {
                                                    USHORT btn_count = button_caps[btn_idx].Range.UsageMax - button_caps[btn_idx].Range.UsageMin + 1;
                                                    for (USHORT k = 0; k < btn_count && win32_gamepad_ctx.win32_raw_devices[j].button_count < PAL_MAX_BUTTONS; k++) {
                                                        win32_gamepad_ctx.win32_raw_devices[j].buttons[win32_gamepad_ctx.win32_raw_devices[j].button_count].usage = 
                                                            button_caps[btn_idx].Range.UsageMin + k;
                                                        win32_gamepad_ctx.win32_raw_devices[j].button_count++;
                                                    }
                                                } else {
                                                    if (win32_gamepad_ctx.win32_raw_devices[j].button_count < PAL_MAX_BUTTONS) {
                                                        win32_gamepad_ctx.win32_raw_devices[j].buttons[win32_gamepad_ctx.win32_raw_devices[j].button_count].usage = 
                                                            button_caps[btn_idx].NotRange.Usage;
                                                        win32_gamepad_ctx.win32_raw_devices[j].button_count++;
                                                    }
                                                }
                                            }
                                        }
                                        free(button_caps);
                                    }

                                    // Initialize axes
                                    USHORT value_caps_length = caps.NumberInputValueCaps;
                                    HIDP_VALUE_CAPS* value_caps = (HIDP_VALUE_CAPS*)malloc(value_caps_length * sizeof(HIDP_VALUE_CAPS));
                                    if (value_caps) {
                                        if (HidP_GetValueCaps(HidP_Input, value_caps, &value_caps_length, pp_data) == HIDP_STATUS_SUCCESS) {
                                            win32_gamepad_ctx.win32_raw_devices[j].axis_count = 0;
                                            for (int k = 0; k < value_caps_length && win32_gamepad_ctx.win32_raw_devices[j].axis_count < PAL_MAX_AXES; k++) {
                                                if (value_caps[k].UsagePage == 1) { // Generic Desktop Page
                                                    USAGE usage = value_caps[k].IsRange ? value_caps[k].Range.UsageMin : value_caps[k].NotRange.Usage;
                                                    
                                                    win32_gamepad_ctx.win32_raw_devices[j].axes[win32_gamepad_ctx.win32_raw_devices[j].axis_count].usage = 
                                                        (value_caps[k].UsagePage << 8) | usage;
                                                    win32_gamepad_ctx.win32_raw_devices[j].axes[win32_gamepad_ctx.win32_raw_devices[j].axis_count].min = 
                                                        value_caps[k].LogicalMin;
                                                    win32_gamepad_ctx.win32_raw_devices[j].axes[win32_gamepad_ctx.win32_raw_devices[j].axis_count].max = 
                                                        value_caps[k].LogicalMax;
                                                    win32_gamepad_ctx.win32_raw_devices[j].axes[win32_gamepad_ctx.win32_raw_devices[j].axis_count].inverted = FALSE;
                                                    win32_gamepad_ctx.win32_raw_devices[j].axis_count++;
                                                }
                                            }
                                        }
                                        free(value_caps);
                                    }

                                    win32_gamepad_ctx.win32_raw_devices[j].connected = TRUE;
                                    break;
                                } else {
                                    HidD_FreePreparsedData(pp_data);
                                    CloseHandle(hDevice);
                                }
                            } else {
                                CloseHandle(hDevice);
                            }
                        }
                    }
                }
            }
        }
    }

    free(devices);
}
// Callback for WM_INPUT_DEVICE_CHANGE
static void win32_handle_device_change(HANDLE hDevice, DWORD dwChange) {
    RID_DEVICE_INFO dev_info = {0};
    dev_info.cbSize = sizeof(dev_info);
    UINT dev_info_size = sizeof(dev_info);

    if (GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO, &dev_info, &dev_info_size) > 0) {
        if (dwChange == GIDC_ARRIVAL) {
            if (win32_is_dualsense(dev_info.hid.dwVendorId, dev_info.hid.dwProductId)) {
                win32_handle_dualsense_connection(hDevice);
            } else {
                // Handle other gamepad connections
                for (int i = 0; i < PAL_MAX_GAMEPADS; i++) {
                    if (!win32_gamepad_ctx.win32_raw_devices[i].connected) {
                        char name[128] = {0};
                        UINT name_size = sizeof(name);
                        GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, name, &name_size);

                        HANDLE hNewDevice = CreateFileA(
                            name,
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);

                        if (hNewDevice != INVALID_HANDLE_VALUE) {
                            PHIDP_PREPARSED_DATA pp_data = NULL;
                            if (HidD_GetPreparsedData(hNewDevice, &pp_data)) {
                                HIDP_CAPS caps;
                                if (HidP_GetCaps(pp_data, &caps) == HIDP_STATUS_SUCCESS) {
                                    win32_gamepad_ctx.win32_raw_devices[i].handle = hNewDevice;
                                    win32_gamepad_ctx.win32_raw_devices[i].pp_data = pp_data;
                                    win32_gamepad_ctx.win32_raw_devices[i].vendor_id = dev_info.hid.dwVendorId;
                                    win32_gamepad_ctx.win32_raw_devices[i].product_id = dev_info.hid.dwProductId;
                                    strncpy(win32_gamepad_ctx.win32_raw_devices[i].name, name, sizeof(win32_gamepad_ctx.win32_raw_devices[i].name) - 1);
                                    win32_gamepad_ctx.win32_raw_devices[i].connected = TRUE;
                                    break;
                                } else {
                                    HidD_FreePreparsedData(pp_data);
                                    CloseHandle(hNewDevice);
                                }
                            } else {
                                CloseHandle(hNewDevice);
                            }
                        }
                    }
                }
            }
        } else if (dwChange == GIDC_REMOVAL) {
            // Handle device disconnection
            for (int i = 0; i < PAL_MAX_GAMEPADS; i++) {
                if (win32_gamepad_ctx.win32_dualsense_devices[i].connected && 
                    win32_gamepad_ctx.win32_dualsense_devices[i].handle == hDevice) {
                    CancelIo(win32_gamepad_ctx.win32_dualsense_devices[i].handle);
                    if (win32_gamepad_ctx.win32_dualsense_devices[i].state.overlapped.hEvent) {
                        CloseHandle(win32_gamepad_ctx.win32_dualsense_devices[i].state.overlapped.hEvent);
                    }
                    CloseHandle(win32_gamepad_ctx.win32_dualsense_devices[i].handle);
                    win32_gamepad_ctx.win32_dualsense_devices[i].connected = FALSE;
                    break;
                }

                if (win32_gamepad_ctx.win32_raw_devices[i].connected && 
                    win32_gamepad_ctx.win32_raw_devices[i].handle == hDevice) {
                    HidD_FreePreparsedData(win32_gamepad_ctx.win32_raw_devices[i].pp_data);
                    CloseHandle(win32_gamepad_ctx.win32_raw_devices[i].handle);
                    win32_gamepad_ctx.win32_raw_devices[i].connected = FALSE;
                    break;
                }
            }
        }
    }
}

// Window procedure integration
LRESULT CALLBACK win32_gamepad_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INPUT:
            win32_handle_raw_input((HRAWINPUT)lParam);
            break;
            
        case WM_INPUT_DEVICE_CHANGE:
            win32_handle_device_change((HANDLE)lParam, (DWORD)wParam);
            break;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Extended initialization
void platform_gamepad_extended_init(HWND hwnd) {
    platform_gamepad_init(hwnd);
    win32_enumerate_devices();
    
    // Register for device change notifications
    RAWINPUTDEVICE rid = {
        0x01, // Usage Page (Generic Desktop)
        0x05, // Usage (Gamepad)
        RIDEV_DEVNOTIFY | RIDEV_INPUTSINK,
        hwnd
    };
    RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
}

// Battery information
pal_bool platform_gamepad_get_battery_info(int index, pal_gamepad_state* out) {
    if (!out) return FALSE;
    memset(out, 0, sizeof(pal_gamepad_state));
    
#if PAL_XINPUT_ENABLED
    if (index < 4 && win32_gamepad_ctx.win32_xinput_connected[index]) {
        XINPUT_BATTERY_INFORMATION battery;
        if (XInputGetBatteryInformation(index, BATTERY_DEVTYPE_GAMEPAD, &battery) == ERROR_SUCCESS) {
            out->battery_level = (float)battery.BatteryLevel / 100.0f;
            out->is_charging = (battery.BatteryType == BATTERY_TYPE_WIRED);
            return TRUE;
        }
        return FALSE;
    }
    index -= 4;
#endif

    if (index >= 0 && index < PAL_MAX_GAMEPADS && 
        win32_gamepad_ctx.win32_dualsense_devices[index].connected) {
        // DualSense battery reporting is in the input report
        if (win32_gamepad_ctx.win32_dualsense_devices[index].state.has_report) {
            uint8_t battery = win32_gamepad_ctx.win32_dualsense_devices[index].state.report[52] & 0x0F;
            out->battery_level = (float)battery / 10.0f; // 0-10 scale
            out->is_charging = (win32_gamepad_ctx.win32_dualsense_devices[index].state.report[52] & 0x10) != 0;
            return TRUE;
        }
    }
    
    // For other HID devices
    if (index >= 0 && index < PAL_MAX_GAMEPADS && 
        win32_gamepad_ctx.win32_raw_devices[index].connected) {
        // Generic HID battery query would go here
        // This requires HID usage page 0x84 (Power Device)
    }
    
    return FALSE;
}

// LED control for DualSense
pal_bool platform_gamepad_set_led(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= PAL_MAX_GAMEPADS || 
        !win32_gamepad_ctx.win32_dualsense_devices[index].connected) {
        return FALSE;
    }

    HANDLE handle = win32_gamepad_ctx.win32_dualsense_devices[index].handle;
    
    uint8_t output_report[48] = {0};
    output_report[0] = DS_OUTPUT_REPORT_ID;
    output_report[1] = 0xFF; // Flags
    output_report[44] = r;
    output_report[45] = g;
    output_report[46] = b;

    DWORD bytesWritten;
    return WriteFile(handle, output_report, sizeof(output_report), &bytesWritten, NULL) && 
           bytesWritten == sizeof(output_report);
}

// Touchpad support for DualSense
pal_bool platform_gamepad_get_touchpad(int index, pal_gamepad_state* out) {
    if (!out || index < 0 || index >= PAL_MAX_GAMEPADS || 
        !win32_gamepad_ctx.win32_dualsense_devices[index].connected ||
        !win32_gamepad_ctx.win32_dualsense_devices[index].state.has_report) {
        return FALSE;
    }

    uint8_t* report = win32_gamepad_ctx.win32_dualsense_devices[index].state.report;
    out->touchpad.touch_count = report[35] & 0x03;
    
    for (int i = 0; i < out->touchpad.touch_count; i++) {
        int offset = 36 + (i * 9);
        out->touchpad.touches[i].id = report[offset] & 0x7F;
        out->touchpad.touches[i].x = ((report[offset + 2] & 0x0F) << 8 | report[offset + 1]);
        out->touchpad.touches[i].y = report[offset + 3] << 4 | ((report[offset + 2] & 0xF0) >> 4);
        out->touchpad.touches[i].down = !(report[offset] & 0x80);
    }
    
    return TRUE;
}

// Motion sensors for DualSense
pal_bool platform_gamepad_get_motion(int index, pal_gamepad_state* out) {
    if (!out || index < 0 || index >= PAL_MAX_GAMEPADS || 
        !win32_gamepad_ctx.win32_dualsense_devices[index].connected ||
        !win32_gamepad_ctx.win32_dualsense_devices[index].state.has_report) {
        return FALSE;
    }

    uint8_t* report = win32_gamepad_ctx.win32_dualsense_devices[index].state.report;
    
    // Accelerometer (in G's)
    out->accel_x = (int16_t)(report[16] << 8 | report[15]) / 8192.0f;
    out->accel_y = (int16_t)(report[18] << 8 | report[17]) / 8192.0f;
    out->accel_z = (int16_t)(report[20] << 8 | report[19]) / 8192.0f;
    
    // Gyroscope (in degrees per second)
    out->gyro_x = (int16_t)(report[22] << 8 | report[21]) / 1024.0f;
    out->gyro_y = (int16_t)(report[24] << 8 | report[23]) / 1024.0f;
    out->gyro_z = (int16_t)(report[26] << 8 | report[25]) / 1024.0f;
    
    return TRUE;
}

void platform_gamepad_shutdown() {
    if (!win32_gamepad_ctx.win32_initialized) return;
    
    for (int i = 0; i < PAL_MAX_GAMEPADS; i++) {
        if (win32_gamepad_ctx.win32_dualsense_devices[i].connected) {
            CancelIo(win32_gamepad_ctx.win32_dualsense_devices[i].handle);
            if (win32_gamepad_ctx.win32_dualsense_devices[i].state.overlapped.hEvent) {
                CloseHandle(win32_gamepad_ctx.win32_dualsense_devices[i].state.overlapped.hEvent);
            }
            CloseHandle(win32_gamepad_ctx.win32_dualsense_devices[i].handle);
            win32_gamepad_ctx.win32_dualsense_devices[i].connected = FALSE;
        }
    }

    win32_gamepad_ctx.win32_initialized = FALSE;
}

pal_bool platform_gamepad_set_vibration(int index, float left_motor, float right_motor) {
#if PAL_XINPUT_ENABLED
    if (index < 4 && win32_gamepad_ctx.win32_xinput_connected[index]) {
        XINPUT_VIBRATION vib = {
            .wLeftMotorSpeed = (WORD)(left_motor * 65535),
            .wRightMotorSpeed = (WORD)(right_motor * 65535)
        };
        return XInputSetState(index, &vib) == ERROR_SUCCESS;
    }
    index -= 4;
#endif

    // DualSense vibration
    if (index >= 0 && index < PAL_MAX_GAMEPADS && 
        win32_gamepad_ctx.win32_dualsense_devices[index].connected) {
        return win32_dualsense_set_vibration(index, left_motor, right_motor);
    }
    
    return FALSE;
}
int platform_sleep(double milliseconds) {
    static int initialized = 0;
    static TIMECAPS tc;
    
    // Initialize timer resolution (once)
    if (!initialized) {
        if (timeGetDevCaps(&tc, sizeof(tc)) != TIMERR_NOERROR) {
            return -1; // Failed to get timer capabilities
        }
        if (timeBeginPeriod(tc.wPeriodMin) != TIMERR_NOERROR) {
            return -2; // Failed to set timer resolution
        }
        initialized = 1;
    }

    if (milliseconds <= 0.0) {
        return 0; // Success (no sleep needed)
    }

    HANDLE hTimer = CreateWaitableTimerEx(
        NULL,
        NULL,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS
    );
    
    if (!hTimer) {
        // Fallback to Sleep() if timer creation fails
        Sleep((DWORD)milliseconds);
        return 1; // Success but with fallback
    }

    // Convert to 100-nanosecond intervals (negative for relative time)
    LARGE_INTEGER li;
    li.QuadPart = -(LONGLONG)(milliseconds * 10000.0);
    
    if (!SetWaitableTimer(
        hTimer,
        &li,
        0, NULL, NULL, FALSE
    )) {
        CloseHandle(hTimer);
        return -3; // Failed to set timer
    }

    // Wait for timer with alertable wait (supports APCs)
    DWORD result = WaitForSingleObjectEx(hTimer, INFINITE, TRUE);
    CloseHandle(hTimer);
    
    if (result != WAIT_OBJECT_0) {
        return -4; // Wait failed
    }

    return 0; // Success
}
#endif // WIN32_PLATFORM_H