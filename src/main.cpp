#include "main.h"
#include "rendering.h"
#include "resource_manager.h"
#include "world.h"
#include "entity.h"
#include "tilemap.h"
#include "camera.h"
#include "font.h"
#include "main_menu.h"
#include "audio.h"

#include <stdio.h>

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

double nanoseconds_to_seconds(u64 nanoseconds) {
    double result = (double)nanoseconds / NS_PER_SECOND;
    return result;
}

u64 seconds_to_nanoseconds(double seconds) {
    u64 result = (u64)(seconds * NS_PER_SECOND);
    return result;
}

static void update_time() {
    u64 now_time = os_get_time_nanoseconds();

    globals.time_info.delta_time       = now_time - globals.time_info.last_time;
    globals.time_info.real_world_time += globals.time_info.delta_time;
    globals.time_info.delta_time_seconds = nanoseconds_to_seconds(globals.time_info.delta_time);
    globals.time_info.last_time = now_time;

    globals.time_info.num_frames_since_last_fps_update++;
    globals.time_info.accumulated_fps_dt += globals.time_info.delta_time_seconds;
    if (globals.time_info.accumulated_fps_dt >= 1.0) {
        globals.time_info.fps_dt = 1.0 / (double)globals.time_info.num_frames_since_last_fps_update;
        globals.time_info.num_frames_since_last_fps_update = 0;
        globals.time_info.accumulated_fps_dt = 0.0;
    }
}

static void init_shaders() {
    globals.shader_color   = find_or_load_shader("color");
    globals.shader_texture = find_or_load_shader("texture");
    globals.shader_text    = find_or_load_shader("text");
}

static void init_framebuffer() {
    if (globals.window_width == 0 || globals.window_height == 0) return;

    int vaw = VIEW_AREA_WIDTH, vah = VIEW_AREA_HEIGHT;
    if (globals.program_mode == PROGRAM_MODE_MAIN_MENU) {
        vaw = 16;
        vah = 9;
    }
    
    Rectangle2i render_area = aspect_ratio_fit(globals.window_width, globals.window_height, vaw, vah);

    globals.render_width  = render_area.width;
    globals.render_height = render_area.height;

    if (globals.offscreen_buffer) {
        release_framebuffer(globals.offscreen_buffer);
        free(globals.offscreen_buffer);
        globals.offscreen_buffer = NULL;
    }
    
    globals.offscreen_buffer = make_framebuffer(render_area.width, render_area.height);
}

static void draw_debug_hud() {
    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    int fps = 0;
    if (globals.time_info.fps_dt > 0.0) {
        fps = (int)(1.0 / globals.time_info.fps_dt);

        // Lower the fps limit if the user's machine can't hit the required fps.
        // @TODO: Put this somewhere better not fucking draw_debug_hud ffs.
        if (fps < globals.time_info.fps_cap) {
            globals.time_info.fps_cap /= 2;
        }
    }
    
    int font_size = (int)(0.03f * globals.render_height);
    Dynamic_Font *font = get_font_at_size("OpenSans-Regular", font_size);
    char text[128];
    snprintf(text, sizeof(text), "FPS: %d", fps);
    int x = globals.render_width  - font->get_string_width_in_pixels(text);
    int y = globals.render_height - font->character_height;
    draw_text(font, text, x, y, v4(1, 1, 1, 1));
}

static void init_test_world() {
    globals.current_world = new World();
    init_world(globals.current_world, v2i(32, 18));

    globals.current_world->tilemap = new Tilemap();
    if (!load_tilemap(globals.current_world->tilemap, "data/tilemaps/test.tm")) {
        exit(1);
    }
    
    Hero *hero     = make_hero(globals.current_world);
    hero->position = v2(0, 1);
    hero->size     = v2(1, 1);
    hero->color    = v4(1, 0, 1, 1);

    Door *door     = make_door(globals.current_world);
    door->position = v2(31, 12);
    door->size     = v2(1, 2);
    
    globals.current_world->camera                 = new Camera();
    globals.current_world->camera->position       = v2(18, 9);
    globals.current_world->camera->target         = v2(0, 0);
    globals.current_world->camera->following_id   = hero->id;
    globals.current_world->camera->dead_zone_size = v2(VIEW_AREA_WIDTH, VIEW_AREA_HEIGHT) * 0.1f;
    globals.current_world->camera->smooth_factor  = 0.95f;

    Enemy *enemy    = make_enemy(globals.current_world);
    enemy->position = v2(10, 1.5f);
    enemy->color    = v4(1, 1, 1, 1);

    Pickup *pickup   = make_pickup(globals.current_world);
    pickup->position = v2(25.5f, 6.5f);
    pickup->color    = v4(1.0f, 1.0f, 0.0f, 1.0f);
    pickup->radius   = 0.5f;

    globals.current_world->num_pickups_needed_to_unlock_door = globals.current_world->by_type._Pickup.count;
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

                if (event.key_pressed && !event.is_key_repeat) {
                    if (event.key_code == KEY_F11) {
                        os_window_toggle_fullscreen(globals.window);
                    }
                }

                if (globals.program_mode == PROGRAM_MODE_END) {
                    if (event.key_pressed) {
                        globals.program_mode = PROGRAM_MODE_MAIN_MENU;
                        switch_to_random_world(globals.start_level_width);
                    }
                }
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

static void generate_random_level(World *world, int level_width, int level_height) {
    if (!world) return;

    if (!world->tilemap) {
        world->tilemap = new Tilemap();
    }

    Tilemap *tilemap = world->tilemap;
    tilemap->width   = level_width;
    tilemap->height  = level_height;

    tilemap->tiles = new u8[level_width * level_height];
    memset(tilemap->tiles, 0, level_width * level_height);

    tilemap->num_collidable_ids = 1;
    tilemap->collidable_ids = new u8[tilemap->num_collidable_ids];
    tilemap->collidable_ids[0] = 1;

    tilemap->num_colors = 1;
    tilemap->colors = new Vector4[tilemap->num_colors];
    tilemap->colors[0] = v4(1, 1, 1, 1);

    int ground_y = 0;
    for (int x = 0; x < level_width; x++) {
        tilemap->tiles[ground_y * level_width + x] = 1;
    }

    Hero *hero = make_hero(world);
    hero->position = v2(1, ground_y + 1.0f);
    hero->size     = v2(1, 1);

    int last_platform_end_x = 0;
    int last_platform_y = ground_y;

    while (last_platform_end_x < level_width - 5) {
        int gap = 2 + rand() % 3;
        int platform_width = 3 + rand() % 3;
        int platform_y = last_platform_y + (rand() % 3 - 1);

        if (platform_y < 3) platform_y = 3;
        if (platform_y > level_height - 4) platform_y = level_height;

        int start_x = last_platform_end_x + gap;
        if (start_x + platform_width >= level_width - 1) {
            platform_width = level_width - 1 - start_x;
        }

        for (int x = start_x; x < start_x + platform_width; x++) {
            tilemap->tiles[platform_y * level_width + x] = 1;
        }

        if (rand() % 2 == 0) {
            Pickup *pickup = make_pickup(world);
            pickup->position = v2((float)(start_x + platform_width * 0.5f) + 0.5f,
                                  (float)platform_y + 1.5f);
            pickup->color    = v4(1, 1, 0, 1);
            pickup->radius   = 0.5f;
        }

        if (rand() % 3 == 0) {
            Enemy *enemy    = make_enemy(world);
            enemy->position = v2((float)(start_x + platform_width * 0.5f) + 0.5f,
                                 (float)platform_y + 1.5f);
            enemy->color    = v4(0, 0, 1, 1);
            enemy->radius   = 0.5f;
        }

        last_platform_end_x = start_x + platform_width;
        last_platform_y = platform_y;
    }
    
    Door *door = make_door(world);
    door->position = v2((float)(last_platform_end_x - 1.0f), (float)last_platform_y + 1.0f);
    door->size     = v2(1, 2);
    door->locked   = true;

    world->num_pickups_needed_to_unlock_door = (int)world->by_type._Pickup.count;
}

bool switch_to_random_world(int total_width) {
    if (globals.current_world) {
        destroy_world(globals.current_world);
        delete globals.current_world;
        globals.current_world = NULL;
    }

    globals.current_world = new World();
    init_world(globals.current_world, v2i(total_width, 18));

    generate_random_level(globals.current_world, total_width, 18);

    globals.current_world->camera                 = new Camera();
    globals.current_world->camera->position       = v2(total_width * 0.5f, 9);
    globals.current_world->camera->target         = v2(0, 0);
    globals.current_world->camera->following_id   = globals.current_world->by_type._Hero->id;
    globals.current_world->camera->dead_zone_size = v2(VIEW_AREA_WIDTH, VIEW_AREA_HEIGHT) * 0.1f;
    globals.current_world->camera->smooth_factor  = 0.95f;

    return true;
}

static void draw_end_screen() {
    clear_framebuffer(0, 0, 0, 1);
    
    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);
    
    int font_size = (int)(0.1f * globals.render_height);
    Dynamic_Font *font = get_font_at_size("KarminaBoldItalic", font_size);
    char *text = "Congratulations!";
    int x = (globals.render_width  - font->get_string_width_in_pixels(text)) / 2;
    int y = globals.render_height / 2;
    draw_text(font, text, x, y, v4(1, 1, 1, 1));

    text = "You've completed the game!";
    x = (globals.render_width  - font->get_string_width_in_pixels(text)) / 2;
    y -= font->character_height;
    draw_text(font, text, x, y, v4(1, 1, 1, 1));
}

int main(int argc, char *argv[]) {
    os_init();
    srand((u32)os_get_time_nanoseconds());

    globals.window_width  = 1600;
    globals.window_height = 900;
    globals.window = os_create_window(globals.window_width, globals.window_height, "Platformer!");
    if (!globals.window) return 1;
    if (!init_rendering(globals.window, globals.should_vsync)) return 1;
    init_resource_manager(); // !!! Need to init resource manager before shaders and textures !!!
    init_shaders();
    init_framebuffer();
    init_audio();
    defer { destroy_audio(); };

    int current_level_width = globals.start_level_width;
    switch_to_random_world(current_level_width);
    
    globals.time_info.last_time = os_get_time_nanoseconds();
    u64 last_time = os_get_time_nanoseconds();
    while (!globals.should_quit_game) {
        if (globals.should_switch_worlds) {
            current_level_width += 30;
            switch_to_random_world(current_level_width);
            globals.should_switch_worlds = false;
        }
        
        update_time();
        
        for (int i = 0; i < ArrayCount(key_states); i++) {
            Key_State *state = &key_states[i];
            state->was_down  = state->is_down;
            state->changed   = false;
        }

        os_update_window_events();
        respond_to_input();

        if (globals.program_mode == PROGRAM_MODE_GAME) {
            update_world(globals.current_world, (float)globals.time_info.delta_time_seconds);

            if (is_key_pressed(KEY_ESCAPE)) toggle_menu();
        }

        if (globals.window_width > 0 && globals.window_height > 0) {
            set_framebuffer(globals.offscreen_buffer);
            set_viewport(0, 0, globals.render_width, globals.render_height);
            set_shader(NULL);
            
            if (globals.program_mode == PROGRAM_MODE_MAIN_MENU) {
                draw_main_menu();
            } else if (globals.program_mode == PROGRAM_MODE_GAME) {
                draw_world(globals.current_world);
                draw_debug_hud();
            } else if (globals.program_mode == PROGRAM_MODE_END) {
                draw_end_screen();
            }

            blit_framebuffer_to_back_buffer_with_letter_boxing(globals.offscreen_buffer);
        }
        
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
