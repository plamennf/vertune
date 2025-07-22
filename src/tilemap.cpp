#include "main.h"
#include "tilemap.h"
#include "text_file_handler.h"
#include "rendering.h"
#include "world.h"

#include <stdio.h>

#define TILEMAP_FILE_VERSION 1

bool load_tilemap(Tilemap *tilemap, char *filepath) {
    Text_File_Handler handler;
    if (!start_file(&handler, filepath)) return false;
    defer { end_file(&handler); };
    
    if (handler.version < 1) {
        report_error(&handler, "Invalid version number for a tilemap file!");
        return false;
    }

    char *line = consume_next_line(&handler);
    if (!line) {
        report_error(&handler, "Expected width directive instead found end-of-file!");
        return false;
    }
    if (!starts_with(line, "width")) {
        report_error(&handler, "Expected width directive instead found '%s'!", line);
        return false;
    }
    line += string_length("width");
    line = eat_spaces(line);
    line = eat_trailing_spaces(line);

    int width = atoi(line);
    if (width <= 0) {
        report_error(&handler, "Width must be at least 1, but instead it is '%d'!", width);
        return false;
    }

    line = consume_next_line(&handler);
    if (!line) {
        report_error(&handler, "Expected height directive instead found end-of-file!");
        return false;
    }
    if (!starts_with(line, "height")) {
        report_error(&handler, "Expected height directive instead found '%s'!", line);
        return false;
    }
    line += string_length("height");
    line = eat_spaces(line);
    line = eat_trailing_spaces(line);

    int height = atoi(line);
    if (height <= 0) {
        report_error(&handler, "Height must be at least 1, but instead it is '%d'!", height);
        return false;
    }

    Array <Vector4> colors;
    Array <u8> collidable_ids;
    for (;;) {
        line = consume_next_line(&handler);
        if (!line) {
            report_error(&handler, "The file is too short to be a valid tilemap file!");
            return false;
        }

        if (starts_with(line, "color")) {
            line += string_length("color");
            line = eat_spaces(line);
            line = eat_trailing_spaces(line);

            Vector3 color;
            int num_matches = sscanf(line, "%f %f %f", &color.r, &color.g, &color.b);
            if (num_matches != 3) {
                report_error(&handler, "Color must have 3 components, instead found only %d!", num_matches);
                return false;
            }

            Vector4 fcolor;
            fcolor.r = color.r / 255.0f;
            fcolor.g = color.g / 255.0f;
            fcolor.b = color.b / 255.0f;
            fcolor.a = 255.0f;
            colors.add(fcolor);
        } else if (starts_with(line, "collidable_ids")) {
            line += string_length("collidable_ids");
            line = eat_spaces(line);
            line = eat_trailing_spaces(line);

            for (;;) {
                char *rhs = break_by_space(line);
                if (!rhs) break;

                u8 id = (u8)atoi(line);
                collidable_ids.add(id);
                
                line = rhs;
            }
            
            break;
        }
    }

    Array <char *> lines;
    lines.resize(height);
    for (int i = 0; i < height; i++) {
        lines[i] = consume_next_line(&handler);
        if (lines[i] == NULL) {
            report_error(&handler, "File is too short: the rows of ids must match height!");
            return false;
        }
    }

    u8 *tiles = new u8[width * height];
    for (int y = 0; y < height; y++) {
        char *line = lines[height - y - 1];
        line = eat_spaces(line);
        line = eat_trailing_spaces(line);
        for (int x = 0; x < width; x++) {
            char *rhs = break_by_comma(line);
            if (!rhs) {
                report_error(&handler, "File is too short: the columns of ids must match width!");
                delete [] tiles;
                return false;
            }

            u8 id = (u8)atoi(line);
            tiles[y * width + x] = id;
            
            line = rhs;
        }
    }

    tilemap->width  = width;
    tilemap->height = height;

    tilemap->num_colors = colors.count;
    tilemap->colors     = colors.copy_to_array();

    tilemap->num_collidable_ids = collidable_ids.count;
    tilemap->collidable_ids     = collidable_ids.copy_to_array();

    tilemap->tiles = tiles;
    
    return true;
}

void draw_tilemap(Tilemap *tilemap, World *world) {
    float xpos = 0.0f;
    float ypos = 0.0f;
    
    for (int y = 0; y < tilemap->height; y++) {
        for (int x = 0; x < tilemap->width; x++) {
            u8 tile_id = tilemap->tiles[y * tilemap->width + x];
            if (tile_id > 0) {
                assert(tile_id > 0 && tile_id <= tilemap->num_colors);
                Vector4 color = tilemap->colors[tile_id - 1];

                Vector2 screen_space_position = world_space_to_screen_space(world, v2(xpos, ypos));
                Vector2 screen_space_size     = world_space_to_screen_space(world, v2(1, 1));
                
                immediate_quad(screen_space_position, screen_space_size, color);
            }

            xpos += 1.0f;
        }
        ypos += 1.0f;
        xpos = 0.0f;
    }
}
