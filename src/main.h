#pragma once

#include "general.h"
#include "geometry.h"
#include "array.h"
#include "hash_table.h"

#include "os_specific.h"

struct Shader;
struct Framebuffer;
struct World;

struct Time_Info {
    u64 last_time = 0;

    u64 real_world_time = 0;
    u64 delta_time = 0;
    double delta_time_seconds = 0.0;

    // For fps debug info.
    u64 num_frames_since_last_fps_update = 0;
    double accumulated_fps_dt = 0.0;
    double fps_dt = 0.0;
    
    int fps_cap = 120;
};

struct Global_Variables {
    Array <Event> events_this_frame;
    Array <Window_Resize_Record> window_resizes;
    bool should_quit_game = false;
    bool should_vsync = true;
    
    Window_Type window = NULL;
    int window_width = 0;
    int window_height = 0;

    Framebuffer *offscreen_buffer = NULL;
    int render_width  = 0;
    int render_height = 0;
    
    Time_Info time_info;
    
    Shader *shader_color = NULL;
    Shader *shader_texture = NULL;
    Shader *shader_text = NULL;

    Matrix4 object_to_proj_matrix;
    Matrix4 view_to_proj_matrix;
    Matrix4 world_to_view_matrix;
    Matrix4 object_to_world_matrix;
    
    World *current_world = NULL;

    Array <char *> world_names;
    int current_world_index = -1;
    bool should_switch_worlds = false;
};

extern Global_Variables globals;

bool is_key_down(int key_code);
bool is_key_pressed(int key_code);
bool was_key_just_released(int key_code);

bool switch_to_world(char *world_name);
void switch_to_next_world();
