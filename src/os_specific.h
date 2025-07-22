#pragma once

#ifdef _WIN32
typedef struct HWND__ *Window_Type;
#endif

enum Key_Code {
    KEY_UNKNOWN,

    MOUSE_BUTTON_LEFT   = 0x01,
    MOUSE_BUTTON_RIGHT  = 0x02,
    MOUSE_BUTTON_MIDDLE = 0x04,

    KEY_BACKSPACE = 0x08,
    KEY_TAB       = 0x09,

    KEY_ENTER     = 0x0D,

    KEY_SHIFT     = 0x10,
    KEY_CTRL      = 0x11,
    KEY_ALT       = 0x12,
    KEY_CAPS_LOCK = 0x14,

    KEY_ESCAPE    = 0x1B,
    KEY_SPACEBAR  = 0x20,
    KEY_PAGE_UP   = 0x21,
    KEY_PAGE_DOWN = 0x22,
    KEY_END       = 0x23,
    KEY_HOME      = 0x24,

    KEY_LEFT_ARROW  = 0x25,
    KEY_UP_ARROW    = 0x26,
    KEY_RIGHT_ARROW = 0x27,
    KEY_DOWN_ARROW  = 0x28,

    KEY_PRINT_SCREEN = 0x2C,
    KEY_INSERT       = 0x2D,
    KEY_DELETE       = 0x2E,

    // 0-9 and A-Z correspond to their ascii values.

    KEY_NP0 = 0x60,
    KEY_NP1 = 0x61,
    KEY_NP2 = 0x62,
    KEY_NP3 = 0x63,
    KEY_NP4 = 0x64,
    KEY_NP5 = 0x65,
    KEY_NP6 = 0x66,
    KEY_NP7 = 0x67,
    KEY_NP8 = 0x68,
    KEY_NP9 = 0x69,
    KEY_NP_MULTIPLY  = 0x6A,
    KEY_NP_ADD       = 0x6B,
    KEY_NP_SEPARATOR = 0x6C,
    KEY_NP_SUBTRACT  = 0x6D,
    KEY_NP_DECIMAL   = 0x6E,
    KEY_NP_DIVIDE    = 0x6F,

    KEY_F1 = 0x70,
    KEY_F2 = 0x71,
    KEY_F3 = 0x72,
    KEY_F4 = 0x73,
    KEY_F5 = 0x74,
    KEY_F6 = 0x75,
    KEY_F7 = 0x76,
    KEY_F8 = 0x77,
    KEY_F9 = 0x78,
    KEY_F10 = 0x79,
    KEY_F11 = 0x7A,
    KEY_F12 = 0x7B,
    KEY_F13 = 0x7C,
    KEY_F14 = 0x7D,
    KEY_F15 = 0x7E,
    KEY_F16 = 0x7F,
    KEY_F17 = 0x80,
    KEY_F18 = 0x81,
    KEY_F19 = 0x82,
    KEY_F20 = 0x83,
    KEY_F21 = 0x84,
    KEY_F22 = 0x85,
    KEY_F23 = 0x86,
    KEY_F24 = 0x87,

    KEY_NUMLOCK     = 0x90,
    KEY_SCROLL_LOCK = 0x91,

    KEY_COUNT,
};

enum Event_Type {
    EVENT_TYPE_UNKNOWN,
    EVENT_TYPE_QUIT,
    EVENT_TYPE_KEYBOARD,
};

struct Event {
    Event_Type type = EVENT_TYPE_UNKNOWN;

    bool is_key_repeat;
    bool key_pressed;
    int key_code;
    bool shift_down;
    bool ctrl_down;
    bool alt_down;
};

struct Window_Resize_Record {
    Window_Type window;
    int width;
    int height;
};

void os_init();

Window_Type os_create_window(int width, int height, char *title);
void os_update_window_events();
void os_window_toggle_fullscreen(Window_Type window);

void *os_create_opengl_context(Window_Type window, int version_major, int version_minor, bool core_profile);
bool os_opengl_set_vsync(bool vsync);
void os_opengl_swap_buffers(Window_Type window);

bool os_file_exists(char *filepath);

u64 os_get_time_nanoseconds();
