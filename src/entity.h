#pragma once

struct World;

const float GRAVITY = -30.0f;
const float MOVE_SPEED = 5.0f;
const float JUMP_FORCE = 15.0f;
const float MAX_FALL_SPEED = -25.0f;
const float FAST_FALL_MULTIPLIER = 1.5f;

const double PROJECTILE_DAMAGE = 1.0;
const double ENEMY_DAMAGE = 0.5;

enum Entity_Type {
    ENTITY_TYPE_UNKNOWN,
    ENTITY_TYPE_HERO,
    ENTITY_TYPE_ENEMY,
    ENTITY_TYPE_PROJECTILE,
    ENTITY_TYPE_PICKUP,
    ENTITY_TYPE_DOOR,
};

struct Entity {
    Entity_Type type;
    u64 id;
    World *world;
    bool scheduled_for_destruction;

    Vector2 position;
    Vector2 size;
    Vector4 color;
};

enum Hero_State {
    HERO_STATE_IDLE,
    HERO_STATE_JUMPING,
    HERO_STATE_FALLING,
    HERO_STATE_MOVING,
};

struct Hero : public Entity {
    Hero_State state = HERO_STATE_IDLE;
    Vector2 velocity = v2(0, 0);
    bool is_facing_right = true;
    bool is_on_ground = true;
    double health = 3.0;
    int num_pickups = 0;
};

void update_single_hero(Hero *hero, float dt);
void draw_single_hero(Hero *hero);

void damage_hero(Hero *hero, double damage_amount);

struct Enemy : public Entity {    
    float speed = 2.0f;
    bool is_facing_right = true;
    float radius = 0.5f;

    float time_since_last_projectile = 0.0f;
    float time_between_projectiles = 3.0f;
};

void update_single_enemy(Enemy *enemy, float dt);
void draw_single_enemy(Enemy *enemy);

struct Projectile : public Entity {
    float speed = 5.0f;
    bool is_facing_right = true;
    float radius = 0.5f;
};

void update_single_projectile(Projectile *projectile, float dt);
void draw_single_projectile(Projectile *projectile);

struct Pickup : public Entity {
    float radius = 0.5f;
};

void draw_single_pickup(Pickup *pickup);

struct Door : public Entity {
    bool locked = true;
};

void draw_single_door(Door *door);
