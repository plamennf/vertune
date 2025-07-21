#include "main.h"
#include "world.h"
#include "entity.h"
#include "rendering.h"

#include "mt19937-64.h"

void init_world(World *world, Vector2i size) {
    unsigned long long init[] = {(u64)size.x, (u64)size.y};
    init_by_array64(init, ArrayCount(init));
    
    world->size = size;
}

void update_world(World *world, float dt) {
    
}

void draw_world(World *world) {            
    set_shader(NULL);
    
    set_viewport(0, 0, globals.window_width, globals.window_height);
    clear_framebuffer(0.2f, 0.5f, 0.8f, 1.0f);

    set_shader(globals.shader_color);
    rendering_2d(globals.window_width, globals.window_height);

    assert(world->by_type._Hero);
    Hero *hero = world->by_type._Hero;

    Vector2 position = world_space_to_screen_space(world, hero->position);
    Vector2 size     = world_space_to_screen_space(world, hero->size);
    
    immediate_begin();
    immediate_quad(position.x, position.y, size.x, size.y, hero->color);
    immediate_flush();
}

Vector2 world_space_to_screen_space(World *world, Vector2 v) {
    assert(world->size.x > 0);
    assert(world->size.y > 0);
    
    Vector2 result = v;

    result.x /= (float)VIEW_AREA_WIDTH;
    result.y /= (float)VIEW_AREA_HEIGHT;

    result.x *= (float)globals.window_width;
    result.y *= (float)globals.window_height;

    return result;
}

Vector2 screen_space_to_world_space(World *world, Vector2 v) {
    assert(globals.window_width  > 0);
    assert(globals.window_height > 0);

    Vector2 result = v;

    result.x /= (float)globals.window_width;
    result.y /= (float)globals.window_height;

    result.x *= (float)VIEW_AREA_WIDTH;
    result.y *= (float)VIEW_AREA_HEIGHT;

    return result;
}

Entity *get_entity_by_id(World *world, u64 id) {
    Entity **_e = world->entity_lookup.find(id);
    if (!_e) return NULL;
    return *_e;
}

static u64 generate_id(World *world) {
    while (1) {
        u64 id = genrand64_int64();
        Entity **_e = world->entity_lookup.find(id);
        if (!_e) return id;
    }

    return 0;
}

static void register_entity(World *world, Entity *e, Entity_Type type) {
    u64 id = generate_id(world);

    e->id    = id;
    e->world = world;
    e->type  = type;

    world->entity_lookup.add(id, e);
    world->all_entities.add(e);
}

Hero *make_hero(World *world) {
    Hero *hero = new Hero();

    world->by_type._Hero = hero;
    
    register_entity(world, hero, ENTITY_TYPE_HERO);
    return hero;
}
