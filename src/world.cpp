#include "main.h"
#include "world.h"
#include "entity.h"
#include "rendering.h"
#include "tilemap.h"
#include "camera.h"
#include "font.h"

#include "mt19937-64.h"

#include <stdio.h>

void init_world(World *world, Vector2i size) {
    unsigned long long init[] = {(u64)size.x, (u64)size.y};
    init_by_array64(init, ArrayCount(init));

    world->tilemap = NULL;
    world->size    = size;
}

void update_world(World *world, float dt) {
    for (Enemy *enemy : world->by_type._Enemy) {
        if (enemy->scheduled_for_destruction) continue;

        update_single_enemy(enemy, dt);
    }

    for (Projectile *projectile : world->by_type._Projectile) {
        if (projectile->scheduled_for_destruction) continue;

        update_single_projectile(projectile, dt);
    }
    
    if (world->by_type._Hero) {
        if (!world->by_type._Hero->scheduled_for_destruction) {
            update_single_hero(world->by_type._Hero, dt);
        }
    }

    update_camera(world->camera, world, dt);

    // Is it safe to do this here???
    for (Entity *e : world->entities_to_be_destroyed) {
        int index = world->all_entities.find(e);
        if (index != -1) {
            world->all_entities.ordered_remove_by_index(index);
        }

        for (int i = 0; i < world->entity_lookup.allocated; i++) {
            auto bucket = &world->entity_lookup.buckets[i];
            if (bucket->key == e->id && bucket->value == e) {
                world->entity_lookup.occupancy_mask[i] = false;
                break;
            }
        }
        world->entity_lookup.count--;

        switch (e->type) {
            case ENTITY_TYPE_HERO: {
                world->by_type._Hero = NULL;
            } break;

            case ENTITY_TYPE_ENEMY: {
                world->by_type._Enemy.ordered_remove_by_value((Enemy *)e);
            } break;

            case ENTITY_TYPE_PROJECTILE: {
                world->by_type._Projectile.ordered_remove_by_value((Projectile *)e);
            } break;
        }

        delete e;
    }
    world->entities_to_be_destroyed.count = 0;
}

void draw_world(World *world) {
    set_shader(NULL);
    
    set_viewport(0, 0, globals.render_width, globals.render_height);
    clear_framebuffer(0.2f, 0.5f, 0.8f, 1.0f);

    set_shader(globals.shader_color);
    rendering_2d(globals.render_width, globals.render_height, get_world_to_view_matrix(world->camera, world));

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    immediate_begin();

    assert(world->tilemap);
    draw_tilemap(world->tilemap, world);

    for (Enemy *enemy : world->by_type._Enemy) {
        if (enemy->scheduled_for_destruction) continue;
        
        draw_single_enemy(enemy);
    }

    for (Projectile *projectile : world->by_type._Projectile) {
        if (projectile->scheduled_for_destruction) continue;

        draw_single_projectile(projectile);
    }
    
    Hero *hero = world->by_type._Hero;
    if (hero && !hero->scheduled_for_destruction) {
        draw_single_hero(hero);
    }

    immediate_flush();

    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);
    
    int font_size = (int)(0.03f * globals.render_height);
    Dynamic_Font *font = get_font_at_size("Inconsolata-Regular", font_size);
    char text[256];
    snprintf(text, sizeof(text), "Health: %.1lf", world->by_type._Hero ? world->by_type._Hero->health : 0.0);
    int x = 0;
    int y = globals.render_height - font->character_height;

    Vector4 color = v4(0, 1, 0, 1);
    if ((world->by_type._Hero && world->by_type._Hero->health <= 0.0) ||
        !world->by_type._Hero) {
        color = v4(1, 0, 0, 1);
    }
    draw_text(font, text, x, y, color);
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
    e->scheduled_for_destruction = false;

    world->entity_lookup.add(id, e);
    world->all_entities.add(e);
}

Hero *make_hero(World *world) {
    Hero *hero = new Hero();

    world->by_type._Hero = hero;
    register_entity(world, hero, ENTITY_TYPE_HERO);

    return hero;
}

Enemy *make_enemy(World *world) {
    Enemy *enemy = new Enemy();

    world->by_type._Enemy.add(enemy);
    register_entity(world, enemy, ENTITY_TYPE_ENEMY);
    
    return enemy;
}

Projectile *make_projectile(World *world) {
    Projectile *projectile = new Projectile();

    world->by_type._Projectile.add(projectile);
    register_entity(world, projectile, ENTITY_TYPE_PROJECTILE);

    return projectile;
}

void schedule_for_destruction(Entity *entity) {
    World *world = entity->world;
    assert(world);

    entity->scheduled_for_destruction = true;
    world->entities_to_be_destroyed.add(entity);
}
