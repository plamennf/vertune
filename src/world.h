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
struct Light;

struct Particle_System;

struct Entities_By_Type {
    Hero *_Hero = NULL;
    Door *_Door = NULL;
    eastl::vector <Enemy *> _Enemy;
    eastl::vector <Projectile *> _Projectile;
    eastl::vector <Pickup *> _Pickup;
    eastl::vector <Light *> _Light;
};

struct Level_Fade {
    bool active = false;
    float timer = 0.0f;
    float duration = 1.5f;
    int level_number = 0;
};

struct World {
    Entities_By_Type by_type;
    eastl::unordered_map <u64, Entity *> entity_lookup;
    eastl::vector <Entity *> all_entities;

    eastl::vector <Entity *> entities_to_be_destroyed;
    
    int num_pickups_needed_to_unlock_door = 0;
    Level_Fade level_fade;
    bool level_intro = true;
    
    Tilemap *tilemap;
    Camera *camera;
    Particle_System *particle_system;
    
    Vector2i size;
};

void init_world(World *world, Vector2i size);
void update_world(World *world, float dt);
void draw_world(World *world, bool skip_hud = false);
void destroy_world(World *world);
World *copy_world(World *world);
void do_entity_destruction(World *world);

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
Light *make_light(World *world);
