#include "main.h"
#include "rendering.h"
#include "resource_manager.h"
#include "world.h"
#include "entity.h"

#define NS_PER_SECOND 1000000000.0

Global_Variables globals;

static double nanoseconds_to_seconds(u64 nanoseconds) {
    double result = (double)nanoseconds / NS_PER_SECOND;
    return result;
}

static u64 seconds_to_nanoseconds(double seconds) {
    u64 result = (u64)(seconds * NS_PER_SECOND);
    return result;
}

static void update_time() {
    u64 now_time = os_get_time_nanoseconds();

    globals.time_info.delta_time       = now_time - globals.time_info.last_time;
    globals.time_info.real_world_time += globals.time_info.delta_time;

    globals.time_info.delta_time_seconds = nanoseconds_to_seconds(globals.time_info.delta_time);
    globals.time_info.accumulated_dt  += globals.time_info.delta_time;
}

static void init_shaders() {
    globals.shader_color = find_or_load_shader("color");
}

static void init_test_world() {
    globals.current_world = new World();
    init_world(globals.current_world, v2i(32, 18));
    
    Hero *hero = make_hero(globals.current_world);
    hero->position = v2(0, 0);
    hero->size     = v2(1, 1);
    hero->color    = v4(1, 0, 1, 1);
}

static void respond_to_input() {
    for (auto event : globals.events_this_frame) {
        switch (event.type) {
            case EVENT_TYPE_QUIT: {
                globals.should_quit_game = true;
            } break;
        }
    }

    for (auto record : globals.window_resizes) {
        if (record.window != globals.window) continue;

        globals.window_width  = record.width;
        globals.window_height = record.height;
    }
    globals.window_resizes.count = 0;
}

int main(int argc, char *argv[]) {
    os_init();

    globals.window_width  = 1600;
    globals.window_height = 900;
    globals.window = os_create_window(globals.window_width, globals.window_height, "Platformer!");
    if (!globals.window) return 1;
    if (!init_rendering(globals.window, globals.should_vsync)) return 1;
    init_resource_manager(); // !!! Need to init resource manager before shaders and textures !!!
    init_shaders();

    init_test_world();
    
    globals.time_info.last_time = os_get_time_nanoseconds();
    while (!globals.should_quit_game) {
        update_time();
        
        os_update_window_events();
        respond_to_input();

        while (globals.time_info.accumulated_dt >= seconds_to_nanoseconds(1.0 / (double)globals.time_info.fps_cap)) {
            update_world(globals.current_world, (float)(1.0 / (double)globals.time_info.fps_cap));
            draw_world(globals.current_world);
            globals.time_info.accumulated_dt -= seconds_to_nanoseconds(1.0 / (double)globals.time_info.fps_cap);
        }
        
        swap_buffers();

        do_hotloading();
    }
    
    return 0;
}
