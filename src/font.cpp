#include "main.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "font.h"
#include "rendering.h"
#include "memory_arena.h"

static FT_Library ft_lib;
static bool fonts_initted;

static Memory_Arena glyph_and_page_arena;

static Array <Loaded_Font *> loaded_fonts;
static Array <Dynamic_Font *> dynamic_fonts;

static int font_page_size_x;
static int font_page_size_y;

static void init_fonts(int _font_page_size_x, int _font_page_size_y) {
    font_page_size_x = _font_page_size_x;
    font_page_size_y = _font_page_size_y;

    glyph_and_page_arena.init(40000);
    
    FT_Init_FreeType(&ft_lib);
    
    fonts_initted = true;
}

static void ensure_fonts_initted() {
    if (!fonts_initted) init_fonts(1024, 1024);
}

static Loaded_Font *get_loaded_font(char *name) {
    for (int i = 0; i < loaded_fonts.count; i++) {
        Loaded_Font *font = loaded_fonts[i];
        if (strings_match(font->name, name)) return font;
    }

    char *extensions[] = {
        "ttf",
        "otf",
    };

    char full_path[1024]; bool full_path_exists = false;
    for (int i = 0; i < ArrayCount(extensions); i++) {
        snprintf(full_path, sizeof(full_path), "data/fonts/%s.%s", name, extensions[i]);
        if (file_exists(full_path)) {
            full_path_exists = true;
            break;
        }
    }

    if (!full_path_exists) {
        logprintf("No file '%s' found in data/fonts.\n", name);
        return NULL;
    }

    ensure_fonts_initted();
    
    Loaded_Font *font = new Loaded_Font();
    font->name = copy_string(name);
    FT_New_Face(ft_lib, full_path, 0, &font->face);
    loaded_fonts.add(font);
    return font;
}

#ifdef USE_PACKAGE
static Loaded_Font *get_loaded_font_from_package(char *name) {
    // Check if already loaded
    for (int i = 0; i < loaded_fonts.count; i++) {
        Loaded_Font *font = loaded_fonts[i];
        if (strings_match(font->name, name)) return font;
    }

    // Look up asset in your package
    Package_Asset_Entry *entry = find_asset_by_name(&globals.package, name);
    if (!entry) {
        logprintf("No font '%s' found in asset package.\n", name);
        return NULL;
    }

    // Pointer to font data in memory
    u8 *font_data = entry->data;
    s64 font_size = entry->size;

    ensure_fonts_initted();

    Loaded_Font *font = new Loaded_Font();
    font->name = copy_string(name);

    FT_Error error = FT_New_Memory_Face(ft_lib, font_data, (FT_Long)font_size, 0, &font->face);
    if (error) {
        logprintf("Failed to load font '%s' from memory: %d\n", name, error);
        delete font;
        return NULL;
    }

    loaded_fonts.add(font);
    return font;
}
#endif

void Dynamic_Font::load(Loaded_Font *font, int size) {
    face = font->face;
    character_height = size;
}

Glyph_Data *Dynamic_Font::get_or_load_glyph(int utf32) {
    Glyph_Data **_data = glyph_lookup.find(utf32);
    if (_data) return *_data;

    FT_Set_Pixel_Sizes(face, 0, character_height);
    
    unsigned long glyph_index = FT_Get_Char_Index(face, utf32);
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER) != 0) {
        logprintf("Failed to load glyph for %d utf32 codepoint.\n", utf32);
        return NULL;
    }
    
    Glyph_Data *data = (Glyph_Data *)glyph_and_page_arena.allocate(sizeof(*data));
    glyph_lookup.add(utf32, data);
    
    data->advance = face->glyph->advance.x >> 6;
    data->offset_x = face->glyph->bitmap_left;
    data->offset_y = face->glyph->bitmap_top;

    if (is_space(utf32)) return data;

    data->width = face->glyph->bitmap.width;
    data->height = face->glyph->bitmap.rows;
    
    advance_current_page(data, &data->x0, &data->y0);
    data->texture = current_page->texture;

    update_texture(data->texture, data->x0, data->y0, data->width, data->height, face->glyph->bitmap.buffer);
    
    return data;
}

static Font_Page *add_font_page(Dynamic_Font *font) {
    Font_Page *page = (Font_Page *)glyph_and_page_arena.allocate(sizeof(*page));
    page->cursor_x = 0;
    page->cursor_y = 0;

    page->texture = make_texture();
    load_texture_from_data(page->texture, font_page_size_x, font_page_size_y, TEXTURE_FORMAT_R8, NULL);
    
    font->font_pages.add(page);
    
    return page;
}

void Dynamic_Font::advance_current_page(Glyph_Data *data, int *old_x, int *old_y) {
    if (!current_page) current_page = add_font_page(this);
    
    bool is_already_on_a_new_line = false;
    if (current_page->cursor_x + data->width > font_page_size_x) {
        current_page->cursor_y += character_height;
        current_page->cursor_x = 0;
        is_already_on_a_new_line = true;
    }

    if (current_page->cursor_y > font_page_size_y - character_height) {
        current_page = add_font_page(this);
    }

    if (old_x) *old_x = current_page->cursor_x;
    if (old_y) *old_y = current_page->cursor_y;

    int x_pad = 8;
    current_page->cursor_x += data->width + x_pad;
    if (!is_already_on_a_new_line && current_page->cursor_x >= font_page_size_x) {
        current_page->cursor_y += character_height;
        current_page->cursor_x = 0;
    }
}

int Dynamic_Font::get_string_width_in_pixels(char *text) {
    if (!text) return 0;

    int width = 0;
    for (char *at = text; *at;) {
        int utf8_byte_count;
        int utf32 = get_codepoint(at, &utf8_byte_count);
        Glyph_Data *data = get_or_load_glyph(utf32);
        if (!data) { at += utf8_byte_count; continue; }

        if (utf32 == '\n') break;

        width += data->advance;

        at += utf8_byte_count;
    }
    return width;
}

void Dynamic_Font::prep_text(char *text, int x, int y) {
    generate_font_quads(text, x, y);
}

void Dynamic_Font::generate_font_quads(char *text, int x, int y) {
    if (!text) return;
    
    int orig_x = x;
    
    for (char *at = text; *at;) {
        int utf8_byte_count;
        int utf32 = get_codepoint(at, &utf8_byte_count);
        Glyph_Data *data = get_or_load_glyph(utf32);
        if (!data) { at += utf8_byte_count; continue; }
        
        if (utf32 == '\n') {
            x = orig_x;
            y -= character_height;
        } else {
            if (!is_space(utf32)) {
                Font_Quad quad;

                float xpos = (float)(x + data->offset_x);
                float ypos = (float)(y - (data->height - data->offset_y));
                
                quad.x0 = xpos;
                quad.y0 = ypos;
                quad.x1 = quad.x0 + (float)data->width;
                quad.y1 = quad.y0 + (float)data->height;

                quad.u0 = data->x0 / (float)font_page_size_x;
                quad.v0 = data->y0 / (float)font_page_size_y;
                quad.u1 = (data->x0 + data->width) / (float)font_page_size_x;
                quad.v1 = (data->y0 + data->height) / (float)font_page_size_y;
                
                quad.texture = data->texture;
                
                font_quads.add(quad);
            }

            x += data->advance;
        }
        
        at += utf8_byte_count;
    }
}

Dynamic_Font *get_font_at_size(char *name, int size) {
    for (int i = 0; i < dynamic_fonts.count; i++) {
        Dynamic_Font *font = dynamic_fonts[i];
        if (strings_match(font->name, name) && font->character_height == size) return font;
    }

#ifdef USE_PACKAGE
    Loaded_Font *loaded_font = get_loaded_font_from_package(name);
#else
    Loaded_Font *loaded_font = get_loaded_font(name);
#endif
    Dynamic_Font *font = new Dynamic_Font();
    font->name = copy_string(name);
    font->load(loaded_font, size);
    dynamic_fonts.add(font);
    return font;
}
