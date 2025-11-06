#include "main.h"
#include "rendering.h"
#include "resource_manager.h"
#include "font.h"
#include "world.h"
#include "audio.h"

#include <stdio.h>

static const Vector4 MENU_COLOR_TEXT      = v4(1.0f, 0.98f, 0.95f, 1); // clean warm white
static const Vector4 MENU_COLOR_HIGHLIGHT = v4(1.0f, 0.45f, 0.2f, 1);  // rich orange accent
static const Vector4 MENU_COLOR_SUBTEXT   = v4(0.45f, 0.5f, 0.6f, 1);  // muted blue-gray
static const Vector4 MENU_COLOR_SHADOW    = v4(0.0f, 0.2f, 0.4f, 0.7f); // optional text shadow

enum Menu_Page {
    MENU_PAGE_MAIN,
    MENU_PAGE_CONTROLS,
    MENU_PAGE_SETTINGS,
    MENU_PAGE_HIGHSCORES,
};

static float draw_y_offset_for_highscores_due_to_scrolling = 0.0f;
static int bottom_y_after_drawing_highscores = 0;

static int current_menu_choice = 0;
static Menu_Page current_menu_page = MENU_PAGE_MAIN;

static bool asking_for_restart_confirmation = false;
static bool asking_for_quit_confirmation = false;

static double time_of_restart_confirmation = -1.0;
static double time_of_quit_confirmation = -1.0;
static double last_keypress_time = 0.0;

static int index_resume     = -1;
static int index_restart    = -1;
static int index_controls   = -1;
static int index_settings   = -1;
static int index_highscores = -1;
static int index_quit       = -1;

static int menu_items_total = 4; // Will get reset after first draw phase.
static int num_menu_items_drawn = 0;

static int get_x_pad() {
    int x_pad = (int)(globals.render_width * 0.01f);
    return x_pad;
}

static bool menu_can_accept_input() {
    return !globals.menu_fade.active || globals.menu_fade.fading_in == false;
}

void toggle_menu() {
    if (globals.program_mode == PROGRAM_MODE_MAIN_MENU) {
        globals.program_mode = PROGRAM_MODE_GAME;
        current_menu_page = MENU_PAGE_MAIN;
    } else if (globals.program_mode == PROGRAM_MODE_GAME) {
        globals.program_mode = PROGRAM_MODE_MAIN_MENU;
    }

    asking_for_restart_confirmation = false;
    asking_for_quit_confirmation = false;
    current_menu_choice = 0;
}

static void advance_menu_choice(int delta) {
    if (!menu_can_accept_input()) return;
    
    current_menu_choice += delta;

    if (current_menu_page == MENU_PAGE_MAIN) {
        if (current_menu_choice == index_restart) {
            if (globals.current_world == globals.menu_world) {
                current_menu_choice += delta < 0 ? -1 : 1;
            }
        }
        
        if (current_menu_choice < 0) {
            current_menu_choice += menu_items_total;
        }
        
        if (current_menu_choice >= menu_items_total) {
            current_menu_choice -= menu_items_total;
        }

        play_sound(globals.menu_change_option);
    } else if (current_menu_page == MENU_PAGE_SETTINGS) {
        if (current_menu_choice < 0) {
            current_menu_choice += 3;
        }

        if (current_menu_choice >= 3) {
            current_menu_choice -= 3;
        }

        play_sound(globals.menu_change_option);
    }
    
    asking_for_restart_confirmation = false;
    asking_for_quit_confirmation = false;
}

static void handle_enter() {
    if (!menu_can_accept_input()) return;
    if (current_menu_page != MENU_PAGE_MAIN) return;
    
    play_sound(globals.menu_select);

    int choice = current_menu_choice;
    
    if (choice == index_resume) {
        if (globals.current_world == globals.menu_world) {
            switch_to_random_world(globals.start_level_width);
            stop_sound(globals.menu_background_music);
            play_sound(globals.level_background_music);
        }
        toggle_menu();
    } else if (choice == index_restart) {
        if (asking_for_restart_confirmation) {
            restart_current_world();
            toggle_menu();
        } else {
            if (globals.current_world != globals.menu_world) {
                asking_for_restart_confirmation = true;
            }
        }
    } else if (choice == index_controls) {
        current_menu_page = MENU_PAGE_CONTROLS;
    } else if (choice == index_settings) {
        current_menu_page = MENU_PAGE_SETTINGS;
        current_menu_choice = 0;
    } else if (choice == index_highscores) {
        current_menu_page = MENU_PAGE_HIGHSCORES;
    } else if (choice == index_quit) {
        if (asking_for_quit_confirmation) {
            if (globals.current_world != globals.menu_world) {
                globals.highscores.add(globals.num_worlds_completed);
            }
            globals.should_quit_game = true;
        }
        else asking_for_quit_confirmation = true;
    }
}

static int draw_item(char *text, Dynamic_Font *font, int center_x, int y, Vector4 color) {
    int width = font->get_string_width_in_pixels(text);
    int x_pad = get_x_pad();
    
    int index = num_menu_items_drawn;
    
    Vector4 item_color = MENU_COLOR_TEXT;
    if (index == index_restart) {
        if (globals.current_world == globals.menu_world) {
            item_color = v4(0.4f, 0.4f, 0.4f, 1.0f);
        }
    }
    if (index == current_menu_choice) {
        float pad_x = 20.0f;
        float pad_y = font->character_height * 0.4f;
        float text_width = (float)font->get_string_width_in_pixels(text);
        float rect_height = font->character_height * 1.4f;

        Vector4 bg_color = v4(1.0f, 0.85f, 0.4f, 0.15f);

        set_shader(globals.shader_color);
        
        immediate_begin();
        immediate_quad(
            v2(x_pad - pad_x, y - pad_y),
            v2(text_width + pad_x * 2, rect_height),
            bg_color);
        immediate_flush();

        double now = nanoseconds_to_seconds(globals.time_info.real_world_time);
        double t = 0.5 + 0.5 * sin(now * 2.0);
        t = 0.3 + 0.7 * t;
        Vector4 animated_color = lerp(MENU_COLOR_HIGHLIGHT, v4(1, 0.9f, 0.5f, 1), (float)t);

        set_shader(globals.shader_text);
        
        draw_text(font, text, x_pad, (int)(y - font->character_height * 0.05f), MENU_COLOR_SHADOW);
        draw_text(font, text, x_pad, y, animated_color);
    } else {
        set_shader(globals.shader_text);
        draw_text(font, text, x_pad, y, item_color);
    }

    num_menu_items_drawn++;
    return num_menu_items_drawn - 1;
}

static void draw_menu_choices() {
    int BIG_FONT_SIZE = (int)(globals.render_height * 0.0725f);
    auto body_font    = get_font_at_size("Lora-Bold", (int)(BIG_FONT_SIZE * 0.9f));
    
    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    int center_x = globals.render_width / 2;
    int cursor_y = (int)(globals.render_height * 0.7f);

    Dynamic_Font *font = body_font;
    int stride = (int)(1.4f * font->character_height);

    Vector4 item_color = v4(0.5f, 0.5f, 0.5f, 1);
    Vector4 start_color = item_color;

    //
    // Menu item: Resume Game
    //

    char *text = "Resume";
    if (globals.current_world == globals.menu_world) text = "Start";
    index_resume = draw_item(text, font, center_x, cursor_y, start_color);
    cursor_y -= stride;

    //
    // Menu item: Restart
    //

    text = "Restart";
    if (asking_for_restart_confirmation) text = "Restart? Are you sure?";
    index_restart = draw_item(text, font, center_x, cursor_y, start_color);
    cursor_y -= stride;
    
    //
    // Menu item: Controls
    //

    index_controls = draw_item("Controls", font, center_x, cursor_y, start_color);
    cursor_y -= stride;

    //
    // Menu item: Settings
    //

    index_settings = draw_item("Settings", font, center_x, cursor_y, start_color);
    cursor_y -= stride;

    //
    // Menu item: Highscores
    //

    index_highscores = draw_item("Highscores", font, center_x, cursor_y, start_color);
    cursor_y -= stride;
    
    //
    // Menu item: Quit
    //

    text = "Quit";
    if (asking_for_quit_confirmation) text = "Quit? Are you sure?";
    index_quit = draw_item(text, font, center_x, cursor_y, start_color);

    menu_items_total = num_menu_items_drawn;
    num_menu_items_drawn = 0;
}

static void draw_title() {
    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    int BIG_FONT_SIZE = (int)(globals.render_height * 0.0725f);
    auto title_font = get_font_at_size("Lora-BoldItalic", (int)(BIG_FONT_SIZE * 1.6f));

    Dynamic_Font *font = title_font;
    char *text = "Vertune!";
    int x = get_x_pad();
    int y = globals.render_height - (int)(font->character_height * 1.5f);
    draw_text(font, text, x, y, v4(1, 1, 1, 1));
}

static void draw_pair(char *pair_a, char *pair_b, Dynamic_Font *font, int cursor_y) {
    char *text = pair_a;
    int x = 0;
    int y = cursor_y;
    draw_text(font, text, x, y, v4(1, 1, 1, 1));

    text = pair_b;
    x = globals.render_width - font->get_string_width_in_pixels(text);
    draw_text(font, text, x, y, v4(1, 1, 1, 1));
}

static void draw_controls() {
    clear_framebuffer(0.1f, 0.1f, 0.1f, 1.0f);
    
    int BIG_FONT_SIZE = (int)(globals.render_height * 0.0725f);
    auto title_font   = get_font_at_size("Lora-BoldItalic", (int)(BIG_FONT_SIZE * 1.6f));
    auto body_font    = get_font_at_size("Lora-Bold", (int)(BIG_FONT_SIZE * 0.9f));

    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    Dynamic_Font *font = body_font;
    char *text = "Controls";
    int x = (globals.render_width - font->get_string_width_in_pixels(text)) / 2;
    int y = globals.render_height - (int)(font->character_height * 1.5f);
    draw_text(font, text, x, y, v4(1, 1, 1, 1));

    struct Control_Pair {
        char *a;
        char *b;
    };

    Control_Pair pairs[] = {
        { "Move Left", "A" },
        { "Move Right", "D" },
        { "Jump", "W" },
        { "Toggle FPS hud", "F" },
        { "Toggle fullscreen", "F11" },
    };

    int cursor_y = (int)(globals.render_height * 0.62);
    int stride = (int)(1.4f * font->character_height);
    
    for (int i = 0; i < ArrayCount(pairs); i++) {
        auto pair = pairs[i];

        draw_pair(pair.a, pair.b, body_font, cursor_y);
        cursor_y -= stride;
    }
}

static void draw_slider(int x, int y, float value, float max_value, float width, float height, Vector4 knob_color) {
    set_shader(globals.shader_color);

    Vector4 grey_color = v4(0.4f, 0.4f, 0.4f, 1.0f);
    
    immediate_begin();
    immediate_quad(v2((float)x, (float)y), v2(width, height), grey_color);

    Vector2 knob_size = v2(height, height * 2.0f);
    
    Vector2 knob_position;
    knob_position.x = (float)x + ((value / max_value) * width);
    knob_position.y = y - knob_size.y * 0.5f + height * 0.5f;
    
    immediate_quad(knob_position, knob_size, knob_color);

    immediate_flush();
}

static void draw_settings() {
    clear_framebuffer(0.1f, 0.1f, 0.1f, 1.0f);
    
    int BIG_FONT_SIZE = (int)(globals.render_height * 0.0725f);
    auto title_font   = get_font_at_size("Lora-BoldItalic", (int)(BIG_FONT_SIZE * 1.6f));
    auto body_font    = get_font_at_size("Lora-Bold", (int)(BIG_FONT_SIZE * 0.9f));

    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    Dynamic_Font *font = body_font;
    char *text = "Settings";
    int x = (globals.render_width - font->get_string_width_in_pixels(text)) / 2;
    int y = globals.render_height - (int)(font->character_height * 1.5f);
    draw_text(font, text, x, y, v4(1, 1, 1, 1));

    int cursor_y = (int)(globals.render_height * 0.62);
    int stride = (int)(1.4f * font->character_height);
    
    //
    // Draw master volume
    //
    {
        Vector4 knob_color = (current_menu_choice == 0) ? v4(1, 0.8f, 0.2f, 1) : v4(1, 1, 1, 1);
        
        set_shader(globals.shader_text);
        char *text = "Master volume: ";
        x = (int)(0.005f * globals.render_width);
        draw_text(font, text, x, cursor_y, v4(1, 1, 1, 1));

        x = (int)(globals.render_width * 0.45f);
        draw_slider(x, cursor_y, globals.master_volume, 1.0f, 0.45f * globals.render_width, font->character_height * 0.5f, knob_color);

        cursor_y -= stride;
    }

    //
    // Draw music volume
    //
    {
        Vector4 knob_color = (current_menu_choice == 1) ? v4(1, 0.8f, 0.2f, 1) : v4(1, 1, 1, 1);
        
        set_shader(globals.shader_text);
        char *text = "Music volume: ";
        x = (int)(0.005f * globals.render_width);
        draw_text(font, text, x, cursor_y, v4(1, 1, 1, 1));

        x = (int)(globals.render_width * 0.45f);
        draw_slider(x, cursor_y, globals.music_volume, 1.0f, 0.45f * globals.render_width, font->character_height * 0.5f, knob_color);

        cursor_y -= stride;
    }

    //
    // Draw master volume
    //
    {
        Vector4 knob_color = (current_menu_choice == 2) ? v4(1, 0.8f, 0.2f, 1) : v4(1, 1, 1, 1);
        
        set_shader(globals.shader_text);
        char *text = "Sfx volume: ";
        x = (int)(0.005f * globals.render_width);
        draw_text(font, text, x, cursor_y, v4(1, 1, 1, 1));

        x = (int)(globals.render_width * 0.45f);
        draw_slider(x, cursor_y, globals.sfx_volume, 1.0f, 0.45f * globals.render_width, font->character_height * 0.5f, knob_color);
    }
}

static float get_max_draw_y_offset_for_highscores_due_to_scrolling() {
    int BIG_FONT_SIZE = (int)(globals.render_height * 0.0725f);
    auto body_font    = get_font_at_size("Lora-Bold", (int)(BIG_FONT_SIZE * 0.9f));
    Dynamic_Font *font = body_font;

    int cursor_y = (int)(globals.render_height * 0.8);
    int stride = (int)(1.4f * font->character_height);

    for (int hiscore : globals.highscores) {
        cursor_y -= stride;
    }

    return (float)(-cursor_y);
}

static void draw_highscores() {
    clear_framebuffer(0.1f, 0.1f, 0.1f, 1.0f);
    
    int BIG_FONT_SIZE = (int)(globals.render_height * 0.0725f);
    auto title_font   = get_font_at_size("Lora-BoldItalic", (int)(BIG_FONT_SIZE * 1.6f));
    auto body_font    = get_font_at_size("Lora-Bold", (int)(BIG_FONT_SIZE * 0.9f));

    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height, draw_y_offset_for_highscores_due_to_scrolling);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    Dynamic_Font *font = body_font;
    char text[256];
    snprintf(text, sizeof(text), "%s", "Highscores");
    int x = (globals.render_width - font->get_string_width_in_pixels(text)) / 2;
    int y = globals.render_height - (int)(font->character_height * 1.5f);
    draw_text(font, text, x, y, v4(1, 1, 1, 1));

    int cursor_y = (int)(globals.render_height * 0.8);
    int stride = (int)(1.4f * font->character_height);

    for (int hiscore : globals.highscores) {
        snprintf(text, sizeof(text), "%d", hiscore);
        x = (globals.render_width - font->get_string_width_in_pixels(text)) / 2;
        draw_text(font, text, x, cursor_y, v4(1, 1, 1, 1));
        cursor_y -= stride;
    }

    bottom_y_after_drawing_highscores = cursor_y;
}

void draw_main_menu() {  
    draw_world(globals.current_world, true);
    
    set_shader(globals.shader_color);
    rendering_2d(globals.render_width, globals.render_height, matrix4_identity());
    
    immediate_begin();
    immediate_quad(v2(0, 0), v2((float)globals.render_width, (float)globals.render_height), v4(0, 0, 0, 0.1f));
    immediate_flush();
    
    switch (current_menu_page) {
        case MENU_PAGE_MAIN: {
            draw_title();
            draw_menu_choices();
        } break;

        case MENU_PAGE_CONTROLS: {
            draw_controls();
            if (is_key_pressed(SDL_SCANCODE_ESCAPE) || is_key_pressed(SDL_SCANCODE_BACKSPACE)) {
                current_menu_page = MENU_PAGE_MAIN;
                current_menu_choice = index_controls;
                play_sound(globals.exit_menu);
            }
        } break;

        case MENU_PAGE_SETTINGS: {
            float *value_to_change = NULL;
            if (current_menu_choice == 0) value_to_change = &globals.master_volume;
            else if (current_menu_choice == 1) value_to_change = &globals.music_volume;
            else if (current_menu_choice == 2) value_to_change = &globals.sfx_volume;
            
            if (is_key_down(SDL_SCANCODE_LEFT)) {
                if (value_to_change) {
                    *value_to_change -= 0.01f;

                    if (*value_to_change < 0.0f) {
                        *value_to_change = 0.0f;
                    }
                }

                update_volumes();
            } else if (is_key_down(SDL_SCANCODE_RIGHT)) {
                if (value_to_change) {
                    *value_to_change += 0.01f;

                    if (*value_to_change > 1.0f) {
                        *value_to_change = 1.0f;
                    }
                }

                update_volumes();
            }
            
            draw_settings();
            if (is_key_pressed(SDL_SCANCODE_ESCAPE) || is_key_pressed(SDL_SCANCODE_BACKSPACE)) {
                current_menu_page = MENU_PAGE_MAIN;
                current_menu_choice = index_settings;
                play_sound(globals.exit_menu);
            }
        } break;

        case MENU_PAGE_HIGHSCORES: {
            draw_highscores();
            if (is_key_pressed(SDL_SCANCODE_ESCAPE) || is_key_pressed(SDL_SCANCODE_BACKSPACE)) {
                current_menu_page = MENU_PAGE_MAIN;
                current_menu_choice = index_highscores;
                play_sound(globals.exit_menu);
            }
        } break;
    }

    if (current_menu_page == MENU_PAGE_HIGHSCORES) {
        float delta = 20.0f;
        
        if (is_key_down(SDL_SCANCODE_UP)) {
            draw_y_offset_for_highscores_due_to_scrolling -= delta;
        } else if (is_key_down(SDL_SCANCODE_DOWN)) {
            draw_y_offset_for_highscores_due_to_scrolling += delta;
        }
        
        if (draw_y_offset_for_highscores_due_to_scrolling < 0.0f) {
            draw_y_offset_for_highscores_due_to_scrolling = 0.0f;
        }

        float bottom = -(float)bottom_y_after_drawing_highscores;
        if (draw_y_offset_for_highscores_due_to_scrolling > bottom) {
            draw_y_offset_for_highscores_due_to_scrolling = bottom;
        }
    } else {
        if (is_key_pressed(SDL_SCANCODE_UP))   advance_menu_choice(-1);
        if (is_key_pressed(SDL_SCANCODE_DOWN)) advance_menu_choice(+1);
    }
    
    if (is_key_pressed(SDL_SCANCODE_RETURN) || is_key_pressed(SDL_SCANCODE_KP_ENTER)) {
        handle_enter();
    }
}
