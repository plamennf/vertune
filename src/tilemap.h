#pragma once

struct World;

struct Tilemap {
    int width;
    int height;

    int num_colors;
    Vector4 *colors;

    int num_collidable_ids;
    u8 *collidable_ids;

    u8 *tiles;
};

bool load_tilemap(Tilemap *tilemap, char *filepath);
void draw_tilemap(Tilemap *tilemap, World *world);
