#include "main.h"
#include "world.h"
#include "entity.h"
#include "rendering.h"
#include "tilemap.h"
#include "camera.h"
#include "font.h"
#include "text_file_handler.h"

#include "mt19937-64.h"

#include <stdio.h>

#define WORLD_FILE_VERSION 1

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

            if (world->by_type._Door) {
                if (!world->by_type._Door->scheduled_for_destruction) {
                    if (world->by_type._Hero->num_pickups >= world->num_pickups_needed_to_unlock_door) {
                        world->by_type._Door->locked = false;
                    }
                }
            }
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

            case ENTITY_TYPE_PICKUP: {
                world->by_type._Pickup.ordered_remove_by_value((Pickup *)e);
            } break;

            case ENTITY_TYPE_DOOR: {
                world->by_type._Door = NULL;
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

    for (Pickup *pickup : world->by_type._Pickup) {
        if (pickup->scheduled_for_destruction) continue;

        draw_single_pickup(pickup);
    }

    Door *door = world->by_type._Door;
    if (door && !door->scheduled_for_destruction) {
        draw_single_door(door);
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

    y -= font->character_height;
    snprintf(text, sizeof(text), "Pickups: %d", world->by_type._Hero ? world->by_type._Hero->num_pickups : 0);
    color = v4(1, 1, 0, 1);
    draw_text(font, text, x, y, color);
}

void destroy_world(World *world) {
    if (world->camera) {
        delete world->camera;
        world->camera = NULL;
    }

    if (world->tilemap) {
        delete world->tilemap;
        world->tilemap = NULL;
    }

    world->num_pickups_needed_to_unlock_door = 0;

    world->entities_to_be_destroyed.deallocate();

    for (int i = 0; i < world->all_entities.count; i++) {
        delete world->all_entities[i];
        world->all_entities[i] = NULL;
    }
    world->all_entities.deallocate();

    world->entity_lookup.deallocate();

    world->by_type._Hero = NULL;
    world->by_type._Door = NULL;
    world->by_type._Enemy.deallocate();
    world->by_type._Projectile.deallocate();
    world->by_type._Pickup.deallocate();
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

Door *make_door(World *world) {
    Door *door = new Door();

    world->by_type._Door = door;
    register_entity(world, door, ENTITY_TYPE_DOOR);

    return door;
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

Pickup *make_pickup(World *world) {
    Pickup *pickup = new Pickup();

    world->by_type._Pickup.add(pickup);
    register_entity(world, pickup, ENTITY_TYPE_PICKUP);

    return pickup;
}

void schedule_for_destruction(Entity *entity) {
    World *world = entity->world;
    assert(world);

    entity->scheduled_for_destruction = true;
    world->entities_to_be_destroyed.add(entity);
}

static Vector2 parse_vector2(char *s) {
    Vector2 result = v2(0, 0);
    if (!s) return result;

    sscanf(s, "%f %f", &result.x, &result.y);
    
    return result;
}

static Vector4 parse_vector4(char *s) {
    Vector4 result = v4(0, 0, 0, 0);
    if (!s) return result;

    sscanf(s, "%f %f %f %f", &result.x, &result.y, &result.z, &result.w);

    return result;
}

bool load_world_from_file(World *world, char *filepath) {
    Text_File_Handler handler;
    if (!start_file(&handler, filepath)) return false;
    defer { end_file(&handler); };
    
    if (handler.version < 1) {
        report_error(&handler, "Invalid version number for a tilemap file!");
        return false;
    }

    char *line = consume_next_line(&handler);
    if (!line) {
        report_error(&handler, "Expected width directive instead found end-of-file!");
        return false;
    }
    if (!starts_with(line, "width")) {
        report_error(&handler, "Expected width directive instead found '%s'!", line);
        return false;
    }
    line += string_length("width");
    line = eat_spaces(line);
    line = eat_trailing_spaces(line);

    int width = atoi(line);
    if (width <= 0) {
        report_error(&handler, "Width must be at least 1, but instead it is '%d'!", width);
        return false;
    }

    line = consume_next_line(&handler);
    if (!line) {
        report_error(&handler, "Expected height directive instead found end-of-file!");
        return false;
    }
    if (!starts_with(line, "height")) {
        report_error(&handler, "Expected height directive instead found '%s'!", line);
        return false;
    }
    line += string_length("height");
    line = eat_spaces(line);
    line = eat_trailing_spaces(line);

    int height = atoi(line);
    if (height <= 0) {
        report_error(&handler, "Height must be at least 1, but instead it is '%d'!", height);
        return false;
    }

    line = consume_next_line(&handler);
    if (!line) {
        report_error(&handler, "Expected tilemap directive instead found end-of-file!");
        return false;
    }
    if (!starts_with(line, "tilemap")) {
        report_error(&handler, "Expected tilemap directive instead found '%s'!", line);
        return false;
    }
    line += string_length("tilemap");
    line = eat_spaces(line);
    line = eat_trailing_spaces(line);

    world->size.x = width;
    world->size.y = height;
    
    unsigned long long init[] = {(u64)width, (u64)height};
    init_by_array64(init, ArrayCount(init));
    
    char *tilemap_filename = line;
    char tilemap_fullpath[256];
    snprintf(tilemap_fullpath, sizeof(tilemap_fullpath), "data/tilemaps/%s.tm", tilemap_filename);

    world->tilemap = new Tilemap();
    if (!load_tilemap(world->tilemap, tilemap_fullpath)) {
        delete world->tilemap;
        return false;
    }
    
    Entity *current_entity = NULL;
    Camera *current_camera = NULL;
    for (;;) {
        char *line = consume_next_line(&handler);
        if (!line) break;

        if (line[0] == '[') {
            line++;

            char *start = line;
            while (*line && *line != ']') {
                line++;
            }

            if (*line != ']') {
                report_error(&handler, "Expected ']' after entity type");
                return false;
            }

            start[line - start] = 0;

            start = eat_spaces(start);
            start = eat_trailing_spaces(start);

            current_entity = NULL;
            current_camera = NULL;
            if (strings_match(start, "Camera")) {
                current_camera = new Camera();
                current_camera->position       = v2(18, 9);
                current_camera->target         = v2(0, 0);
                current_camera->dead_zone_size = v2(VIEW_AREA_WIDTH, VIEW_AREA_HEIGHT) * 0.1f;
                current_camera->smooth_factor  = 0.95f;

                world->camera = current_camera;
            } else if (strings_match(start, "Hero")) {
                current_entity = make_hero(world);
            } else if (strings_match(start, "Door")) {
                current_entity = make_door(world);
            } else if (strings_match(start, "Enemy")) {
                current_entity = make_enemy(world);
            } else if (strings_match(start, "Pickup")) {
                current_entity = make_pickup(world);
            }
        } else if (starts_with(line, "position")) {
            line += string_length("position");
            line = eat_spaces(line);
            line = eat_trailing_spaces(line);

            Vector2 v = parse_vector2(line);

            if (current_camera) {
                assert(!current_entity);
                current_camera->position = v;
            } else if (current_entity) {
                assert(!current_camera);
                current_entity->position = v;
            }
        } else if (starts_with(line, "target")) {
            line += string_length("target");
            line = eat_spaces(line);
            line = eat_trailing_spaces(line);

            Vector2 v = parse_vector2(line);

            if (!current_camera) {
                report_error(&handler, "Field 'target' can only be set for Camera!");
                return false;
            }

            assert(!current_entity);
            current_camera->target = v;
        } else if (starts_with(line, "size")) {
            line += string_length("size");
            line = eat_spaces(line);
            line = eat_trailing_spaces(line);

            Vector2 v = parse_vector2(line);

            if (current_camera) {
                report_error(&handler, "Field 'size' can't be set for Camera");
                return false;
            }
            
            assert(current_entity);
            current_entity->size = v;
        } else if (starts_with(line, "color")) {
            line += string_length("color");
            line = eat_spaces(line);
            line = eat_trailing_spaces(line);

            Vector4 v = parse_vector4(line);

            if (current_camera) {
                report_error(&handler, "Field 'color' can't be set for Camera!");
                return false;
            }

            assert(current_entity);
            current_entity->color = v;
        } else if (starts_with(line, "radius")) {
            line += string_length("radius");
            line = eat_spaces(line);
            line = eat_trailing_spaces(line);

            float v = (float)atof(line);

            if (current_camera) {
                report_error(&handler, "Field 'radius' can't be set for Camera!");
                return false;
            }

            assert(current_entity);

            if (current_entity->type != ENTITY_TYPE_ENEMY && current_entity->type && ENTITY_TYPE_PROJECTILE && current_entity->type != ENTITY_TYPE_PICKUP) {
                report_error(&handler, "Field 'radius' can be set only for entities of type Enemy, Projectile or Pickup!");
                return false;
            }

            switch (current_entity->type) {
                case ENTITY_TYPE_ENEMY: {
                    Enemy *enemy = (Enemy *)current_entity;
                    enemy->radius = v;
                } break;

                case ENTITY_TYPE_PROJECTILE: {
                    Projectile *projectile = (Projectile *)current_entity;
                    projectile->radius = v;
                } break;

                case ENTITY_TYPE_PICKUP: {
                    Pickup *pickup = (Pickup *)current_entity;
                    pickup->radius = v;
                } break;
            }
        }
    }

    world->num_pickups_needed_to_unlock_door = world->by_type._Pickup.count;

    if (world->by_type._Hero) {
        world->camera->following_id = world->by_type._Hero->id;
    }
    
    return true;
}
