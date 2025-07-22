#include "main.h"
#include "world.h"
#include "entity.h"
#include "rendering.h"
#include "tilemap.h"
#include "camera.h"

#include "mt19937-64.h"

void init_world(World *world, Vector2i size) {
    unsigned long long init[] = {(u64)size.x, (u64)size.y};
    init_by_array64(init, ArrayCount(init));

    world->tilemap = NULL;
    world->size    = size;
}

void update_world(World *world, float dt) {
    if (world->by_type._Hero) {
        update_single_hero(world->by_type._Hero, dt);
    }

    update_camera(world->camera, world, dt);
}

void draw_world(World *world) {
    set_shader(NULL);
    
    set_viewport(0, 0, globals.render_width, globals.render_height);
    clear_framebuffer(0.2f, 0.5f, 0.8f, 1.0f);

    set_shader(globals.shader_color);
    rendering_2d(globals.render_width, globals.render_height, get_world_to_view_matrix(world->camera, world));

    immediate_begin();

    assert(world->tilemap);
    draw_tilemap(world->tilemap, world);
    
    assert(world->by_type._Hero);
    Hero *hero = world->by_type._Hero;
    draw_single_hero(hero);

    immediate_flush();
}

Vector2 world_space_to_screen_space(World *world, Vector2 v) {
    assert(world->size.x > 0);
    assert(world->size.y > 0);
    
    Vector2 result = v;

    result.x /= (float)VIEW_AREA_WIDTH;
    result.y /= (float)VIEW_AREA_HEIGHT;

    result.x *= (float)globals.render_width;
    result.y *= (float)globals.render_height;

    return result;
}

Vector2 screen_space_to_world_space(World *world, Vector2 v) {
    assert(globals.render_width  > 0);
    assert(globals.render_height > 0);

    Vector2 result = v;

    result.x /= (float)globals.render_width;
    result.y /= (float)globals.render_height;

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
