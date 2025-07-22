#pragma once

struct World;

enum Entity_Type {
    ENTITY_TYPE_UNKNOWN,
    ENTITY_TYPE_HERO,
};

struct Entity {
    Entity_Type type;
    u64 id;
    World *world;

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
};

void update_single_hero(Hero *hero, float dt);
void draw_single_hero(Hero *hero);
