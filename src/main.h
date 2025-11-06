#pragma once

#include "general.h"
#include "geometry.h"
#include "array.h"
#include "hash_table.h"
#include "packager/packager.h"

#include <SDL.h>

struct Shader;
struct Framebuffer;
struct World;
struct Texture;
struct Sound;

const int MAX_RESTARTS = 3;
const int MAX_FPS_CAP = 120;
const int MAX_SLOW_FRAMES = 120;

const int AUDIO_FILE_MAGIC_NUMBER = 0x504C4159;
const int AUDIO_FILE_VERSION = 1;

const int HIGHSCORE_FILE_MAGIC_NUMBER = 0x48534601;
const int HIGHSCORE_FILE_VERSION = 1;

struct Fade_Transition {
    bool active = false;
    float timer = 0.0f;
    float duration = 1.0f;
    bool fading_in = true;
    World *last_world = NULL;
};

enum Program_Mode {
    PROGRAM_MODE_MAIN_MENU,
    PROGRAM_MODE_GAME,
    PROGRAM_MODE_END,
};

struct Time_Info {
    s64 last_time = 0;

    s64 real_world_time = 0;
    s64 delta_time = 0;
    double delta_time_seconds = 0.0;

    // For fps debug info.
    s64 num_frames_since_last_fps_update = 0;
    double accumulated_fps_dt = 0.0;
    double fps_dt = 0.0;

    // Fps cap variables.
    int fps_cap = 120;
    int slow_frame_count = 0;
    int fast_frame_count = 0;
};

struct Global_Variables {
    //Array <Event> events_this_frame;
    //Array <Window_Resize_Record> window_resizes;
    bool should_quit_game = false;
    bool should_vsync = true;
    
    SDL_Window *window = NULL;
    SDL_GLContext gl_context = NULL;
    int window_width = 0;
    int window_height = 0;

    Framebuffer *offscreen_buffer = NULL;
    int render_width  = 0;
    int render_height = 0;
    
    Time_Info time_info;
    Program_Mode program_mode = PROGRAM_MODE_MAIN_MENU;
    
    Shader *shader_color = NULL;
    Shader *shader_texture = NULL;
    Shader *shader_text = NULL;

    Matrix4 object_to_proj_matrix;
    Matrix4 view_to_proj_matrix;
    Matrix4 world_to_view_matrix;
    Matrix4 object_to_world_matrix;

    World *menu_world = NULL;    
    World *current_world = NULL;
    World *copy_of_current_world = NULL;
    int num_restarts_for_current_world = 0;
    int current_fail_msg_index = -1;
    
    int current_world_index = 0;
    bool should_switch_worlds = false;
    int num_worlds_completed = 0;
    int start_level_width = 30;

    int num_frames_since_startup = 0;

    bool draw_debug_hud = false;

    Fade_Transition menu_fade;

    Texture *full_heart        = NULL;
    Texture *half_heart        = NULL;
    Texture *empty_heart       = NULL;
    Texture *restart_taken     = NULL;
    Texture *restart_available = NULL;

    Sound *level_background_music = NULL;
    Sound *menu_background_music = NULL;
    Sound *jump_sfx = NULL;
    Sound *damage_sfx = NULL;
    Sound *enemy_kill_sfx = NULL;
    Sound *level_complete_sfx = NULL;
    Sound *death_sfx = NULL;
    Sound *level_fail_sfx = NULL;
    Sound *coin_pickup_sfx = NULL;

    Sound *menu_change_option = NULL;
    Sound *menu_select = NULL;
    Sound *exit_menu = NULL;

    float master_volume = 0.5f;
    float sfx_volume = 1.0f;
    float music_volume = 1.0f;

    Array <int> highscores;

#ifdef USE_PACKAGE
    Package package;
#endif
};

extern Global_Variables globals;

bool is_key_down(int key_code);
bool is_key_pressed(int key_code);
bool was_key_just_released(int key_code);

double nanoseconds_to_seconds(u64 nanoseconds);
u64 seconds_to_nanoseconds(double seconds);

void toggle_menu();
bool switch_to_random_world(int total_width);
bool restart_current_world();

void start_menu_fade(World *world);
void update_menu_fade(float dt);
void draw_menu_fade_overlay();

bool save_audio_settings();
bool load_audio_settings();

bool save_highscores();
bool load_highscores();
