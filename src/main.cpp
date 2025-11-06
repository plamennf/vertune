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

char *fail_msgs[] = {
    "You fell short... Try again!",
    "Not quite there - give it another go!",
    "The world fades... but you can rise again."
    "One misstep. But you're getting closer."
};

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

static void adjust_fps_cap_based_on_performance() {    
    int fps = 0;
    if (globals.time_info.fps_dt > 0.0) {
        fps = (int)(1.0 / globals.time_info.fps_dt);
    } else {
        return;
    }
    
    if (fps < globals.time_info.fps_cap * 0.9f) { // Only count if significantly below target
        globals.time_info.slow_frame_count++;
    } else {
        globals.time_info.slow_frame_count = 0; // reset if performance recovers
    }

    if (globals.time_info.slow_frame_count > MAX_SLOW_FRAMES) {
        globals.time_info.fps_cap = Max(30, globals.time_info.fps_cap / 2);
        globals.time_info.slow_frame_count = 0;
    }

    if (fps > globals.time_info.fps_cap * 1.1f) {
        globals.time_info.fast_frame_count++;
        if (globals.time_info.fast_frame_count > 300 && globals.time_info.fps_cap < 120) {
            globals.time_info.fps_cap *= 2;
            globals.time_info.fps_cap = Max(MAX_FPS_CAP, globals.time_info.fps_cap);
            globals.time_info.fast_frame_count = 0;
        }
    } else {
        globals.time_info.fast_frame_count = 0;
    }
}

static void init_shaders() {
    globals.shader_color   = find_or_load_shader("color");
    globals.shader_texture = find_or_load_shader("texture");
    globals.shader_text    = find_or_load_shader("text");

    globals.full_heart    = load_texture_from_file("data/textures/heart_full_16x16.png");
    globals.half_heart    = load_texture_from_file("data/textures/heart_half_16x16.png");
    globals.empty_heart   = load_texture_from_file("data/textures/heart_empty_16x16.png");
    globals.restart_taken = load_texture_from_file("data/textures/restart_taken.png");
    globals.restart_available = load_texture_from_file("data/textures/restart_available.png");
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

#if 0
        // Lower the fps limit if the user's machine can't hit the required fps.
        // @TODO: Put this somewhere better not fucking draw_debug_hud ffs.
        if (fps < globals.time_info.fps_cap) {
            globals.time_info.fps_cap /= 2;
        }
#endif
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
                    if (event.key_pressed && !event.is_key_repeat) {
                        if (!globals.menu_fade.active) {
                            start_menu_fade(globals.current_world);
                        }

#if 0
                        globals.program_mode = PROGRAM_MODE_MAIN_MENU;
                        //switch_to_random_world(globals.start_level_width);
                        destroy_world(globals.current_world);
                        destroy_world(globals.copy_of_current_world);
                        globals.copy_of_current_world = NULL;
                        globals.current_world = globals.menu_world;
                        globals.num_restarts_for_current_world = 0;
                        // TODO: Save the high scores to a file.
#endif
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

    float max_jump_height = (JUMP_FORCE * JUMP_FORCE / (-2.0f * GRAVITY)) - 1.0f;
    
    struct Platform {
        int x_start;
        int x_end;
        int y;
    };
    Array <Platform> platforms;

    int last_x = 1;
    int last_y = 2;
    int min_platform_length = 3;
    int max_platform_length = 6;
    int min_gap = 2;
    int max_gap = 4;
    bool first_platform = true;
    
    while (last_x + min_gap + min_platform_length < level_width - 6) {
        int gap = min_gap + rand() % (max_gap - min_gap + 1);
        int plat_length = min_platform_length + rand() % (max_platform_length - min_platform_length + 1);

        int y = 2;
        if (first_platform) {
            first_platform = false;
        } else {
            int max_y = last_y + (int)max_jump_height;
            int min_y = Max(2, last_y - (int)max_jump_height);
            y = min_y + rand() % (max_y - min_y + 1);
        }

        int x_start = last_x + gap;
        int x_end = x_start + plat_length;
        if (x_end >= level_width - 3) {
            x_end = level_width - 4;
            x_start = x_end - plat_length;
        }

        x_start = Max(0, x_start);
        x_end   = Min(level_width - 1, x_end);

        for (int x = x_start; x<= x_end; x++) {
            tilemap->tiles[y * level_width + x] = 1;
        }

        platforms.add({x_start, x_end, y});
        last_x = x_end;
        last_y = y;
    }

    int door_platform_length = 3;
    int door_y = platforms[platforms.count - 1].y + 2 + rand() % (int)max_jump_height;
    door_y = Min(level_height - 2, door_y);
    int door_x_end = level_width - 1;
    int door_x_start = Max(0, door_x_end - door_platform_length + 1);

    for (int x = door_x_start; x <= door_x_end; x++) {
        tilemap->tiles[door_y * level_width + x] = 1;
    }

    platforms.add({door_x_start, door_x_end, door_y});
    
    Hero *hero = make_hero(world);
    hero->position = v2(1, ground_y + 1.0f);
    hero->size     = v2(1, 1);

    for (int i = 0; i < platforms.count - 1; i++) {
        Platform plat = platforms[i];
        int num_coins = 1 + rand() % 2;

        for (int i = 0; i < num_coins; i++) {
            float coin_x = plat.x_start + 0.5f + rand() % (plat.x_end - plat.x_start + 1);
            float coin_y = plat.y + 2.5f;

            bool boost_coin = (rand() % 3 == 0);
            if (boost_coin) {
                coin_y = plat.y + max_jump_height + 1.0f + rand() % 2;

                Enemy *enemy    = make_enemy(world);
                enemy->position = v2(coin_x, plat.y + 1.5f);
                enemy->color    = v4(0, 0, 1, 1);
                enemy->radius   = 0.5f;
            }

            Pickup *pickup = make_pickup(world);
            pickup->position = v2(coin_x, coin_y);
            pickup->color    = v4(1, 1, 0, 1);
            pickup->radius   = 0.5f;
        }
    }
        
    Door *door = make_door(world);
    door->position = v2(door_x_start + 2.0f, door_y + 1.0f);
    door->size     = v2(1, 2);
    door->locked   = true;

    world->num_pickups_needed_to_unlock_door = (int)world->by_type._Pickup.count;
}

bool create_menu_world() {
    globals.menu_world = new World();
    init_world(globals.menu_world, v2i(20, 18));

    generate_random_level(globals.menu_world, 20, 18);

    globals.menu_world->camera           = new Camera();
    globals.menu_world->camera->position = globals.menu_world->by_type._Hero->position + v2(VIEW_AREA_WIDTH * 0.5f, VIEW_AREA_HEIGHT * 0.5f);

    return true;
}

bool switch_to_random_world(int total_width) {
    if (globals.current_world && globals.current_world != globals.menu_world) {
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

    globals.current_world_index++;
    
    Level_Fade level_fade   = {};
    level_fade.active       = true;
    level_fade.timer        = 0.0f;
    level_fade.duration     = 1.5f;
    level_fade.level_number = globals.current_world_index;
    globals.current_world->level_fade = level_fade;

    if (globals.copy_of_current_world) {
        destroy_world(globals.copy_of_current_world);
        globals.copy_of_current_world = NULL;
    }
    globals.copy_of_current_world = copy_world(globals.current_world);

    globals.num_restarts_for_current_world = 0;
    
    return true;
}

bool restart_current_world() {
    if (!globals.current_world) return false;
    assert(globals.current_world);

    globals.num_restarts_for_current_world++;
    if (globals.num_restarts_for_current_world > MAX_RESTARTS) {
        globals.program_mode = PROGRAM_MODE_END;
        globals.current_fail_msg_index = rand() % ArrayCount(fail_msgs);
        return true;
    }

    if (globals.copy_of_current_world) {
        destroy_world(globals.current_world);
    }
    globals.current_world = copy_world(globals.copy_of_current_world);
    
    return true;
}

static void draw_end_screen() {
    //clear_framebuffer(0, 0, 0, 1);

    set_shader(globals.shader_color);
    rendering_2d(globals.render_width, globals.render_height, matrix4_identity());
    
    immediate_begin();
    immediate_quad(v2(0, 0), v2((float)globals.render_width, (float)globals.render_height), v4(0, 0, 0, 0.5f));
    immediate_flush();

    immediate_begin();
    int num_drops = 50;
    for (int i = 0; i < num_drops; i++) {
        float speed = 200.0f + (i * 10.0f);
        float t = fmodf((globals.num_frames_since_startup * 0.016f * speed + i * 30.0f), globals.render_height + 20.0f); 
        float x = fmodf(i * 37.0f + 50.0f, (float)globals.render_width);
        float y = globals.render_height - t;
    
        float width = 2.0f;
        float height = 10.0f + (i % 5);
        Vector4 color = v4(0.8f, 0.9f, 1.0f, 0.2f + 0.1f * sinf(i + t));
    
        immediate_quad(v2(x, y), v2(width, height), color);
    }
    immediate_flush();
    
    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);
    
    int font_size = (int)(0.05f * globals.render_height);
    Dynamic_Font *font = get_font_at_size("KarminaBoldItalic", font_size);
    char text[256];
    snprintf(text, sizeof(text), "You managed to complete %d levels!", globals.num_worlds_completed);
    int x = (globals.render_width  - font->get_string_width_in_pixels(text)) / 2;
    int y = globals.render_height / 2;
    draw_text(font, text, x, y, v4(1, 1, 1, 1));
    
    snprintf(text, sizeof(text), "%s", fail_msgs[globals.current_fail_msg_index]);
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

    if (!create_menu_world()) return 1;
    globals.current_world = globals.menu_world;
    
    int current_level_width = globals.start_level_width;
    //switch_to_random_world(current_level_width);
    
    globals.time_info.last_time = os_get_time_nanoseconds();
    u64 last_time = os_get_time_nanoseconds();
    while (!globals.should_quit_game) {
        globals.num_frames_since_startup++;
        
        if (globals.should_switch_worlds) {
            bool should_restart_level = false;
            if (globals.current_world) {
                if (!globals.current_world->by_type._Hero ||
                    globals.current_world->by_type._Hero->health <= 0.0) {
                    should_restart_level = true;
                }
            }

            if (should_restart_level) {
                restart_current_world();
            } else {
                current_level_width += 30;
                switch_to_random_world(current_level_width);
            }

            globals.should_switch_worlds = false;
        }
        
        update_time();
        adjust_fps_cap_based_on_performance();
        
        for (int i = 0; i < ArrayCount(key_states); i++) {
            Key_State *state = &key_states[i];
            state->was_down  = state->is_down;
            state->changed   = false;
        }

        os_update_window_events();
        respond_to_input();

        if (globals.program_mode == PROGRAM_MODE_GAME) {
            update_world(globals.current_world, (float)globals.time_info.delta_time_seconds);

            if (is_key_pressed(KEY_ESCAPE)) {
                toggle_menu();
            }
        }

        update_menu_fade((float)globals.time_info.delta_time_seconds);
        
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
                if (globals.current_world) {
                    draw_world(globals.current_world, true);
                }
                draw_end_screen();
            }

            if (globals.menu_fade.active) {
                draw_menu_fade_overlay();
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


void start_menu_fade(World *world) {
    globals.menu_fade.active = true;
    globals.menu_fade.timer = 0.0f;
    globals.menu_fade.fading_in = false;
    globals.menu_fade.last_world = world;
}

void update_menu_fade(float dt) {
    if (!globals.menu_fade.active) return;

    globals.menu_fade.timer += dt;
    if (globals.menu_fade.timer >= globals.menu_fade.duration) {
        globals.menu_fade.timer = globals.menu_fade.duration;

        if (!globals.menu_fade.fading_in) {
            // Switch to menu now that fade out is done
            globals.program_mode = PROGRAM_MODE_MAIN_MENU;

            destroy_world(globals.current_world);
            destroy_world(globals.copy_of_current_world);
            globals.copy_of_current_world = NULL;
            globals.current_world = globals.menu_world;
            globals.num_restarts_for_current_world = 0;
            
            globals.menu_fade.fading_in = true;
            globals.menu_fade.timer = 0.0f;
            globals.menu_fade.last_world = globals.menu_world;
        } else {
            globals.menu_fade.active = false; // done fading in
            globals.menu_fade.last_world = NULL;
        }
    }
}

void draw_menu_fade_overlay() {
    float alpha = 0.0f;

    if (!globals.menu_fade.active) return;

    if (!globals.menu_fade.fading_in) {
        alpha = globals.menu_fade.timer / globals.menu_fade.duration;  // fade out
    } else {
        alpha = 1.0f - (globals.menu_fade.timer / globals.menu_fade.duration); // fade in
    }

    // Draw the last world behind the fade
    if (globals.menu_fade.last_world) {
        draw_world(globals.menu_fade.last_world, true); // skip HUD
    }

    // Draw fade overlay
    set_shader(globals.shader_color);
    rendering_2d(globals.render_width, globals.render_height);
    immediate_begin();
    immediate_quad(
        v2(0, 0),
        v2((float)globals.render_width, (float)globals.render_height),
        v4(0, 0, 0, alpha)
                   );
    immediate_flush();
}
