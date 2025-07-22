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

bool is_tile_id_collidable(Tilemap *tilemap, u8 tile_id);
u8 get_tile_id_at(Tilemap *tilemap, Vector2 position);
