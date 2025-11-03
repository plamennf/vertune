#pragma once

struct World;

struct Tilemap {
    int width = 0;
    int height = 0;

    int num_colors = 0;
    Vector4 *colors = NULL;

    int num_collidable_ids = 0;
    u8 *collidable_ids = NULL;

    u8 *tiles = NULL;
};

bool load_tilemap(Tilemap *tilemap, char *filepath);
void draw_tilemap(Tilemap *tilemap, World *world);

bool is_tile_id_collidable(Tilemap *tilemap, u8 tile_id);
u8 get_tile_id_at(Tilemap *tilemap, Vector2 position);
