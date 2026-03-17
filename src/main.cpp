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
#include "packager/packager.h"
#ifndef OS_WINDOWS
#include "icon_data.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#include <GL/glew.h>
#endif
#include <stdio.h>

#include <eastl/sort.h>

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

static Key_State key_states[SDL_NUM_SCANCODES];

static void toggle_fullscreen(SDL_Window *window);

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
    s64 now_time = get_time_nanoseconds();
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
    globals.shader_color   = make_shader();
    load_shader(globals.shader_color, R"(
precision highp float;

OUT_IN vec4 v_color;

#ifdef VERTEX_SHADER

in vec2 a_position;
in vec4 a_color;
in vec2 a_uv;

uniform mat4 object_to_proj_matrix;

void main() {
    gl_Position = object_to_proj_matrix * vec4(a_position, 0.0, 1.0);
    v_color     = a_color;
}

#endif

#ifdef FRAGMENT_SHADER

out vec4 o_color;

void main() {
#ifdef SGLES
    o_color = vec4(pow(v_color.xyz, vec3(1.0 / 2.2)), v_color.a);
#else
    o_color = v_color;
#endif
}

#endif
)", "color");

    globals.shader_lighting   = make_shader();
    load_shader(globals.shader_lighting, R"(
precision highp float;

OUT_IN vec4 v_color;
OUT_IN vec2 v_world_position;

#ifdef VERTEX_SHADER

in vec2 a_position;
in vec4 a_color;
in vec2 a_uv;

uniform mat4 object_to_proj_matrix;

void main() {
    gl_Position      = object_to_proj_matrix * vec4(a_position, 0.0, 1.0);
    v_color          = a_color;
    v_world_position = a_position;
}

#endif

#ifdef FRAGMENT_SHADER

out vec4 o_color;

struct Light {
    vec2 position;
    vec4 color;
    float radius;
    float intensity;
};

uniform Light u_lights[MAX_LIGHTS];
uniform vec4 u_occluders[64]; // x, y, w, h

float get_shadow(vec2 pixel_pos, vec2 light_pos, float light_dist) {
    vec2 ray_dir = normalize(light_pos - pixel_pos);

    vec2 safe_dir = vec2(
        abs(ray_dir.x) < 0.00001 ? 0.00001 : ray_dir.x,
        abs(ray_dir.y) < 0.00001 ? 0.00001 : ray_dir.y
    );
    vec2 inv_dir = 1.0 / safe_dir;

    for (int i = 0; i < 64; i++) {
        vec4 r = u_occluders[i];

        vec2 t1 = (r.xy - pixel_pos) * inv_dir;
        vec2 t2 = (r.xy + r.zw - pixel_pos) * inv_dir;
        vec2 t_min = min(t1, t2);
        vec2 t_max = max(t1, t2);

        float n = max(t_min.x, t_min.y);
        float f = min(t_max.x, t_max.y);

        if (f > n && n > 0.0 && n < light_dist) return 0.0;
    }

    return 1.0;
}

void main() {
    vec2 pixel_pos = v_world_position;
    vec3 final_lighting = vec3(0.1, 0.1, 0.2);

    for (int i = 0; i < MAX_LIGHTS; i++) {
        float d = length(u_lights[i].position - pixel_pos);
        if (d < u_lights[i].radius) {
            float s = get_shadow(pixel_pos, u_lights[i].position, d);
            float a = pow(clamp(1.0 - d / u_lights[i].radius, 0.0, 1.0), 2.0);
            final_lighting += u_lights[i].color.rgb * a * s * u_lights[i].intensity;
        }
    }

    final_lighting *= v_color.rgb;

#ifdef SGLES
    o_color = vec4(pow(final_lighting.xyz, vec3(1.0 / 2.2)), 1.0);
#else
    o_color = vec4(final_lighting, 1.0);
#endif
}

#endif
)", "lighting");
    
    globals.shader_texture = make_shader();
    load_shader(globals.shader_texture, R"(
precision highp float;

OUT_IN vec4 v_color;
OUT_IN vec2 v_uv;

#ifdef VERTEX_SHADER

in vec2 a_position;
in vec4 a_color;
in vec2 a_uv;

uniform mat4 object_to_proj_matrix;

void main() {
    gl_Position = object_to_proj_matrix * vec4(a_position, 0.0, 1.0);
    v_color     = a_color;
    v_uv        = a_uv;
}

#endif

#ifdef FRAGMENT_SHADER

out vec4 o_color;

uniform sampler2D tex;

void main() {
    vec4 tex_color = texture(tex, v_uv);

#ifdef SGLES
    tex_color.xyz = pow(tex_color.xyz, vec3(2.2));
    o_color = vec4(pow(v_color.xyz * tex_color.xyz, vec3(1.0 / 2.2)), v_color.a * tex_color.a);
#else
    o_color = v_color * tex_color;
#endif
}

#endif

)", "texture");
    
    globals.shader_text    = make_shader();
    load_shader(globals.shader_text, R"(
precision highp float;

OUT_IN vec4 v_color;
OUT_IN vec2 v_uv;

#ifdef VERTEX_SHADER

in vec2 a_position;
in vec4 a_color;
in vec2 a_uv;

uniform mat4 object_to_proj_matrix;

void main() {
    gl_Position = object_to_proj_matrix * vec4(a_position, 0.0, 1.0);
    v_color     = a_color;
    v_uv        = a_uv;
}

#endif

#ifdef FRAGMENT_SHADER

out vec4 o_color;

uniform sampler2D tex;

void main() {
    vec4 tex_color = texture(tex, v_uv);
    tex_color = vec4(1.0, 1.0, 1.0, tex_color.r);
#ifdef SGLES
    o_color = vec4(pow(v_color.xyz * tex_color.xyz, vec3(1.0 / 2.2)), v_color.a * tex_color.a);
#else
    o_color = v_color * tex_color;
#endif
}

#endif
)", "text");
}

static void load_assets() {
    MyZoneScoped;
    
    globals.white_texture = make_texture();
    u8 white_texture_data[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    load_texture_from_data(globals.white_texture, 1, 1, TEXTURE_FORMAT_RGBA8, white_texture_data);
    
    globals.full_heart    = find_or_load_texture("heart_full_16x16");
    if (!globals.full_heart) globals.full_heart = globals.white_texture;
    
    globals.half_heart    = find_or_load_texture("heart_half_16x16");
    if (!globals.half_heart) globals.half_heart = globals.white_texture;
    
    globals.empty_heart   = find_or_load_texture("heart_empty_16x16");
    if (!globals.empty_heart) globals.empty_heart = globals.white_texture;
    
    globals.restart_taken = find_or_load_texture("restart_taken");
    if (!globals.restart_taken) globals.restart_taken = globals.white_texture;
    
    globals.restart_available = find_or_load_texture("restart_available");
    if (!globals.restart_available) globals.restart_available = globals.white_texture;

    globals.door_texture = find_or_load_texture("door");
    if (!globals.door_texture) globals.door_texture = globals.white_texture;
    
    globals.menu_background_music = find_or_load_sound("menu-music", true);
    globals.level_background_music = find_or_load_sound("level-music", true);
    globals.coin_pickup_sfx    = find_or_load_sound("coin-pickup", false);
    globals.level_complete_sfx = find_or_load_sound("level-completed", false);
    globals.death_sfx = find_or_load_sound("death", false);
    globals.jump_sfx = find_or_load_sound("jump", false);
    globals.damage_sfx = find_or_load_sound("damage", false);
    globals.enemy_kill_sfx = find_or_load_sound("enemy-kill", false);
    globals.level_fail_sfx = find_or_load_sound("level-failed", false);

    globals.menu_change_option = find_or_load_sound("menu-change-option", false);
    globals.menu_select = find_or_load_sound("menu-select", false);
    globals.exit_menu = find_or_load_sound("exit-menu", false);
}

static void init_framebuffer() {
    if (globals.window_width == 0 || globals.window_height == 0) return;

#ifdef __EMSCRIPTEN__
    globals.render_width  = globals.window_width;
    globals.render_height = globals.window_height;
#else
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
#endif
}

static void draw_debug_hud() {
    MyZoneScoped;
    
    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    int fps = 0;
    if (globals.time_info.fps_dt > 0.0) {
        fps = (int)(1.0 / globals.time_info.fps_dt);
    }
    
    int font_size = (int)(0.03f * globals.render_height);
    Dynamic_Font *font = get_font_at_size("OpenSans-Regular", font_size);
    char text[128];
    snprintf(text, sizeof(text), "FPS: %d", fps);
    int x = globals.render_width  - font->get_string_width_in_pixels(text);
    int y = globals.render_height - font->character_height - ((int)(0.04f * globals.render_height));

    if (globals.draw_outlines) {
        int offset = font->character_height / 20;
        draw_text_outlined(font, text, x, y, v4(1, 1, 1, 1), offset);
    } else {
        draw_text(font, text, x, y, v4(1, 1, 1, 1));
    }
}

static void respond_to_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: {
                globals.should_quit_game = true;
            } break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                bool is_down = event.type == SDL_KEYDOWN;
                
                Key_State *state = &key_states[event.key.keysym.scancode];
                state->changed   = state->is_down != is_down;
                state->is_down   = is_down;

                if (is_down && !event.key.repeat) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_F11) {
                        toggle_fullscreen(globals.window);
                    }
                }

                if (globals.program_mode == PROGRAM_MODE_END) {
                    if (is_down && !event.key.repeat && event.key.keysym.scancode != SDL_SCANCODE_F11) {
                        if (!globals.menu_fade.active) {
                            start_menu_fade(globals.current_world);
                        }
                    }
                }
            } break;

            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED: {
                        globals.window_width  = event.window.data1;
                        globals.window_height = event.window.data2;

                        init_framebuffer();
                    } break;
                }
            }
        }
    }
}

static void generate_random_level(World *world, int level_width, int level_height, bool create_lights = true, Level_Type *override_level_type = NULL) {
    MyZoneScoped;
    
    if (!world) return;

    if (!world->tilemap) {
        world->tilemap = new Tilemap();
    }

    int level_type = rand() % LEVEL_TYPE_COUNT;
    world->level_type = (Level_Type)level_type;

    if (override_level_type) {
        world->level_type = *override_level_type;
    }

    if (world->level_type == LEVEL_TYPE_OCEAN) {
        int num_clouds = 3 + (rand() % 3);

        for (int i = 0; i < num_clouds; i++) {
            float rx = (float)(rand() % 100) / 100.0f;
            float ry = 0.7f + ((float)(rand() % 30) / 100.0f);
            float rs = 0.5f + ((float)(rand() % 100) / 100.0f);
            float ra = 0.6f + ((float)(rand() % 40) / 100.0f);

            Vector2 base_size  = v2(2.0f * (16.0f/9.0f), 2.0f);
            Vector2 final_size = base_size * rs;

            Vector2 world_position = v2(VIEW_AREA_WIDTH * rx, VIEW_AREA_HEIGHT * ry);
            float speed_multiplier = rs * 0.8f;

            Cloud cloud;

            cloud.position         = world_position;
            cloud.size             = final_size;
            cloud.speed_multiplier = speed_multiplier;
            
            world->clouds_for_ocean_level.push_back(cloud);
        }

        eastl::sort(world->clouds_for_ocean_level.begin(), world->clouds_for_ocean_level.end(), [](Cloud const &a, Cloud const &b) {
            return a.speed_multiplier < b.speed_multiplier;
        });
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
    eastl::vector <Platform> platforms;

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
            max_y = Min(max_y, level_height - 4);
            int min_y = Max(2, last_y - (int)max_jump_height);
            min_y = Min(min_y, max_y);
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

        platforms.push_back({x_start, x_end, y});
        last_x = x_end;
        last_y = y;
    }

    int door_platform_length = 3;
    int door_y = platforms[platforms.size() - 1].y + 2 + rand() % (int)max_jump_height;
    door_y = Min(level_height - 2, door_y);
    int door_x_end = level_width - 1;
    int door_x_start = Max(0, door_x_end - door_platform_length + 1);

    if (door_x_start - platforms[platforms.size() - 1].x_end >= 4) {
        door_x_start = platforms[platforms.size() - 1].x_end + 3;
        door_x_end = level_width - 1;
        door_platform_length = door_x_end - door_x_start;
    }
    
    for (int x = door_x_start; x <= door_x_end; x++) {
        tilemap->tiles[door_y * level_width + x] = 1;
    }

    platforms.push_back({door_x_start, door_x_end, door_y});
    
    Hero *hero = make_hero(world);
    hero->position = v2(1, ground_y + 1.0f);
    hero->size     = v2(1, 1);

    if (create_lights) {
        Light *start_light = make_light(world);
        start_light->position  = v2(hero->position.x, hero->position.y + 5.0f);
        start_light->color     = v4(0.4f, 0.6f, 1.0f, 1.0f);
        start_light->radius    = 13.0f;
        start_light->intensity = 0.8f;
    }
        
    for (int i = 0; i < platforms.size() - 1; i++) {
        Platform platform = platforms[i];
        int platform_width = platform.x_end - platform.x_start;

        if (create_lights) {
            Light *platform_light     = make_light(world);
            platform_light->position  = v2((platform.x_start + platform.x_end) * 0.5f, platform.y + 10.0f);
            platform_light->color     = v4(1.0f, 0.9f, 0.7f, 1.0f);
            platform_light->radius    = 18.0f;
            platform_light->intensity = 0.6f;
        }
        
        eastl::vector<bool> slots_occupied(platform_width + 1, false);

        int max_features = platform_width > 5 ? 2 : 1;
        int features_spawned = 0;
        int attempts = 0;
        
        while (features_spawned < max_features && attempts < 10) {
            attempts++;

            int slot = 1 + (rand() % Max(1, platform_width - 1));
            if (slots_occupied[slot]) continue;

            float spawn_x = platform.x_start + slot + 0.5f;
            bool is_combo = (rand() % 2 == 0);
            if (is_combo) {
                Enemy *enemy    = make_enemy(world);
                enemy->position = v2(spawn_x, platform.y + 1.5f);
                enemy->color    = v4(0, 0, 1, 1);
                enemy->radius   = 0.5f;

                float coin_y = platform.y + max_jump_height + 3.0f + rand() % 2;

                Pickup *pickup   = make_pickup(world);
                pickup->position = v2(spawn_x, coin_y);
                pickup->color    = v4(1, 1, 0, 1);
                pickup->radius   = 0.25f;

                if (create_lights) {
                    Light *light       = make_light(world);
                    light->position    = pickup->position;
                    light->color       = v4(1, 0.8f, 0, 1);
                    light->radius      = 1.5f;
                    light->intensity   = 1.5f;
                    light->should_draw = false;
                    pickup->light_id   = light->id;
                }
            } else {
                Pickup *pickup   = make_pickup(world);
                pickup->position = v2(spawn_x, platform.y + 2.5f);
                pickup->color    = v4(1, 1, 0, 1);
                pickup->radius   = 0.25f;

                if (create_lights) {
                    Light *light       = make_light(world);
                    light->position    = pickup->position;
                    light->color       = v4(1, 0.8f, 0, 1);
                    light->radius      = 1.5f;
                    light->intensity   = 1.5f;
                    light->should_draw = false;
                    pickup->light_id   = light->id;
                }
            }

            slots_occupied[slot] = true;
            if (slot > 0)              slots_occupied[slot - 1] = true;
            if (slot < platform_width) slots_occupied[slot + 1] = true;

            features_spawned++;
        }
    }
        
    Door *door = make_door(world);
    door->position = v2((float)door_x_end, door_y + 1.0f);
    door->size     = v2(1, 2);
    door->locked   = true;

    if (create_lights) {
        Light *door_light = make_light(world);
        door_light->position  = v2(door->position.x, door->position.y + door->size.y + 2.0f);
        door_light->color     = DOOR_LIGHT_LOCKED_COLOR;
        door_light->radius    = 12.0f;
        door_light->intensity = 1.0f;

        door->light_id = door_light->id;
    }
        
    world->num_pickups_needed_to_unlock_door = (int)world->by_type._Pickup.size();
}

bool create_menu_world() {
    MyZoneScoped;
    
    globals.menu_world = new World();
    init_world(globals.menu_world, v2i(20, 18));

    Level_Type level_type = LEVEL_TYPE_BASIC;
    generate_random_level(globals.menu_world, 20, 18, false, &level_type);

    globals.menu_world->camera           = new Camera();
    globals.menu_world->camera->position = globals.menu_world->by_type._Hero->position + v2(VIEW_AREA_WIDTH * 0.5f, VIEW_AREA_HEIGHT * 0.5f);

    return true;
}

bool switch_to_random_world(int total_width) {
    MyZoneScoped;
    
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

    globals.current_world->camera->intro_active = true;
    globals.current_world->camera->intro_timer = 0.0f;
    globals.current_world->camera->intro_duration = 3.0f;

    float world_w = (float)globals.current_world->size.x;
    float world_h = (float)globals.current_world->size.y;
    float aspect = (float)globals.render_width / globals.render_height;

    float visible_w = (float)VIEW_AREA_WIDTH;
    float visible_h = (float)VIEW_AREA_HEIGHT;

    float zoom_x = visible_w / world_w;
    float zoom_y = visible_h / world_h;

    float zoom_to_fit = Min(zoom_x, zoom_y) * 1.0f;
    
    globals.current_world->camera->intro_start_zoom = zoom_to_fit;
    globals.current_world->camera->intro_end_zoom = 1.0f;

    Vector2 intro_start_pos = v2(world_w * 0.5f, world_h * 0.5f) - v2(visible_w * 0.45f, visible_h * 0.5f);
    if (intro_start_pos.x < 0.0f) intro_start_pos.x = 0.0f;
    if (intro_start_pos.y < 0.0f) intro_start_pos.y = 0.0f;
    globals.current_world->camera->intro_start_pos = intro_start_pos;
    globals.current_world->camera->intro_end_pos = v2(visible_w * 0.5f, visible_h * 0.5f);

    globals.current_world->camera->zoom = globals.current_world->camera->intro_start_zoom;
    globals.current_world->camera->position = globals.current_world->camera->intro_start_pos;
    
    globals.current_world_index++;
    
    Level_Fade level_fade   = {};
    level_fade.active       = true;
    level_fade.timer        = 0.0f;
    level_fade.duration     = 1.5f;
    level_fade.level_number = globals.current_world_index;
    globals.current_world->level_fade = level_fade;

    if (globals.copy_of_current_world) {
        destroy_world(globals.copy_of_current_world);
        delete globals.copy_of_current_world;
        globals.copy_of_current_world = NULL;
    }
    globals.copy_of_current_world = copy_world(globals.current_world);

    globals.num_restarts_for_current_world = 0;
    
    return true;
}

bool restart_current_world() {
    MyZoneScoped;
    
    if (!globals.current_world) return false;
    assert(globals.current_world);

    globals.num_restarts_for_current_world++;
    if (globals.num_restarts_for_current_world > MAX_RESTARTS) {
        globals.program_mode = PROGRAM_MODE_END;
        globals.current_fail_msg_index = rand() % ArrayCount(fail_msgs);

        Highscore highscore;
        highscore.time                 = get_local_time();
        highscore.num_levels_completed = globals.num_worlds_completed;
        highscore.name                 = get_name_of_user();
        
        globals.highscores.push_back(highscore);
        play_sound(globals.level_fail_sfx);
        return true;
    }

    if (globals.copy_of_current_world) {
        destroy_world(globals.current_world);
        delete globals.current_world;
        globals.current_world = NULL;
    }
    globals.current_world = copy_world(globals.copy_of_current_world);
    
    return true;
}

static void draw_end_screen() {
    MyZoneScoped;
    
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
    
    int font_size = (int)(0.045f * globals.render_height);
    Dynamic_Font *font = get_font_at_size("Lora-BoldItalic", font_size);
    char text[256];
    snprintf(text, sizeof(text), "You managed to complete %d %s!", globals.num_worlds_completed, globals.num_worlds_completed == 1 ? "level" : "levels");
    int x = (globals.render_width  - font->get_string_width_in_pixels(text)) / 2;
    int y = (int)(globals.render_height * 0.75f);
    draw_text(font, text, x, y, v4(1, 1, 1, 1));
    
    snprintf(text, sizeof(text), "%s", fail_msgs[globals.current_fail_msg_index]);
    x = (globals.render_width  - font->get_string_width_in_pixels(text)) / 2;
    y -= font->character_height * 2;
    draw_text(font, text, x, y, v4(1, 1, 1, 1));

    snprintf(text, sizeof(text), "Press any key to return back to the menu");
    x = (globals.render_width - font->get_string_width_in_pixels(text)) / 2;
    y = (int)(globals.render_height * 0.25f);
    draw_text(font, text, x, y, v4(1, 1, 1, 1));
}

#ifdef __EMSCRIPTEN__

EM_BOOL resize_callback(int event_type, const void *e, void *user_data) {
    double w, h;
    emscripten_get_element_css_size("canvas", &w, &h);

    globals.window_width  = (int)w;
    globals.window_height = (int)h;
    globals.render_width  = globals.window_width;
    globals.render_height = globals.window_height;
    
    set_viewport(0, 0, globals.render_width, globals.render_height);
    
    return EM_TRUE;
}

#endif

static void toggle_fullscreen(SDL_Window *window) {
    Uint32 flags = SDL_GetWindowFlags(window);
    bool is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;

    if (is_fullscreen) {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowBordered(window, SDL_TRUE);
        SDL_SetWindowResizable(window, SDL_TRUE);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    } else {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_SetWindowBordered(window, SDL_FALSE);
    }

    SDL_GetWindowSize(window, &globals.window_width, &globals.window_height);
    init_framebuffer();
}

static SDL_Window *create_window(int width, int height, char *title) {
    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    SDL_Window *window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
    if (!window) {
        logprintf("Failed to create window!\n");
        return NULL;
    }

    globals.gl_context = SDL_GL_CreateContext(window);
    if (!globals.gl_context) {
#ifdef __EMSCRIPTEN__
        logprintf("WebGL2 failed, trying WebGL1 (ES2)...\n");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        globals.gl_context = SDL_GL_CreateContext(window);
        if (!globals.gl_context) {
            logprintf("Failed to create opengl context!\n");
            SDL_DestroyWindow(globals.window);
            return NULL;
        }
#else
        logprintf("Failed to create opengl context!\n");
        SDL_DestroyWindow(globals.window);
        return NULL;
#endif
    }
    SDL_GL_MakeCurrent(globals.window, globals.gl_context);

#ifndef __EMSCRIPTEN__
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        logprintf("Failed to initialize GLEW!\n");
        return NULL;
    }
#endif
    
#ifndef OS_WINDOWS
    SDL_RWops *rw = SDL_RWFromMem(icon_bmp, icon_bmp_len);
    SDL_Surface *icon = SDL_LoadBMP_RW(rw, 1);
    SDL_SetWindowIcon(window, icon);
    SDL_FreeSurface(icon);
#endif

#ifdef __EMSCRIPTEN__
    //emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, false, resize_callback);
#endif
    
    return window;
}

static void init_window_size(int *width, int *height) {
    if (*width == -1 && *height == -1) {
        SDL_Rect usable;
        if (SDL_GetDisplayUsableBounds(0, &usable) == 0) {
            int monitor_width  = usable.w;
            int monitor_height = usable.h;

            *width  = (int)(monitor_width  * 2.0 / 3.0);
            *height = (int)(*width * (9.0 / 16.0));
        } else {
            // Fallback if SDL fails
            *width  = 1280;
            *height = 720;
        }
    } else if (*width == -1 && *height > 0) {
        *width = (int)(*height * (16.0 / 9.0));
    } else if (*width > 0 && *height == -1) {
        *height = (int)(*width * (9.0 / 16.0));
    }
}

static void main_loop() {
    globals.num_frames_since_startup++;

    globals.frame_memory.reset();
    
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
            globals.current_level_width += 10;
            switch_to_random_world(globals.current_level_width);
            globals.num_worlds_completed++;
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
    
    respond_to_input();

    if (globals.program_mode == PROGRAM_MODE_GAME) {
        update_world(globals.current_world, (float)globals.time_info.delta_time_seconds);

        if (is_key_pressed(SDL_SCANCODE_ESCAPE) || is_key_pressed(SDL_SCANCODE_TAB) || is_key_pressed(SDL_SCANCODE_P)) {
            toggle_menu();
        }
    }

    update_menu_fade((float)globals.time_info.delta_time_seconds);
        
    if (globals.window_width > 0 && globals.window_height > 0) {
#ifndef __EMSCRIPTEN__
        set_framebuffer(globals.offscreen_buffer);
#endif
        set_viewport(0, 0, globals.render_width, globals.render_height);
        set_shader(NULL);
        
        if (globals.program_mode == PROGRAM_MODE_MAIN_MENU) {
            draw_main_menu();
        } else if (globals.program_mode == PROGRAM_MODE_GAME) {
            draw_world(globals.current_world);
            if (globals.draw_debug_hud) {
                draw_debug_hud();
            }

            do_entity_destruction(globals.current_world);
        } else if (globals.program_mode == PROGRAM_MODE_END) {
            if (globals.current_world) {
                draw_world(globals.current_world, true);
            }
            draw_end_screen();
        }

        if (globals.menu_fade.active) {
            draw_menu_fade_overlay();
        }

#ifndef __EMSCRIPTEN__
        blit_framebuffer_to_back_buffer_with_letter_boxing(globals.offscreen_buffer);
#endif
    }
    
    swap_buffers();

#ifndef __EMSCRIPTEN__
    s64 fps_cap_nanoseconds = 1000000000 / globals.time_info.fps_cap;

    s64 now_time = get_time_nanoseconds();
    s64 target_time = globals.time_info.sync_last_time + fps_cap_nanoseconds;
    s64 time_to_sleep = target_time - now_time;
    if (time_to_sleep <= 0) {
        globals.time_info.sync_last_time = now_time;
    } else if (time_to_sleep > NS_PER_SECOND) {
        globals.time_info.sync_last_time = now_time;
    } else {
        s64 safety_counter = 0;
        while (get_time_nanoseconds() < target_time) {
            safety_counter++;
            if (safety_counter > NS_PER_SECOND) break;
        }
        globals.time_info.sync_last_time = target_time;
    }
#endif

#ifndef __EMSCRIPTEN__
    MyFrameMark;
#endif
}

int main(int argc, char *argv[]) {    
#ifdef _WIN32
    void enable_dpi_awareness();
    enable_dpi_awareness();
#endif

    globals.frame_memory.init(Megabytes(32));
    
    init_log();
    defer { close_log(); };

    bool start_fullscreen = false;
#ifdef BUILD_RELEASE
    start_fullscreen = true;
#endif

    globals.window_width  = -1;
    globals.window_height = -1;
    
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strings_match(arg, "-width") || strings_match(arg, "-w")) {
            if (i == argc - 1) {
                logprintf("Tried to set window width but with no width provided!\n");
                break;
            } else {
                globals.window_width = atoi(argv[++i]);
            }
        } else if (strings_match(arg, "-height") || strings_match(arg, "-h")) {
            if (i == argc - 1) {
                logprintf("Tried to set window height but with no height provided!\n");
                break;
            } else {
                globals.window_height = atoi(argv[++i]);
            }            
        } else if (strings_match(arg, "-fullscreen") || strings_match(arg, "-f")) {
            start_fullscreen = true;
        } else if (strings_match(arg, "-windowed")) {
            start_fullscreen = false;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        logprintf("Failed to initialize SDL!\n");
        return 1;
    }
    defer { SDL_Quit(); };
    
    srand((u32)get_time_nanoseconds());

    init_window_size(&globals.window_width, &globals.window_height);
    globals.window = create_window(globals.window_width, globals.window_height, "Vertune!");
    if (!globals.window) return 1;
    if (!init_rendering(globals.window, globals.should_vsync)) return 1;
    init_shaders();
    init_framebuffer();
    init_audio();
    defer { destroy_audio(); };

    if (start_fullscreen) {
        toggle_fullscreen(globals.window);
    }

#ifdef __EMSCRIPTEN__
    globals.master_volume = 0.5f;
    globals.music_volume = 1.0f;
    globals.sfx_volume = 1.0f;
#else
    if (!load_settings()) {
        globals.master_volume = 0.5f;
        globals.music_volume = 1.0f;
        globals.sfx_volume = 1.0f;
    }
#endif
    
#ifdef BUILD_DEBUG
    load_highscores();
    for (int i = 0; i < 10; i++) {
        globals.highscores.push_back({get_local_time(), 100, get_name_of_user()});
    }
#elif defined(__EMSCRIPTEN__)
#else
    load_highscores();
#endif
    
#ifdef USE_PACKAGE
    if (!read_package(&globals.package)) {
        return 1;
    }
#endif

    load_assets();
    
    if (!create_menu_world()) return 1;
    globals.current_world = globals.menu_world;
    play_sound(globals.menu_background_music);

    globals.current_level_width = globals.start_level_width;
    //switch_to_random_world(current_level_width);

    globals.time_info.last_time = get_time_nanoseconds();
    globals.time_info.sync_last_time = get_time_nanoseconds();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    while (!globals.should_quit_game) {
        main_loop();
    }
#endif

#ifndef __EMSCRIPTEN__
    save_settings();
    save_highscores();
#endif
    
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
            stop_sound(globals.level_background_music);
            play_sound(globals.menu_background_music);
            
            destroy_world(globals.current_world);
            delete globals.current_world;
            destroy_world(globals.copy_of_current_world);
            delete globals.copy_of_current_world;
            globals.copy_of_current_world = NULL;
            globals.current_world = globals.menu_world;
            globals.num_restarts_for_current_world = 0;
            
            globals.menu_fade.fading_in = true;
            globals.menu_fade.timer = 0.0f;
            globals.menu_fade.last_world = globals.menu_world;

            globals.num_worlds_completed = 0;
            globals.current_world_index = 0;
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

bool save_settings() {
    FILE *file = fopen("settings.tmp.dat", "wb");
    if (!file) {
        logprintf("Failed to open 'settings.tmp.dat' for writing!\n");
        return false;
    }

    fwrite(&SETTINGS_FILE_MAGIC_NUMBER, sizeof(int), 1, file);
    fwrite(&SETTINGS_FILE_VERSION, sizeof(int), 1, file);
    fwrite(&globals.master_volume, sizeof(float), 1, file);
    fwrite(&globals.music_volume, sizeof(float), 1, file);
    fwrite(&globals.sfx_volume, sizeof(float), 1, file);

    int enable_lighting = globals.enable_lighting ? 1 : 0;
    fwrite(&enable_lighting, sizeof(int), 1, file);

    int hard_mode_enabled = globals.hard_mode_enabled ? 1 : 0;
    fwrite(&hard_mode_enabled, sizeof(int), 1, file);

    int draw_debug_hud = globals.draw_debug_hud ? 1 : 0;
    fwrite(&draw_debug_hud, sizeof(int), 1, file);

    int draw_outlines = globals.draw_outlines ? 1 : 0;
    fwrite(&draw_outlines, sizeof(int), 1, file);
    
    fflush(file);
    fclose(file);

    move_file("settings.tmp.dat", "settings.dat");
    
    return true;
}

bool load_settings() {
    char *filename = "settings.dat";
    FILE *file = fopen(filename, "rb");
    if (!file) {
        filename = "audio.dat";
        logprintf("Failed to open 'settings.dat' for reading! Trying 'audio.dat'!\n");
        file = fopen(filename, "rb");
        if (!file) {
            logprintf("Failed to open 'audio.dat' for reading!\n");
        }
        return false;
    }
    defer { fclose(file); };

    int magic_number;
    fread(&magic_number, sizeof(int), 1, file);
    if (magic_number != SETTINGS_FILE_MAGIC_NUMBER) {
        logprintf("Invalid magic number for '%s'\n", filename);
        return false;
    }

    int version;
    fread(&version, sizeof(int), 1, file);
    if (version <= 0 || version > SETTINGS_FILE_VERSION) {
        logprintf("Invalid version for '%s'\n", filename);
        return false;
    }

    fread(&globals.master_volume, sizeof(float), 1, file);
    clamp(&globals.master_volume, 0.0f, 1.0f);
    
    fread(&globals.music_volume, sizeof(float), 1, file);
    clamp(&globals.music_volume, 0.0f, 1.0f);

    fread(&globals.sfx_volume, sizeof(float), 1, file);
    clamp(&globals.sfx_volume, 0.0f, 1.0f);

    if (version >= 2) {
        int enable_lighting;
        fread(&enable_lighting, sizeof(int), 1, file);
        globals.enable_lighting = enable_lighting ? true : false;

        int hard_mode_enabled;
        fread(&hard_mode_enabled, sizeof(int), 1, file);
        globals.hard_mode_enabled = hard_mode_enabled ? true : false;
    }

    if (version >= 3) {
        int draw_debug_hud;
        fread(&draw_debug_hud, sizeof(int), 1, file);
        globals.draw_debug_hud = draw_debug_hud ? true : false;

        int draw_outlines;
        fread(&draw_outlines, sizeof(int), 1, file);
        globals.draw_outlines = draw_outlines ? true : false;
    }
    
    return true;
}

bool save_highscores() {
    FILE *file = fopen("hiscores.tmp.dat", "wb");
    if (!file) {
        logprintf("Failed to open 'hiscores.tmp.dat' for writing!\n");
        return false;
    }

    fwrite(&HIGHSCORE_FILE_MAGIC_NUMBER, sizeof(int), 1, file);
    fwrite(&HIGHSCORE_FILE_VERSION, sizeof(int), 1, file);

    int highscores_size = 0;
    for (int i = 0; i < globals.highscores.size(); i++) {
        if (globals.highscores[i].num_levels_completed > 0) {
            highscores_size++;
        }
    }
    
    fwrite(&highscores_size, sizeof(int), 1, file);
    for (int i = 0; i < globals.highscores.size(); i++) {
        if (globals.highscores[i].num_levels_completed > 0) {
            Highscore highscore = globals.highscores[i];

            fwrite(&highscore.time.year,   sizeof(int), 1, file);
            fwrite(&highscore.time.month,  sizeof(int), 1, file);
            fwrite(&highscore.time.day,    sizeof(int), 1, file);
            fwrite(&highscore.time.hour,   sizeof(int), 1, file);
            fwrite(&highscore.time.minute, sizeof(int), 1, file);
            fwrite(&highscore.time.second, sizeof(int), 1, file);
            
            int name_length = (int)string_length(highscore.name);
            fwrite(&name_length, sizeof(int), 1, file);
            fwrite(highscore.name, sizeof(char), name_length, file);

            fwrite(&highscore.num_levels_completed, sizeof(int), 1, file);
        }
    }

    fflush(file);
    fclose(file);

    move_file("hiscores.tmp.dat", "hiscores.dat");
    
    return true;
}

bool load_highscores() {
    FILE *file = fopen("hiscores.dat", "rb");
    if (!file) {
        logprintf("Failed to open 'hiscores.dat' for reading!\n");
        return false;
    }
    defer { fclose(file); };

    int magic_number;
    fread(&magic_number, sizeof(int), 1, file);
    if (magic_number != HIGHSCORE_FILE_MAGIC_NUMBER) {
        logprintf("Invalid magic number for 'hiscores.dat'\n");
        return false;
    }

    int version;
    fread(&version, sizeof(int), 1, file);
    if (version <= 0 || version > HIGHSCORE_FILE_VERSION) {
        logprintf("Invalid version for 'hiscores.dat'\n");
        return false;
    }

    int num_highscores;
    fread(&num_highscores, sizeof(int), 1, file);
    if (num_highscores < 0) {
        logprintf("Invalid num_highscores for 'hiscores.dat'\n");
        return false;
    }

    if (num_highscores > 0) {
        for (int i = 0; i < num_highscores; i++) {
            System_Time time = { 1970, 1, 1, 0, 0, 0 };
            char *name = "Unknown";
            
            if (version >= 2) {
                fread(&time.year,   sizeof(int), 1, file);
                fread(&time.month,  sizeof(int), 1, file);
                fread(&time.day,    sizeof(int), 1, file);
                fread(&time.hour,   sizeof(int), 1, file);
                fread(&time.minute, sizeof(int), 1, file);
                fread(&time.second, sizeof(int), 1, file);

                int name_length = 0;
                fread(&name_length, sizeof(int), 1, file);

                if (name_length > 0) {
                    name = new char[name_length + 1];
                    fread(name, sizeof(char), name_length, file);
                    name[name_length] = 0;
                } else {
                    char buf[32];
                    fread(buf, sizeof(char), name_length, file);
                }
            }
            
            int highscore;
            fread(&highscore,       sizeof(int), 1, file);

            if (highscore > 0) {
                globals.highscores.push_back({ time, highscore, name });
            }
        }
    }

    return true;
}
