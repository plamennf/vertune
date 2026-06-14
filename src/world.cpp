#include "main.h"
#include "world.h"
#include "entity.h"
#include "rendering.h"
#include "tilemap.h"
#include "camera.h"
#include "font.h"
#include "text_file_handler.h"
#include "particles.h"
#include "audio.h"
#include "resource_manager.h"

#include "mt19937-64.h"

#include <stdio.h>
#include <stdlib.h>

const float TUTORIAL_START_X = 0.0f;
const float TUTORIAL_END_X = 5.0f;

#define WORLD_FILE_VERSION 1

static void register_entity(World *world, Entity *e, Entity_Type type, u64 id = 0);

void init_world(World *world, Vector2i size) {
    unsigned long long init[] = {(u64)size.x, (u64)size.y};
    init_by_array64(init, ArrayCount(init));

    world->tilemap = NULL;
    world->size    = size;

    world->particle_system = new Particle_System();
    world->particle_system->particles.reserve(1024);
}

void update_world(World *world, float dt) {
    MyZoneScoped;
    
    if (!is_intro(world) && !is_outro(world)) {
        for (Enemy *enemy : world->by_type._Enemy) {
            if (enemy->scheduled_for_destruction) continue;
            enemy->num_nanoseconds_since_collision_with_the_hero += (s64)(dt * NS_PER_SECOND);
            
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
                            unlock_door(world->by_type._Door);
                        }
                    }
                }
            }
        }
    }
    
    if (world->level_fade.active) {
        world->level_fade.timer += dt;
        if (world->level_fade.timer > world->level_fade.duration) {
            world->level_fade.active = false;
        }
    }

    update_camera(world->camera, world, dt);
    if (is_level_intro(world)) {
        Vector2 delta = world->camera->target - world->camera->position;
        if (length(delta) < 0.1f) {
            world->level_intro = false;
        }
    }

    if (is_outro(world)) {
        update_level_outro(world, world->by_type._Door, dt);
    }
    
    if (!is_intro(world)) {
        update_particles(world->particle_system, dt);
    }
}

static void draw_health(Vector2 position, Vector2 size, int pad) {
    if (!globals.current_world) return;
    if (!globals.current_world->by_type._Hero) return;
    
    double health = globals.current_world->by_type._Hero->health;
    
    int max_hearts = 3;
    int full_hearts = (int)health;
    bool half_heart = (health - (double)full_hearts) >= 0.5;

    set_shader(globals.shader_texture);
    for (int i = 0; i < max_hearts; i++) {
        Vector2 pos;
        if (i == 0) {
            pos = position;
        } else {
            pos = v2(position.x + pad * 0.5f + i * (size.x + pad), position.y);
        }
        
        Texture *texture = NULL;
        if (i < full_hearts) texture = globals.full_heart;
        else if (i == full_hearts && half_heart) texture = globals.half_heart;
        else texture = globals.empty_heart;

        set_texture(0, texture);
        
        immediate_begin();

        if (globals.draw_outlines) {
            Vector2 outline_size     = size * 1.15f;
            Vector2 outline_position = pos - ((outline_size - size) * 0.5f);
            
            immediate_quad(outline_position, outline_size, v4(0, 0, 0, 1));
        }
        
        immediate_quad(pos, size, v4(1, 1, 1, 1));
        immediate_flush();
    }
}

static void draw_pickups(Vector2 position, Vector2 size, int pad) {
    World *world = globals.current_world;
    if (!world) return;
    
    set_shader(globals.shader_color);
    
    immediate_begin();
    if (globals.draw_outlines) {
        Vector2 outline_size     = size * 1.15f;
        Vector2 outline_position = position - ((outline_size - size) * 0.5f);
        immediate_circle(outline_position + outline_size * 0.5f, outline_size.y * 0.5f, v4(0, 0, 0, 1));
    }
    immediate_circle(position + size * 0.5f, size.y * 0.5f, v4(1, 1, 0, 1));
    immediate_flush();

    set_shader(globals.shader_text);
    //int font_size = (int)(0.05f * globals.render_height);
    int font_size = (int)size.y;
    Dynamic_Font *font = get_font_at_size("Inconsolata-Regular", font_size);
    char text[256];
    snprintf(text, sizeof(text), "%d/%d", world->by_type._Hero ? world->by_type._Hero->num_pickups : 0, world->num_pickups_needed_to_unlock_door);
    int x = (int)(position.x + size.x) + pad;
    int y = (int)(position.y) + font->character_height / 4;
    draw_text(font, text, x, y, v4(1, 1, 0, 1));
}

static void draw_restarts(Vector2 position, Vector2 size) {
    if (!globals.current_world) return;
    
    set_shader(globals.shader_texture);
    for (int i = 0; i < MAX_RESTARTS; i++) {
        Vector2 pos = v2(position.x + i * (size.x + 6), position.y);
        Texture *texture = globals.restart_available;
        if (i < globals.num_restarts_for_current_world) texture = globals.restart_taken;
        
        set_texture(0, texture);
        
        immediate_begin();
        immediate_quad(pos, size, v4(1, 1, 1, 1));
        immediate_flush();
    }
}

static int light_compare_func(const void *_a, const void *_b) {
    Light *a = *(Light **)_a;
    Light *b = *(Light **)_b;

    if (a->distance_to_player < b->distance_to_player) return -1;
    if (a->distance_to_player > b->distance_to_player) return +1;
    return 0;
}

static void find_closest_lights(World *world, Vector2 position, Light lights[MAX_LIGHTS], int num_lights) {
    auto const &all_lights = world->by_type._Light;

    int num_lights_to_sort = 0;
    for (Light *light : all_lights) {
        if (light->scheduled_for_destruction) continue;

        num_lights_to_sort++;
    }

    Light **sorted_lights = globals.frame_memory.allocate_array<Light *>(num_lights_to_sort);
    //defer { delete [] sorted_lights; };
    num_lights_to_sort = 0;
    for (Light *light : all_lights) {
        if (light->scheduled_for_destruction) continue;

        sorted_lights[num_lights_to_sort++] = light;
    }
    
    for (int i = 0; i < num_lights_to_sort; i++) {
        Light *light = sorted_lights[i];
        light->distance_to_player = length(position - light->position);
    }
    
    qsort(sorted_lights, num_lights_to_sort, sizeof(Light *), light_compare_func);

    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i < num_lights) {
            lights[i] = *sorted_lights[i];
            lights[i].id       = 0;
            lights[i].position = world_space_to_screen_space(world, lights[i].position);
            lights[i].radius   = world_space_to_screen_space(world, v2(0, lights[i].radius)).y;
        } else {
            lights[i] = *sorted_lights[0];
            lights[i].id        = 0;
            lights[i].position  = v2(0, 0);
            lights[i].color     = v4(0, 0, 0, 1);
            lights[i].radius    = 0.0f;
            lights[i].intensity = 0.0f;
        }
    }
}

static void get_merged_occluders(std::vector <Vector4> &merged, World *world, Vector2 hero_pos, float search_radius) {
    Tilemap *tm = world->tilemap;
    
    int x_min = Max(0, (int)(hero_pos.x - search_radius));
    int x_max = Min(tm->width - 1, (int)(hero_pos.x + search_radius));
    int y_min = Max(0, (int)(hero_pos.y - search_radius));
    int y_max = Min(tm->height - 1, (int)(hero_pos.y + search_radius));

    bool *visited = globals.frame_memory.allocate_array<bool>(((x_max - x_min + 1) * (y_max - y_min + 1) * sizeof(bool)));
    memset(visited, 0, (x_max - x_min + 1) * (y_max - y_min + 1) * sizeof(bool));

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int local_idx = (y - y_min) * (x_max - x_min + 1) + (x - x_min);
            
            if (!visited[local_idx] && is_tile_id_collidable(tm, get_tile_id_at(tm, v2((float)x, (float)y)))) {
                int start_x = x;
                int width = 0;

                while (x <= x_max && is_tile_id_collidable(tm, get_tile_id_at(tm, v2((float)x, (float)y))) && !visited[(y - y_min) * (x_max - x_min + 1) + (x - x_min)]) {
                    visited[(y - y_min) * (x_max - x_min + 1) + (x - x_min)] = true;
                    width++;
                    x++;
                }

                Vector2 screen_space_position = world_space_to_screen_space(world, v2(floorf((float)start_x), floorf((float)y)));
                Vector2 screen_space_size = world_space_to_screen_space(world, v2((float)width, 1));
                
                Vector4 occluder = v4(screen_space_position.x, screen_space_position.y, screen_space_size.x, screen_space_size.y);
                
                merged.push_back(occluder);
                
                if (merged.size() >= 64) return;
            }
        }
    }
}

void draw_world(World *world, bool skip_hud) {
    MyZoneScoped;
    
    clear_framebuffer(0.2f, 0.5f, 0.8f, 1.0f);

    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);
    
    rendering_2d(globals.render_width, globals.render_height, get_world_to_view_matrix(world->camera, world));
    
    bool use_lighting = world->by_type._Light.size() > 0 && globals.enable_lighting;
    if (use_lighting) {
        set_shader(globals.shader_lighting);
    } else {
        set_shader(globals.shader_color);
    }
    
    immediate_begin();

    if (use_lighting) {
        Vector2 hero_position = world->by_type._Hero ? world->by_type._Hero->position : v2(0, 0);
    
        Light closest_lights[MAX_LIGHTS];

        int num_closest_lights = 0;
        for (Light *light : world->by_type._Light) {
            if (light->scheduled_for_destruction) continue;

            num_closest_lights++;
        }
        
        num_closest_lights = Min(MAX_LIGHTS, num_closest_lights);
        find_closest_lights(world, hero_position, closest_lights, num_closest_lights);

        std::vector <Vector4> active_occluders;
        get_merged_occluders(active_occluders, world, hero_position, 50.0f);
        
        refresh_lighting(closest_lights, num_closest_lights, active_occluders);
    }
    
    assert(world->tilemap);
    draw_tilemap(world->tilemap, world);

    if (!skip_hud) {
        for (Enemy *enemy : world->by_type._Enemy) {
            if (enemy->scheduled_for_destruction) continue;

    
            if (!is_intro(world)) {
                draw_single_enemy(enemy, false);
            } else {
                draw_single_enemy(enemy, true);
            }
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
    }
    
    Hero *hero = world->by_type._Hero;
    if (hero && !hero->scheduled_for_destruction) {
        draw_single_hero(hero);
    }

    draw_particles(world->particle_system, world);

    for (Light *light : world->by_type._Light) {
        if (!light->should_draw) continue;
        
        float world_ceiling_y = (float)world->size.y + 50.0f;

        float wire_half_width = 0.02f; 
        float bulb_half_width = 0.2f;
        float bulb_height     = 0.15f;

        Vector2 wire_position = v2(light->position.x - wire_half_width, light->position.y);
        Vector2 wire_size     = v2(wire_half_width * 2.0f, world_ceiling_y - light->position.y);

        Vector2 bulb_position = v2(light->position.x - bulb_half_width, light->position.y);
        Vector2 bulb_size     = v2(bulb_half_width * 2.0f, bulb_height);

        Vector2 screen_space_wire_position = world_space_to_screen_space(world, wire_position);
        Vector2 screen_space_wire_size     = world_space_to_screen_space(world, wire_size);
    
        Vector2 screen_space_bulb_position = world_space_to_screen_space(world, bulb_position);
        Vector2 screen_space_bulb_size     = world_space_to_screen_space(world, bulb_size);

        immediate_quad(screen_space_wire_position, screen_space_wire_size, v4(0.05f, 0.05f, 0.05f, 1.0f));

        if (globals.draw_outlines) {
            Vector2 outline_size     = v2(bulb_size.x * 1.1f, bulb_size.y * 1.2f);
            Vector2 outline_position = bulb_position - (outline_size - bulb_size) * 0.5f;

            Vector2 screen_space_position = world_space_to_screen_space(world, outline_position);
            Vector2 screen_space_size     = world_space_to_screen_space(world, outline_size);
            immediate_quad(screen_space_position, screen_space_size, v4(0, 0, 0, 1));
        }

        immediate_quad(screen_space_bulb_position, screen_space_bulb_size, light->color);
    }
    
    immediate_flush();
    
    set_shader(globals.shader_text);
    rendering_2d(globals.render_width, globals.render_height);

    set_blend_mode(BLEND_MODE_ALPHA);
    set_cull_mode(CULL_MODE_OFF);
    set_depth_test_mode(DEPTH_TEST_OFF);

    if (!skip_hud) {
        int pad = (int)(0.002f * globals.render_width);
        if (globals.draw_outlines) {
            pad = pad * 2;
        }
        
        Vector2 health_size = v2(0.05f * globals.render_height, 0.05f * globals.render_height);
        Vector2 screen_space_health_position = v2(pad * 0.5f, (float)globals.render_height - health_size.y);
        Vector2 screen_space_health_size = health_size;
        draw_health(screen_space_health_position, screen_space_health_size, pad);

        int stride = (int)screen_space_health_size.y;
        if (globals.draw_outlines) {
            stride = (int)(stride * 1.15f);
        }
        
        screen_space_health_position.y -= stride;
        
        draw_pickups(screen_space_health_position, screen_space_health_size, pad);

        screen_space_health_position.y -= stride;
        draw_restarts(screen_space_health_position, screen_space_health_size);

        set_shader(globals.shader_text);

        if (!world->level_fade.active) {
            int font_size = (int)(0.04f * globals.render_height);
            Dynamic_Font *font = get_font_at_size("OpenSans-Regular", font_size);
            char text[256];
            snprintf(text, sizeof(text), "Level %d", globals.current_world_index);
            int x = globals.render_width - font->get_string_width_in_pixels(text);
            int y = globals.render_height - font->character_height;

            if (globals.draw_outlines) {
                int offset = font->character_height / 20;
                draw_text_outlined(font, text, x, y, v4(1, 1, 1, 1), offset);
            } else {
                draw_text(font, text, x, y, v4(1, 1, 1, 1));
            }
        }
            
        if (world->level_fade.active) {
            int font_size = (int)(0.0075f * globals.render_height);
            Dynamic_Font *font = get_font_at_size("Inconsolata-Regular", font_size);
            float alpha = 1.0f;
            if (world->level_fade.timer > 1.0f) {
                alpha = 1.0f - (world->level_fade.timer / (world->level_fade.duration + 0.0f));
            }
            char text[256];
            snprintf(text, sizeof(text), "Level %d", world->level_fade.level_number);
            int x = (globals.render_width - font->get_string_width_in_pixels(text)) / 2;
            int y = globals.render_height - font->character_height;
            Vector4 color = v4(1, 1, 1, alpha);
            draw_text(font, text, x, y, color);
        }

        if (globals.num_worlds_completed == 0 && !is_intro(world)) {
            if (world->by_type._Hero) {
                Hero *hero = world->by_type._Hero;
                if (hero->position.x >= TUTORIAL_START_X &&
                    hero->position.x <= TUTORIAL_END_X) {
                    int font_size = (int)(0.02f * globals.render_height);
                    Dynamic_Font *font = get_font_at_size("OpenSans-Regular", font_size);
                    char *text = "Jumping on enemies will give you a jump boost";

                    Vector2 text_position;
                    text_position.x = world_space_to_screen_space(world, v2((hero->position.x + hero->size.x * 0.5f), 0)).x - font->get_string_width_in_pixels(text) * 0.5f;
                    text_position.x = Max(0.0058616647127784f * globals.render_width, text_position.x);
                    text_position.y = world_space_to_screen_space(world, v2(0, hero->position.y + hero->size.y * 1.1f)).y;

                    int x = (int)text_position.x;
                    int y = (int)text_position.y;

                    set_shader(globals.shader_text);
                    Vector4 color = v4(1, 1, 1, 1);
                    int offset = font_size / 10;
                    draw_text_outlined(font, text, x, y, color, offset);
                }
            }
        }
    }
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

    world->entities_to_be_destroyed.clear();

    for (u32 i = 0; i < world->all_entities.size(); i++) {
        delete world->all_entities[i];
        world->all_entities[i] = NULL;
    }
    world->all_entities.clear();

    world->entity_lookup.clear();

    world->by_type._Hero = NULL;
    world->by_type._Door = NULL;
    world->by_type._Enemy.clear();
    world->by_type._Projectile.clear();
    world->by_type._Pickup.clear();
}

static Tilemap *copy_tilemap(Tilemap *tilemap) {
    MyZoneScoped;
    
    if (!tilemap) return nullptr;

    Tilemap *result = new Tilemap();
    result->width  = tilemap->width;
    result->height = tilemap->height;
    result->num_colors = tilemap->num_colors;
    result->num_collidable_ids = tilemap->num_collidable_ids;

    int total_tiles = tilemap->width * tilemap->height;
    if (tilemap->tiles && total_tiles > 0) {
        result->tiles = (u8 *)malloc(total_tiles * sizeof(u8));
        memcpy(result->tiles, tilemap->tiles, total_tiles * sizeof(u8));
    }

    if (tilemap->colors && tilemap->num_colors > 0) {
        result->colors = (Vector4 *)malloc(tilemap->num_colors * sizeof(Vector4));
        memcpy(result->colors, tilemap->colors, tilemap->num_colors * sizeof(Vector4));
    }

    if (tilemap->collidable_ids && tilemap->num_collidable_ids > 0) {
        result->collidable_ids = (u8 *)malloc(tilemap->num_collidable_ids * sizeof(u8));
        memcpy(result->collidable_ids, tilemap->collidable_ids, tilemap->num_collidable_ids * sizeof(u8));
    }

    return result;
}

World *copy_world(World *world) {
    MyZoneScoped;
    
    if (!world) return NULL;
    
    World *result = new World();
    
    result->size = world->size;
    result->num_pickups_needed_to_unlock_door = world->num_pickups_needed_to_unlock_door;
    result->level_fade = world->level_fade;
    result->level_intro = world->level_intro;

    if (world->tilemap) {
        result->tilemap = copy_tilemap(world->tilemap);
    }

    if (world->camera) {
        result->camera = new Camera();
        *result->camera = *world->camera;
    }

    result->particle_system = new Particle_System();
    result->particle_system->particles.reserve(128);

    result->all_entities.reserve(world->all_entities.size());

    auto clone_entity = [&](Entity *e) -> Entity * {
        if (!e) return nullptr;
        
        Entity *copy = nullptr;
        switch (e->type) {
            case ENTITY_TYPE_HERO: {
                Hero *h = new Hero(*((Hero *)e));
                copy = h;
                result->by_type._Hero = h;
                register_entity(result, h, ENTITY_TYPE_HERO, h->id);
            } break;
                
            case ENTITY_TYPE_DOOR: {
                Door *d = new Door(*((Door *)e));
                copy = d;
                result->by_type._Door = d;
                register_entity(result, d, ENTITY_TYPE_DOOR, d->id);
            } break;
                
            case ENTITY_TYPE_ENEMY: {
                Enemy *en = new Enemy(*((Enemy *)e));
                copy = en;
                result->by_type._Enemy.push_back(en);
                register_entity(result, en, ENTITY_TYPE_ENEMY, e->id);
            } break;
                
            case ENTITY_TYPE_PROJECTILE: {
                Projectile *p = new Projectile(*((Projectile *)e));
                copy = p;
                result->by_type._Projectile.push_back(p);
                register_entity(result, p, ENTITY_TYPE_PROJECTILE, p->id);
            } break;
                
            case ENTITY_TYPE_PICKUP: {
                Pickup *p = new Pickup(*((Pickup *)e));
                copy = p;
                result->by_type._Pickup.push_back(p);
                register_entity(result, p, ENTITY_TYPE_PICKUP, p->id);
            } break;

            case ENTITY_TYPE_LIGHT: {
                Light *l = new Light(*((Light *)e));
                copy = l;
                result->by_type._Light.push_back(l);
                register_entity(result, l, ENTITY_TYPE_LIGHT, l->id);
            } break;
                
            default: {
                copy = new Entity(*e);
            } break;
        }

        return copy;
    };

    for (Entity *e : world->all_entities) {
        clone_entity(e);
    }

    if (result->camera && result->by_type._Hero) {
        result->camera->following_id = result->by_type._Hero->id;
    }

    return result;
}

void do_entity_destruction(World *world) {
    if (!is_intro(world)) {
        for (Entity *e : world->entities_to_be_destroyed) {
            if (e == NULL) continue;

            auto it = std::find(world->all_entities.begin(), world->all_entities.end(), e);
            if (it != world->all_entities.end()) {
                world->all_entities.erase(it);
            }

            world->entity_lookup.erase(e->id);

            switch (e->type) {
                case ENTITY_TYPE_HERO: {
                    world->by_type._Hero = NULL;
                } break;

                case ENTITY_TYPE_ENEMY: {
                    auto &enemies = world->by_type._Enemy;
                    auto it = std::find(enemies.begin(), enemies.end(), (Enemy *)e);
                    if (it != enemies.end()) enemies.erase(it);
                } break;

                case ENTITY_TYPE_PROJECTILE: {
                    auto &projectiles = world->by_type._Projectile;
                    auto it = std::find(projectiles.begin(), projectiles.end(), (Projectile *)e);
                    if (it != projectiles.end()) projectiles.erase(it);
                } break;

                case ENTITY_TYPE_PICKUP: {
                    auto &pickups = world->by_type._Pickup;
                    auto it = std::find(pickups.begin(), pickups.end(), (Pickup *)e);
                    if (it != pickups.end()) pickups.erase(it);
                } break;

                case ENTITY_TYPE_LIGHT: {
                    auto &lights = world->by_type._Light;
                    auto it = std::find(lights.begin(), lights.end(), (Light *)e);
                    if (it != lights.end()) lights.erase(it);
                } break;
                    
                case ENTITY_TYPE_DOOR: {
                    world->by_type._Door = NULL;
                } break;
            }

            if (e) {
                delete e;
                e = NULL;
            }
        }
        world->entities_to_be_destroyed.clear();
    }
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
    auto it = world->entity_lookup.find(id);
    if (it != world->entity_lookup.end()) {
        return it->second;
    }
    return NULL;
}

static u64 generate_id(World *world) {
    while (1) {
        u64 id = genrand64_int64();

        bool found = false;
        for (Entity *e : world->all_entities) {
            if (e->id == id) {
                found = true;
                break;
            }
        }

        if (!found && id != 0) return id;
    }

    return 0;
}

static void register_entity(World *world, Entity *e, Entity_Type type, u64 id) {
    if (id == 0) {
        id = generate_id(world);
    }
    
    e->id    = id;
    e->world = world;
    e->type  = type;
    e->scheduled_for_destruction = false;

    world->entity_lookup.insert({id, e});
    world->all_entities.push_back(e);
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

    world->by_type._Enemy.push_back(enemy);
    register_entity(world, enemy, ENTITY_TYPE_ENEMY);
    
    return enemy;
}

Projectile *make_projectile(World *world) {
    Projectile *projectile = new Projectile();

    world->by_type._Projectile.push_back(projectile);
    register_entity(world, projectile, ENTITY_TYPE_PROJECTILE);

    return projectile;
}

Pickup *make_pickup(World *world) {
    Pickup *pickup = new Pickup();

    world->by_type._Pickup.push_back(pickup);
    register_entity(world, pickup, ENTITY_TYPE_PICKUP);

    return pickup;
}

Light *make_light(World *world) {
    Light *light = new Light();

    world->by_type._Light.push_back(light);
    register_entity(world, light, ENTITY_TYPE_LIGHT);

    return light;
}

void schedule_for_destruction(Entity *entity) {
    World *world = entity->world;
    assert(world);

    entity->scheduled_for_destruction = true;
    world->entities_to_be_destroyed.push_back(entity);
}

bool is_level_intro(World *world) {
    return world->level_intro;
}

bool is_camera_intro(World *world) {
    bool camera_intro = false;
    if (world && world->camera && world->camera->intro_active) camera_intro = true;
    return camera_intro;
}

bool is_intro(World *world) {
    return is_level_intro(world) || is_camera_intro(world);
}

bool is_outro(World *world) {
    return world->level_outro;
}

void start_level_outro(World *world, Door *door) {
    world->level_outro       = true;
    door->is_opening         = true;
    
    Hero *hero = world->by_type._Hero;
    if (!hero) return;

    hero->position.x = door->position.x - hero->size.x;
    hero->position.y = door->position.y;
}

void update_level_outro(World *world, Door *door, float dt) {
    if (door->open_t < 1.0f) {
        door->open_t += dt * NUM_SECONDS_NEEDED_TO_OPEN_DOOR;
    }
    bool should_update_hero = door->open_t >= 0.3f;

    if (should_update_hero) {
        Hero *hero = world->by_type._Hero;
        if (!hero) {
            globals.should_switch_worlds = true;
            play_sound(globals.level_complete_sfx);
            world->level_outro = false;
            return;
        }

        float target = door->position.x + door->size.x;
            
        float speed = 2.0f * hero->size.x * NUM_SECONDS_NEEDED_TO_OPEN_DOOR;
        hero->position.x = move_toward(hero->position.x, target, speed * dt);

        if (hero->position.x >= target) {
            globals.should_switch_worlds = true;
            play_sound(globals.level_complete_sfx);
            world->level_outro = false;
            return;
        }
    }
    
    door->visual_width = door->size.x * (1.0f - door->open_t);
    door->visual_width = Max(door->visual_width, door->size.x * 0.1f);
}
