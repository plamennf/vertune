#include "main.h"
#include "rendering.h"
#include "resource_manager.h"
#include "world.h"
#include "entity.h"
#include "tilemap.h"
#include "camera.h"

#define NS_PER_SECOND 1000000000.0

Global_Variables globals;

struct Key_State {
    bool is_down;
    bool was_down;
    bool changed;
};

static Key_State key_states[KEY_COUNT];

bool is_key_down(int key_code) {
    return key_states[key_code].is_down;
}

bool is_key_pressed(int key_code) {
    return key_states[key_code].is_down && key_states[key_code].changed;
}

bool was_key_just_released(int key_code) {
    return key_states[key_code].was_down && !key_states[key_code].is_down;
}

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

    globals.time_info.last_time = now_time;
}

static void init_shaders() {
    globals.shader_color = find_or_load_shader("color");
}

static void init_framebuffer() {
    Rectangle2i render_area = aspect_ratio_fit(globals.window_width, globals.window_height, VIEW_AREA_WIDTH, VIEW_AREA_HEIGHT);

    globals.render_width  = render_area.width;
    globals.render_height = render_area.height;

    if (globals.offscreen_buffer) {
        release_framebuffer(globals.offscreen_buffer);
        free(globals.offscreen_buffer);
        globals.offscreen_buffer = NULL;
    }
    
    globals.offscreen_buffer = make_framebuffer(render_area.width, render_area.height);
}

static void init_test_world() {
    globals.current_world = new World();
    init_world(globals.current_world, v2i(32, 18));

    globals.current_world->tilemap = new Tilemap();
    if (!load_tilemap(globals.current_world->tilemap, "data/tilemaps/test.tm")) {
        exit(1);
    }
    
    Hero *hero = make_hero(globals.current_world);
    hero->position = v2(0, 1);
    hero->size     = v2(1, 1);
    hero->color    = v4(1, 0, 1, 1);

    globals.current_world->camera = new Camera();
    globals.current_world->camera->position       = v2(0, 0);
    globals.current_world->camera->target         = v2(0, 0);
    globals.current_world->camera->following_id   = hero->id;
    globals.current_world->camera->dead_zone_size = v2(VIEW_AREA_WIDTH, VIEW_AREA_HEIGHT) * 0.1f;
    globals.current_world->camera->smooth_factor  = 0.95f;
}

static void respond_to_input() {
    for (auto event : globals.events_this_frame) {
        switch (event.type) {
            case EVENT_TYPE_QUIT: {
                globals.should_quit_game = true;
            } break;

            case EVENT_TYPE_KEYBOARD: {
                Key_State *state = &key_states[event.key_code];
                state->changed   = state->is_down != event.key_pressed;
                state->is_down   = event.key_pressed;
            } break;
        }
    }

    for (auto record : globals.window_resizes) {
        if (record.window != globals.window) continue;

        globals.window_width  = record.width;
        globals.window_height = record.height;

        init_framebuffer();
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
    init_framebuffer();

    init_test_world();
    
    globals.time_info.last_time = os_get_time_nanoseconds();
    u64 last_time = os_get_time_nanoseconds();
    while (!globals.should_quit_game) {
        update_time();

        for (int i = 0; i < ArrayCount(key_states); i++) {
            Key_State *state = &key_states[i];
            state->was_down  = state->is_down;
            state->changed   = false;
        }
        
        os_update_window_events();
        respond_to_input();

        set_framebuffer(globals.offscreen_buffer);
        
        update_world(globals.current_world, (float)globals.time_info.delta_time_seconds);
        draw_world(globals.current_world);

        blit_framebuffer_to_back_buffer_with_letter_boxing(globals.offscreen_buffer);
        
        swap_buffers();

        do_hotloading();

        u64 fps_cap_nanoseconds = 1000000000 / globals.time_info.fps_cap;
    
        while (os_get_time_nanoseconds() <= last_time + fps_cap_nanoseconds) {
            // @TODO: Maybe sleep.
        }
        last_time += fps_cap_nanoseconds;
    }
    
    return 0;
}
