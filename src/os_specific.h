#pragma once

#ifdef _WIN32
typedef struct HWND__ *Window_Type;
#endif

enum Event_Type {
    EVENT_TYPE_UNKNOWN,
    EVENT_TYPE_QUIT,
};

struct Event {
    Event_Type type = EVENT_TYPE_UNKNOWN;
};

struct Window_Resize_Record {
    Window_Type window;
    int width;
    int height;
};

void os_init();

Window_Type os_create_window(int width, int height, char *title);
void os_update_window_events();

void *os_create_opengl_context(Window_Type window, int version_major, int version_minor, bool core_profile);
bool os_opengl_set_vsync(bool vsync);
void os_opengl_swap_buffers(Window_Type window);

bool os_file_exists(char *filepath);

u64 os_get_time_nanoseconds();
