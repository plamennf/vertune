#include "main.h"

#ifdef _WIN32

#ifdef WINGDIAPI
#undef WINGDIAPI
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#include "glex.h"

struct Window_State {
    HWND hwnd;
    WINDOWPLACEMENT prev_wp;
};

static Array <Window_State> created_window_states;

static Window_State *get_window_state(HWND hwnd) {
    for (int i = 0; i < created_window_states.count; i++) {
        Window_State *state = &created_window_states[i];
        if (state->hwnd == hwnd) {
            return state;
        }
    }

    Window_State *state = created_window_states.add();
    state->hwnd    = hwnd;
    state->prev_wp = {};
    return state;
}

#define WINDOW_CLASS_NAME L"PlatformerWin32WindowClass"
static bool window_class_initted;

static LARGE_INTEGER global_perf_freq;
static u64 nanoseconds_per_tick;

static bool shift_state;
static bool ctrl_state;
static bool alt_state;

#ifdef __cplusplus
extern "C" {
#endif

	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#ifdef __cplusplus
}
#endif

#define LoadGLFunc(func) func = reinterpret_cast <decltype(func)>(wglGetProcAddress(#func))

#define WGL_CONTEXT_DEBUG_BIT_ARB         0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB       0x2093
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB  0x20A9
#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042
#define WGL_NUMBER_PIXEL_FORMATS_ARB      0x2000
#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_DRAW_TO_BITMAP_ARB            0x2002
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_NEED_PALETTE_ARB              0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB       0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB        0x2006
#define WGL_SWAP_METHOD_ARB               0x2007
#define WGL_NUMBER_OVERLAYS_ARB           0x2008
#define WGL_NUMBER_UNDERLAYS_ARB          0x2009
#define WGL_TRANSPARENT_ARB               0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB     0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB   0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB    0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB   0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB   0x203B
#define WGL_SHARE_DEPTH_ARB               0x200C
#define WGL_SHARE_STENCIL_ARB             0x200D
#define WGL_SHARE_ACCUM_ARB               0x200E
#define WGL_SUPPORT_GDI_ARB               0x200F
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_STEREO_ARB                    0x2012
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_RED_BITS_ARB                  0x2015
#define WGL_RED_SHIFT_ARB                 0x2016
#define WGL_GREEN_BITS_ARB                0x2017
#define WGL_GREEN_SHIFT_ARB               0x2018
#define WGL_BLUE_BITS_ARB                 0x2019
#define WGL_BLUE_SHIFT_ARB                0x201A
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_ALPHA_SHIFT_ARB               0x201C
#define WGL_ACCUM_BITS_ARB                0x201D
#define WGL_ACCUM_RED_BITS_ARB            0x201E
#define WGL_ACCUM_GREEN_BITS_ARB          0x201F
#define WGL_ACCUM_BLUE_BITS_ARB           0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB          0x2021
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_AUX_BUFFERS_ARB               0x2024
#define WGL_NO_ACCELERATION_ARB           0x2025
#define WGL_GENERIC_ACCELERATION_ARB      0x2026
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_SWAP_EXCHANGE_ARB             0x2028
#define WGL_SWAP_COPY_ARB                 0x2029
#define WGL_SWAP_UNDEFINED_ARB            0x202A
#define WGL_TYPE_RGBA_ARB                 0x202B
#define WGL_TYPE_COLORINDEX_ARB           0x202C

//static BOOL(*wglMakeContextCurrentARB)(HDC hDrawDC, HDC hReadDC, HGLRC hglrc);
static HGLRC(*wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);
static BOOL(*wglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
static BOOL(*wglSwapIntervalEXT)(int interval);

void os_init() {
    timeBeginPeriod(1);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

static Window_Resize_Record *get_window_resize_record(HWND hwnd) {
    for (int i = 0; i < globals.window_resizes.count; i++) {
        auto record = &globals.window_resizes[i];
        if (record->window == hwnd) return record;
    }

    Window_Resize_Record *record = globals.window_resizes.add();
    record->window = hwnd;
    return record;
}

static LRESULT CALLBACK MyWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY: {
            Event event;
            event.type = EVENT_TYPE_QUIT;
            globals.events_this_frame.add(event);
        } break;

        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            int width  = rect.right  - rect.left;
            int height = rect.bottom - rect.top;

            auto record    = get_window_resize_record(hwnd);
            record->width  = width;
            record->height = height;
        } break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            int key_code  = (int)wparam;
            bool is_down  = (lparam & (1 << 31)) == 0;
            bool was_down = (lparam & (1 << 30)) == 1;

            switch (key_code) {
                case KEY_SHIFT: {
                    shift_state = is_down;
                } break;

                case KEY_CTRL: {
                    ctrl_state = is_down;
                } break;

                case KEY_ALT: {
                    alt_state = is_down;
                } break;
            }
            
            Event event;
            event.type           = EVENT_TYPE_KEYBOARD;
            event.is_key_repeat  = is_down && was_down;
            event.key_pressed    = is_down;
            event.key_code       = key_code;
            event.shift_down     = shift_state;
            event.ctrl_down      = ctrl_state;
            event.alt_down       = alt_state;
            globals.events_this_frame.add(event);

            if (alt_state && is_down) {
                if (key_code == KEY_F4) {
                    Event event;
                    event.type = EVENT_TYPE_QUIT;
                    globals.events_this_frame.add(event);
                } else if (key_code == KEY_ENTER) {
                    os_window_toggle_fullscreen(hwnd);
                }
            }
        } break;

        default: {
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
    }

    return 0;
}

static void init_window_class() {
    WNDCLASSEXW wc = {};

    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = MyWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hIcon = LoadIconW(NULL, L"APPICON");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = static_cast <HBRUSH>(GetStockObject(WHITE_BRUSH));

    wc.lpszMenuName = NULL;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (RegisterClassExW(&wc) == 0) {
        logprintf("Failure in init_window_class(): RegisterClassExW returned 0.\n");
        return;
    }

    window_class_initted = true;
}

Window_Type os_create_window(int width, int height, char *title) {
    if (!window_class_initted) {
        init_window_class();
        if (!window_class_initted) return NULL;
    }

    if (width == -1 || height == -1) {
        HWND desktop = GetDesktopWindow();
        RECT desktop_rect;
        GetWindowRect(desktop, &desktop_rect);

        int desktop_width  = desktop_rect.right - desktop_rect.left;
        int desktop_height = desktop_rect.bottom - desktop_rect.top;

        width  = (int)((double)desktop_width  * 2.0/3.0);
        height = (int)((double)desktop_height * 2.0/3.0);
    }

    if (width <= 0) {
        logprintf("[create_window] width can't be <= 0.\n");
        return NULL;
    }
    if (height <= 0) {
        logprintf("[create_window] height can't be <= 0.\n");
        return NULL;
    }
    
    DWORD style = WS_OVERLAPPEDWINDOW;

    RECT wr = {};
    wr.right = width;
    wr.bottom = height;
    AdjustWindowRect(&wr, style, FALSE);

    int window_width = wr.right - wr.left;
    int window_height = wr.bottom - wr.top;

    wchar_t wide_title[4096];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, ArrayCount(wide_title));
    Window_Type window = CreateWindowExW(0, WINDOW_CLASS_NAME, wide_title,
                                         style, 0, 0, window_width, window_height,
                                         NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (window == NULL) {
        logprintf("Error in create_window: CreateWindowExW returned 0.\n");
        return NULL;
    }
    
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi);
    
    int mwwt = mi.rcWork.right - mi.rcWork.left;
    int mhwt = mi.rcWork.bottom - mi.rcWork.top;

    int wx = mi.rcWork.left + ((mwwt - window_width) / 2);
    int wy = mi.rcWork.top + ((mhwt - window_height) / 2);

    SetWindowPos(window, HWND_TOP, wx, wy, 0, 0, SWP_NOSIZE);
    
    UpdateWindow(window);
    ShowWindow(window, SW_SHOWDEFAULT);

    return window;
}

void os_update_window_events() {
    globals.events_this_frame.count = 0;

    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void os_window_toggle_fullscreen(Window_Type window) {
    Window_State *state = get_window_state(window);
    assert(state);

    state->prev_wp.length = sizeof(WINDOWPLACEMENT);

    DWORD style = GetWindowLongW(window, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = {sizeof(mi)};
        if (GetWindowPlacement(window, &state->prev_wp) &&
            GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongW(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLongW(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &state->prev_wp);
        SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    RECT rect;
    GetClientRect(window, &rect);
    int width  = rect.right  - rect.left;
    int height = rect.bottom - rect.top;

    auto record    = get_window_resize_record(window);
    record->width  = width;
    record->height = height;
}

void *os_create_opengl_context(Window_Type window, int version_major, int version_minor, bool core_profile) {
    // Get wgl functions
    {
        HWND dummy = CreateWindowExW(0, L"STATIC", L"DummyWindow", WS_OVERLAPPED,
                                 0, 0, 0, 0, NULL, NULL, NULL, NULL);
        if (!dummy) {
            logprintf("Failed to create the dummy opengl window!\n");
            return NULL;
        }
        HDC dc = GetDC(dummy);
        
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;

        int format = ChoosePixelFormat(dc, &pfd);
        if (!format) {
            logprintf("Failed to choose the dummy pixel format!\n");
        }

        if (DescribePixelFormat(dc, format, sizeof(pfd), &pfd) == 0) {
            logprintf("Failed to describe the dummy pixel format!\n");
            return NULL;
        }
        
        if (SetPixelFormat(dc, format, &pfd) == FALSE) {
            logprintf("Failed to set the dummy pixel format!\n");
            return NULL;
        }

        HGLRC rc = wglCreateContext(dc);
        if (!rc) {
            logprintf("Failed to create the dummy opengl context!\n");
            return NULL;
        }
        
        if (!wglMakeCurrent(dc, rc)) {
            logprintf("Failed to set the dummy opengl context!\n");
            return NULL;
        }

        LoadGLFunc(wglChoosePixelFormatARB);
        LoadGLFunc(wglCreateContextAttribsARB);
        LoadGLFunc(wglSwapIntervalEXT);

        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(rc);
        ReleaseDC(dummy, dc);
        DestroyWindow(dummy);
    }
    
    // Set pixel format for OpenGL context
    HDC dc = GetDC(window);

    {
        int attrib[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,     24,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,

            // uncomment for sRGB framebuffer, from WGL_ARB_framebuffer_sRGB extension
            // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_framebuffer_sRGB.txt
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
        
            // uncomment for multisampeld framebuffer, from WGL_ARB_multisample extension
            // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_multisample.txt
            // WGL_SAMPLE_BUFFERS_ARB, 1,
            // WGL_SAMPLES_ARB,        4, // 4x MSAA

            0,
        };

        int format;
        UINT formats;
        wglChoosePixelFormatARB(dc, attrib, NULL, 1, &format, &formats);
        if (!formats) {
            logprintf("Failed to choose a pixel format!\n");
            return NULL;
        }
        
        PIXELFORMATDESCRIPTOR desc = {};
        desc.nSize = sizeof(desc);
        if (DescribePixelFormat(dc, format, sizeof(desc), &desc) == 0) {
            logprintf("Failed to describe the pixel format!\n");
            return NULL;
        }
        if (SetPixelFormat(dc, format, &desc) == FALSE) {
            logprintf("Failed to set the pixel format!\n");
            return NULL;
        }
    }

    // Create modern OpenGL context
    {
        int attrib[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, version_major,
            WGL_CONTEXT_MINOR_VERSION_ARB, version_minor,
            WGL_CONTEXT_PROFILE_MASK_ARB,  core_profile ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#ifndef NDEBUG
            // ask for debug context for non "Release" builds
            // this is so we can enable debug callback
            //WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
            0,
        };

        HGLRC rc = wglCreateContextAttribsARB(dc, NULL, attrib);
        if (!rc) {
            logprintf("Failed to create opengl context!\n");
            return NULL;
        }
        
        if (!wglMakeCurrent(dc, rc)) {
            logprintf("Failed to set opengl context!\n");
            return NULL;
        }
        
        return rc;
    }
}

bool os_opengl_set_vsync(bool vsync) {
    if (!wglSwapIntervalEXT) return false;

    wglSwapIntervalEXT(vsync ? 1 : 0);
    
    return true;
}

void os_opengl_swap_buffers(Window_Type window) {
    HDC dc = GetDC(window);
    SwapBuffers(dc);
}

static void to_windows_filepath(char *filepath, wchar_t *wide_filepath, int wide_filepath_size) {
    MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wide_filepath, wide_filepath_size);

    for (wchar_t *at = wide_filepath; *at; at++) {
        if (*at == L'/') {
            *at = L'\\';
        }
    }
}

static void to_normal_filepath(wchar_t *wide_filepath, char *filepath, int filepath_size) {
    WideCharToMultiByte(CP_UTF8, 0, wide_filepath, -1, filepath, filepath_size, NULL, 0);

    for (char *at = filepath; *at; at++) {
        if (*at == '\\') {
            *at = '/';
        }
    }
}

bool os_file_exists(char *filepath) {
    wchar_t wide_filepath[4096];
    to_windows_filepath(filepath, wide_filepath, ArrayCount(wide_filepath));

    DWORD attrib = GetFileAttributesW(wide_filepath);

    return (attrib != INVALID_FILE_ATTRIBUTES && 
            !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

u64 os_get_time_nanoseconds() {
    if (!global_perf_freq.QuadPart) {
        QueryPerformanceFrequency(&global_perf_freq);
        nanoseconds_per_tick = 1000000000 / global_perf_freq.QuadPart;
    }
    
    LARGE_INTEGER perf_counter;
    QueryPerformanceCounter(&perf_counter);
    
    return perf_counter.QuadPart * nanoseconds_per_tick;
}

#endif
