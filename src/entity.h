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

struct Hero : public Entity {
    bool is_facing_right = true;
};
