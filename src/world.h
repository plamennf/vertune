#pragma once

const int VIEW_AREA_WIDTH  = 16;
const int VIEW_AREA_HEIGHT = 9;

struct Entity;
struct Hero;
struct Tilemap;
struct Camera;
struct Enemy;
struct Projectile;

struct Entities_By_Type {
    Hero *_Hero = NULL;
    Array <Enemy *> _Enemy;
    Array <Projectile *> _Projectile;
};

struct World {
    Entities_By_Type by_type;
    Hash_Table <u64, Entity *> entity_lookup;
    Array <Entity *> all_entities;

    Array <Entity *> entities_to_be_destroyed;
    
    Tilemap *tilemap;
    Camera *camera;
    
    Vector2i size;
};

void init_world(World *world, Vector2i size);
void update_world(World *world, float dt);
void draw_world(World *world);

Vector2 world_space_to_screen_space(World *world, Vector2 v);
Vector2 screen_space_to_world_space(World *world, Vector2 v);

Entity *get_entity_by_id(World *world, u64 id);
void schedule_for_destruction(Entity *entity);

Hero *make_hero(World *world);
Enemy *make_enemy(World *world);
Projectile *make_projectile(World *world);
