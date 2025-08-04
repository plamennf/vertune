#pragma once

const int VIEW_AREA_WIDTH  = 16;
const int VIEW_AREA_HEIGHT = 9;

struct Entity;
struct Hero;
struct Tilemap;
struct Camera;
struct Enemy;
struct Projectile;
struct Pickup;
struct Door;

struct Entities_By_Type {
    Hero *_Hero = NULL;
    Door *_Door = NULL;
    Array <Enemy *> _Enemy;
    Array <Projectile *> _Projectile;
    Array <Pickup *> _Pickup;
};

struct World {
    Entities_By_Type by_type;
    Hash_Table <u64, Entity *> entity_lookup;
    Array <Entity *> all_entities;

    Array <Entity *> entities_to_be_destroyed;

    int num_pickups_needed_to_unlock_door = 0;
    
    Tilemap *tilemap;
    Camera *camera;
    
    Vector2i size;
};

void init_world(World *world, Vector2i size);
void update_world(World *world, float dt);
void draw_world(World *world);
void destroy_world(World *world);

bool load_world_from_file(World *world, char *filepath);

Vector2 world_space_to_screen_space(World *world, Vector2 v);
Vector2 screen_space_to_world_space(World *world, Vector2 v);

Entity *get_entity_by_id(World *world, u64 id);
void schedule_for_destruction(Entity *entity);

Hero *make_hero(World *world);
Door *make_door(World *world);
Enemy *make_enemy(World *world);
Projectile *make_projectile(World *world);
Pickup *make_pickup(World *world);
