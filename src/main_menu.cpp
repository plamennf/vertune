#include "main.h"
#include "rendering.h"
#include "resource_manager.h"
#include "font.h"

enum Menu_Page {
    MENU_PAGE_MAIN,
    MENU_PAGE_CONTROLS,
};

static int current_menu_choice = 0;
static Menu_Page current_menu_page = MENU_PAGE_MAIN;

static bool asking_for_restart_confirmation = false;
static bool asking_for_quit_confirmation = false;

static double time_of_restart_confirmation = -1.0;
static double time_of_quit_confirmation = -1.0;
static double last_keypress_time = 0.0;

static int index_resume   = -1;
static int index_controls = -1;
static int index_restart  = -1;
static int index_quit     = -1;

static int menu_items_total = 4; // Will get reset after first draw phase.
static int num_menu_items_drawn = 0;

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
    current_menu_choice += delta;

    if (current_menu_choice < 0)                 current_menu_choice += menu_items_total;
    if (current_menu_choice >= menu_items_total) current_menu_choice -= menu_items_total;

    asking_for_restart_confirmation = false;
    asking_for_quit_confirmation = false;
}

static void handle_enter() {
    int choice = current_menu_choice;
    
    if (choice == index_resume) {
        toggle_menu();
    } else if (choice == index_controls) {
        current_menu_page = MENU_PAGE_CONTROLS;
    } else if (choice == index_restart) {
        if (asking_for_restart_confirmation) {
            switch_to_first_world();
            toggle_menu();
        }
        else asking_for_restart_confirmation = true;
    } else if (choice == index_quit) {
        if (asking_for_quit_confirmation) globals.should_quit_game = true;
        else asking_for_quit_confirmation = true;
    }
}

static int draw_item(char *text, Dynamic_Font *font, int center_x, int y, Vector4 color) {
    int width = font->get_string_width_in_pixels(text);

    int index = num_menu_items_drawn;
    
    Vector4 item_color = color;
    if (index == current_menu_choice) {
        item_color = v4(1, 173/255.0f, 0, 1);

        Vector4 non_white = v4(1, 222/255.0f, 0, 1);
        Vector4 white = v4(1, 1, 1, 1);

        double now = nanoseconds_to_seconds(globals.time_info.real_world_time);
        double t = cos(now * 3.0);
        t *= t;

        t = .4 + .54 * t;
        Vector4 backing_color = lerp(non_white, white, (float)t);

        int offset = font->character_height / 40;
        draw_text(font, text, center_x - width / 2 + offset, y - offset, item_color);
        draw_text(font, text, center_x - width / 2, y, backing_color);
    } else {
        draw_text(font, text, center_x - width / 2, y, item_color);
    }

    num_menu_items_drawn++;
    return num_menu_items_drawn - 1;
}

static void draw_menu_choices() {
    int BIG_FONT_SIZE = (int)(globals.render_height * 0.0725f);
    auto body_font    = get_font_at_size("KarminaBold", (int)(BIG_FONT_SIZE * 0.9f));
    
    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    int center_x = globals.render_width / 2;
    int cursor_y = (int)(globals.render_height * 0.62);

    Dynamic_Font *font = body_font;
    int stride = (int)(1.4f * font->character_height);

    Vector4 item_color = v4(0.5f, 0.5f, 0.5f, 1);
    Vector4 start_color = item_color;

    //
    // Menu item: Resume Game
    //

    index_resume = draw_item("Resume", font, center_x, cursor_y, start_color);
    cursor_y -= stride;

    //
    // Menu item: Controls
    //

    index_controls = draw_item("Controls", font, center_x, cursor_y, start_color);
    cursor_y -= stride;

    //
    // Menu item: Restart
    //

    char *text = "Restart";
    if (asking_for_restart_confirmation) text = "Restart? Are you sure?";
    index_restart = draw_item(text, font, center_x, cursor_y, start_color);
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
    auto title_font = get_font_at_size("KarminaBoldItalic", (int)(BIG_FONT_SIZE * 1.6f));

    Dynamic_Font *font = title_font;
    char *text = "Platformer!";
    int x = (globals.render_width - font->get_string_width_in_pixels(text)) / 2;
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
    int BIG_FONT_SIZE = (int)(globals.render_height * 0.0725f);
    auto title_font   = get_font_at_size("KarminaBoldItalic", (int)(BIG_FONT_SIZE * 1.6f));
    auto body_font    = get_font_at_size("KarminaBold", (int)(BIG_FONT_SIZE * 0.9f));

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
    };

    int cursor_y = (int)(globals.render_height * 0.62);
    int stride = (int)(1.4f * font->character_height);
    
    for (int i = 0; i < ArrayCount(pairs); i++) {
        auto pair = pairs[i];

        draw_pair(pair.a, pair.b, body_font, cursor_y);
        cursor_y -= stride;
    }
}

void draw_main_menu() {
    clear_framebuffer(0, 0.3f, 0.4f, 1);

    switch (current_menu_page) {
        case MENU_PAGE_MAIN: {
            draw_title();
            draw_menu_choices();
        } break;

        case MENU_PAGE_CONTROLS: {
            draw_controls();
            if (is_key_pressed(KEY_ESCAPE)) {
                current_menu_page = MENU_PAGE_MAIN;
            }
        } break;
    }

    if (is_key_pressed(KEY_UP_ARROW))   advance_menu_choice(-1);
    if (is_key_pressed(KEY_DOWN_ARROW)) advance_menu_choice(+1);
    
    if (is_key_pressed(KEY_ENTER)) {
        handle_enter();
    }
}
