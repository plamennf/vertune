#include "main.h"
#include "entity.h"
#include "world.h"
#include "rendering.h"
#include "tilemap.h"
#include "particles.h"
#include "audio.h"

void update_single_hero(Hero *hero, float dt) {
    World *world = hero->world;
    assert(world);
    
    Tilemap *tilemap = world->tilemap;
    assert(tilemap);
    
    float input_x = 0.0f;
    if (is_key_down(SDL_SCANCODE_A) || is_key_down(SDL_SCANCODE_LEFT)) { input_x -= 1.0f; hero->is_facing_right = false; }
    if (is_key_down(SDL_SCANCODE_D) || is_key_down(SDL_SCANCODE_RIGHT)) { input_x += 1.0f; hero->is_facing_right = true; }

    hero->velocity.x = input_x * MOVE_SPEED;

    if ((is_key_down(SDL_SCANCODE_W) || is_key_down(SDL_SCANCODE_SPACE) || is_key_down(SDL_SCANCODE_UP)) && hero->is_on_ground) {
        hero->velocity.y   = JUMP_FORCE;
        hero->is_on_ground = false;
        emit_jump_particles(world->particle_system, hero->position);
        play_sound(globals.jump_sfx);
    }

    hero->velocity.y += GRAVITY * dt;

    if (!hero->is_on_ground && (is_key_down(SDL_SCANCODE_S) || is_key_down(SDL_SCANCODE_DOWN))) {
        hero->velocity.y += GRAVITY * 2.0f * dt; 
    }

    hero->velocity.y = Max(hero->velocity.y, MAX_FALL_SPEED);

    Vector2 new_position = hero->position + hero->velocity * dt;
    bool has_jumped_on_enemy = false;
    
    if (hero->velocity.x <= 0.0f) {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x, hero->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x, hero->position.y + hero->size.y * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            new_position.x = (int)new_position.x + 1.0f;
            hero->velocity.x = 0.0f;
        }
    } else {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x + hero->size.x, hero->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x + hero->size.x, hero->position.y + hero->size.y * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            new_position.x = static_cast <float>((int)new_position.x);
            hero->velocity.x = 0.0f;
        }
    }

    if (hero->velocity.y >= 0) {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x, new_position.y + hero->size.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x + 0.9f * hero->size.x, new_position.y + hero->size.y));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            new_position.y = static_cast <float>((int)new_position.y);
            hero->velocity.y = 0.0f;
        }
    } else {
        u8 tile1_id = get_tile_id_at(tilemap, new_position);
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x + 0.9f * hero->size.x, new_position.y));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            new_position.y = (int)new_position.y + 1.0f;
            hero->velocity.y = 0.0f;
            hero->is_on_ground = true;
        }

        for (Enemy *enemy : world->by_type._Enemy) {
            Rectangle2 hero_rect = { hero->position.x, hero->position.y, hero->size.x, hero->size.y };
            if (are_rect_and_circle_colliding(hero_rect, enemy->position, enemy->radius) && !hero->is_on_ground) {
                schedule_for_destruction(enemy);
                new_position.y = (int)new_position.y + 1.0f;
                hero->velocity.y = JUMP_FORCE * 1.5f;
                hero->is_on_ground = false;

                has_jumped_on_enemy = true;

                emit_stomp_particles(world->particle_system, enemy->position);
                play_sound(globals.enemy_kill_sfx);
                
                break;
            }
        }
    }

    if (!has_jumped_on_enemy) {
        for (Enemy *enemy : world->by_type._Enemy) {
            Rectangle2 hero_rect = { hero->position.x, hero->position.y, hero->size.x, hero->size.y };
            Rectangle2 enemy_rect = { enemy->position.x, enemy->position.y, enemy->size.x, enemy->size.y };
            if (are_rect_and_circle_colliding(hero_rect, enemy->position, enemy->radius)) {
                if (hero->velocity.x <= 0.0f) {
                    new_position.x = (int)new_position.x + enemy->radius * 3.0f;
                } else {
                    new_position.x = (int)new_position.x - enemy->radius;
                }
                hero->velocity.x = 0.0f;
                damage_hero(hero, ENEMY_DAMAGE);

                enemy->num_nanoseconds_since_collision_with_the_hero = 0;
            }
        }
    }
    
    hero->position = new_position;

    if (hero->position.y <= 0.0f) {
        hero->position.y   = 0.0f;
        hero->velocity.y   = 0.0f;
        hero->is_on_ground = true;
    }

    if (hero->position.x < 0.0f) {
        hero->position.x = 0.0f;
    }

    if (hero->position.x > world->size.x - hero->size.x) {
        hero->position.x = world->size.x - hero->size.x;
    }

    Rectangle2 hero_rect = { hero->position.x, hero->position.y, hero->size.x, hero->size.y };
    for (Pickup *pickup : world->by_type._Pickup) {
        if (pickup->scheduled_for_destruction) continue;

        if (are_rect_and_circle_colliding(hero_rect, pickup->position, pickup->radius)) {
            hero->num_pickups++;
            play_sound(globals.coin_pickup_sfx);
            schedule_for_destruction(pickup);

            Entity *light_e = get_entity_by_id(world, pickup->light_id);
            if (light_e) {
                schedule_for_destruction(light_e);
            }
            
            hero->coin_flash_timer = COIN_FLASH_TIME;
            emit_coin_particles(world->particle_system, pickup->position);
            if (hero->num_pickups >= world->num_pickups_needed_to_unlock_door) {
                if (world->by_type._Door) {
                    unlock_door(world->by_type._Door);
                }
            }
        }
    }

    if (hero->coin_flash_timer > 0.0f) {
        hero->coin_flash_timer -= dt;
    }
    
    if (world->by_type._Door && !world->by_type._Door->scheduled_for_destruction) {
        Door *door = world->by_type._Door;
        
        Rectangle2 door_rect = { door->position.x, door->position.y, door->size.x, door->size.y };
        if (are_intersecting(hero_rect, door_rect)) {
            if (!door->locked) {
                start_level_outro(world, door);
            }
        }
    }
    
    if (!hero->is_on_ground) {
        if (hero->velocity.y > 0.0f) {
            hero->state = HERO_STATE_JUMPING;
        } else if (hero->velocity.y < 0.0f) {
            hero->state = HERO_STATE_FALLING;
        }
    } else if (hero->velocity.x != 0.0f) {
        hero->state = HERO_STATE_MOVING;
    } else {
        hero->state = HERO_STATE_IDLE;
    }

    hero->num_invincibility_frames--;
    if (hero->num_invincibility_frames < 0) {
        hero->num_invincibility_frames = 0;
    }
}

void draw_single_hero(Hero *hero) {
    World *world = hero->world;

    Vector2 eye_size = hero->size * 0.25f;
    eye_size.y = hero->size.y * 0.35f;

    Vector2 left_eye_position = hero->position + (hero->size * 0.5f - eye_size) * 0.5f;
    left_eye_position.y += hero->size.y * 0.5f;
    
    Vector2 right_eye_position = left_eye_position + (hero->size * 0.5f);
    right_eye_position.y = left_eye_position.y;

    if (hero->is_facing_right) {
        left_eye_position.x  += 0.05f * hero->size.x;
        right_eye_position.x += 0.05f * hero->size.x;
    } else {
        left_eye_position.x  -= 0.05f * hero->size.x;
        right_eye_position.x -= 0.05f * hero->size.x;
    }

    switch (hero->state) {
        case HERO_STATE_JUMPING: {
            left_eye_position.y  += 0.05f * hero->size.y;
            right_eye_position.y += 0.05f * hero->size.y;
        } break;

        case HERO_STATE_FALLING: {
            left_eye_position.y  -= 0.05f * hero->size.y;
            right_eye_position.y -= 0.05f * hero->size.y;
        } break;
    }
    
    Vector2 body_screen_space_position = world_space_to_screen_space(world, hero->position);
    Vector2 body_screen_space_size     = world_space_to_screen_space(world, hero->size);

    Vector2 left_eye_screen_space_position  = world_space_to_screen_space(world, left_eye_position);
    Vector2 right_eye_screen_space_position = world_space_to_screen_space(world, right_eye_position);

    Vector2 screen_space_eye_size           = world_space_to_screen_space(world, eye_size);

    Vector4 body_color = v4(0, 0, 0, 1);
    if (hero->coin_flash_timer > 0.0f) {
        body_color = v4(1, 1, 0, 1);
    }
    
    immediate_quad(body_screen_space_position, body_screen_space_size, body_color);

    immediate_quad(left_eye_screen_space_position,  screen_space_eye_size, v4(1, 1, 1, 1));
    immediate_quad(right_eye_screen_space_position, screen_space_eye_size, v4(1, 1, 1, 1));
}

void damage_hero(Hero *hero, double damage_amount) {
    if (hero->num_invincibility_frames > 0) return;
    hero->num_invincibility_frames = MAX_HERO_INVINCIBILITY_FRAMES;
    
    hero->health -= damage_amount;
    emit_blood_particles(hero->world->particle_system, hero->position);
    if (hero->health <= 0.0) {
        hero->health = 0.0;
        play_sound(globals.death_sfx);
        schedule_for_destruction(hero);
        globals.should_switch_worlds = true;
    } else {
        play_sound(globals.damage_sfx);
    }
}

void update_single_enemy(Enemy *enemy, float dt) {
    World *world = enemy->world;
    assert(world);

    Tilemap *tilemap = world->tilemap;
    assert(tilemap);

    Vector2 move_dir = v2(0, 0);
    if (enemy->is_facing_right) move_dir.x = +1.0f;
    else                        move_dir.x = -1.0f;
    
    Vector2 new_position = enemy->position + move_dir * enemy->speed * dt;
    new_position.x -= 0.5f;
    new_position.y -= 0.5f;

    float front_x = new_position.x;
    if (!enemy->is_facing_right) {
        front_x -= 0.1f;
    } else {
        front_x += enemy->radius * 2.0f + 0.1f;
    }
    float foot_y  = new_position.y - 0.1f;
    u8 tile_below = get_tile_id_at(tilemap, v2(front_x, foot_y));

    bool will_fall = !is_tile_id_collidable(tilemap, tile_below);

    bool hitting_wall = false;
    if (!enemy->is_facing_right) {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x, enemy->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x, enemy->position.y + enemy->radius * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            hitting_wall = true;
        }

        if (new_position.x < 0.0f) {
            hitting_wall = true;
        }
    } else {
#if 0
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x + enemy->radius * 2.0f, enemy->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x + enemy->radius * 2.0f, enemy->position.y + enemy->radius * 0.9f));
#else
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x + enemy->radius * 2.0f, new_position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x + enemy->radius * 2.0f, new_position.y + enemy->radius * 0.9f));
#endif
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            hitting_wall = true;
        }

        if (new_position.x > world->size.x - enemy->radius * 2.0f) {
            hitting_wall = true;
        }
    }

    if (will_fall || hitting_wall) {
        if (!enemy->is_facing_right) {
            enemy->is_facing_right = true;
        } else {
            enemy->is_facing_right = false;
        }
    } else {
        enemy->position = new_position + v2(0.5f, 0.5f);
    }
    
    enemy->time_since_last_projectile += dt;
    if (enemy->time_since_last_projectile >= enemy->time_between_projectiles) {
        Vector2 projectile_position = enemy->position;
        if (enemy->is_facing_right) {
            projectile_position.x += enemy->radius;
        } else {
            projectile_position.x -= enemy->radius;
        }
        
        Projectile *projectile      = make_projectile(world);
        projectile->position        = projectile_position;
        projectile->is_facing_right = enemy->is_facing_right;
        projectile->speed           = 5.0f;
        projectile->color           = v4(1, 0, 1, 1);
        projectile->radius          = 0.2f;
        enemy->time_since_last_projectile = 0.0f;
    }
}

void draw_single_enemy(Enemy *enemy, bool disable_eye_flashing) {
    World *world = enemy->world;
    assert(world);

    float eye_radius = enemy->radius * 0.25f;

    Vector2 left_eye_position = enemy->position - v2(eye_radius, 0);
    left_eye_position.y += enemy->radius * 0.25f;
    
    Vector2 right_eye_position = left_eye_position + v2(enemy->radius * 0.75f);
    right_eye_position.y = left_eye_position.y;

    if (enemy->is_facing_right) {
        left_eye_position.x  += 0.05f * enemy->radius * 2.0f;
        right_eye_position.x += 0.05f * enemy->radius * 2.0f;
    } else {
        left_eye_position.x  -= 0.075f * enemy->radius * 2.0f;
        right_eye_position.x -= 0.075f * enemy->radius * 2.0f;
    }

    Vector4 eye_color = v4(1, 1, 1, 1);

    if (!disable_eye_flashing) { // This is true for the intro.
        Vector4 white     = v4(1.0f, 0, 0, 1);
        Vector4 red       = v4(0.5f, 0, 0, 1);

        float warning_threshold = enemy->time_between_projectiles * 0.8f;
        bool is_charging, is_angry_from_colliding, is_cooling_down;
        if (enemy->has_had_first_flash) {
            is_charging = enemy->time_since_last_projectile > warning_threshold;
            is_angry_from_colliding = enemy->num_nanoseconds_since_collision_with_the_hero < (s64)(1.0f * NS_PER_SECOND);
            is_cooling_down = enemy->time_since_last_projectile < 0.75f;
        } else {
            is_charging = enemy->time_since_last_projectile > warning_threshold;
            is_angry_from_colliding = false;
            is_cooling_down = false;
        }

        if (is_charging) {
            red   = v4(0.5f, 0, 0, 1);
            white = v4(1, 1, 1, 1);

            float fade_duration = enemy->time_between_projectiles - warning_threshold;
            float remaining = fade_duration - (enemy->time_between_projectiles - enemy->time_since_last_projectile);
            
            float t = remaining / fade_duration;
            eye_color = lerp(white, red, t);
            enemy->has_had_first_flash = true;
        } else if (is_cooling_down || is_angry_from_colliding) {
            float pulse = (sinf(globals.time_info.real_world_time * 20.0f) * 0.5f) + 0.5f;
            eye_color = lerp(white, red, pulse);
            enemy->has_had_first_flash = true;
        }
    }

    if (globals.draw_outlines) {
        Vector2 outline_size     = v2(0, enemy->radius * 1.1f);
        Vector2 outline_position = enemy->position;

        Vector2 screen_space_position = world_space_to_screen_space(world, outline_position);
        Vector2 screen_space_size     = world_space_to_screen_space(world, outline_size);
        immediate_circle(screen_space_position, screen_space_size.y, v4(0, 0, 0, 1));
    }
    
    {
        Vector2 screen_space_position = world_space_to_screen_space(world, enemy->position);
        Vector2 screen_space_size     = world_space_to_screen_space(world, v2(0, enemy->radius));
        immediate_circle(screen_space_position, screen_space_size.y, enemy->color);
    }
    
    {
        Vector2 screen_space_position = world_space_to_screen_space(world, left_eye_position);
        Vector2 screen_space_size     = world_space_to_screen_space(world, v2(0, eye_radius));
        immediate_circle(screen_space_position, screen_space_size.y, eye_color);
    }

    {
        Vector2 screen_space_position = world_space_to_screen_space(world, right_eye_position);
        Vector2 screen_space_size     = world_space_to_screen_space(world, v2(0, eye_radius));
        immediate_circle(screen_space_position, screen_space_size.y, eye_color);
    }
}

void update_single_projectile(Projectile *projectile, float dt) {
    World *world = projectile->world;
    assert(world);

    Tilemap *tilemap = world->tilemap;
    assert(tilemap);

    Vector2 move_dir = v2(0, 0);
    if (projectile->is_facing_right) move_dir.x = +1.0f;
    else                             move_dir.x = -1.0f;
    
    Vector2 new_position = projectile->position + move_dir * projectile->speed * dt;
    new_position.x -= 0.5f;
    new_position.y -= 0.5f;
    
    bool has_collided = false;

    Hero *hero = world->by_type._Hero;
    if (hero) {
        Rectangle2 hero_rect = { hero->position.x, hero->position.y, hero->size.x, hero->size.y };
        if (are_rect_and_circle_colliding(hero_rect, projectile->position, projectile->radius)) {
            damage_hero(hero, PROJECTILE_DAMAGE);
            has_collided = true;
        }
    }
    
    if (!projectile->is_facing_right) {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x, projectile->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x, projectile->position.y + projectile->radius * 2.0f * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            has_collided = true;
        }

        if (new_position.x < 0.0f) {
            has_collided = true;
        }
    } else {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x + projectile->radius * 2, projectile->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x + projectile->radius * 2, projectile->position.y + projectile->radius * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            has_collided = true;
        }

        if (new_position.x > world->size.x - projectile->radius * 2.0f) {
            has_collided = true;
        }
    }

    if (!globals.hard_mode_enabled) {
        u8 tile_id = get_tile_id_at(tilemap, v2(new_position.x, new_position.y - 1.0f));
        if (projectile->is_facing_right) {
            tile_id = get_tile_id_at(tilemap, v2(new_position.x + projectile->radius * 2.0f + 0.5f, new_position.y - 1.0f));
        }
        if (!is_tile_id_collidable(tilemap, tile_id)) {
            has_collided = true;
        }
    }
    
    if (!has_collided) {
        projectile->position = new_position + v2(0.5f, 0.5f);
    } else {
        schedule_for_destruction(projectile);
    }
}

void draw_single_projectile(Projectile *projectile) {
    World *world = projectile->world;
    assert(world);

    if (globals.draw_outlines) {
        Vector2 outline_size     = v2(0, projectile->radius * 1.1f);
        Vector2 outline_position = projectile->position;

        Vector2 screen_space_position = world_space_to_screen_space(world, outline_position);
        Vector2 screen_space_size     = world_space_to_screen_space(world, outline_size);
        immediate_circle(screen_space_position, screen_space_size.y, v4(0, 0, 0, 1));
    }

    Vector2 screen_space_position = world_space_to_screen_space(world, projectile->position);
    Vector2 screen_space_size     = world_space_to_screen_space(world, v2(0, projectile->radius));

    immediate_circle(screen_space_position, screen_space_size.y, projectile->color);    
}

void draw_single_pickup(Pickup *pickup) {
    World *world = pickup->world;
    assert(world);

#if 0
    Vector4 glow_color = v4(1, 1, 0, 0.15f);
    for (int i = 1; i <= 3; i++) {
        float glow_size = pickup->radius + (i * 0.05f);

        Vector2 screen_space_position = world_space_to_screen_space(world, pickup->position);
        Vector2 screen_space_size = world_space_to_screen_space(world, v2(0, glow_size));

        immediate_circle(screen_space_position, screen_space_size.y, glow_color);
    }
#endif

    if (globals.draw_outlines) {
        Vector2 outline_size     = v2(0, pickup->radius * 1.1f);
        Vector2 outline_position = pickup->position;

        Vector2 screen_space_position = world_space_to_screen_space(world, outline_position);
        Vector2 screen_space_size     = world_space_to_screen_space(world, outline_size);
        immediate_circle(screen_space_position, screen_space_size.y, v4(0, 0, 0, 1));
    }
    
    Vector2 screen_space_position = world_space_to_screen_space(world, pickup->position);
    Vector2 screen_space_size     = world_space_to_screen_space(world, v2(0, pickup->radius));

    immediate_circle(screen_space_position, screen_space_size.y, pickup->color);
}

void unlock_door(Door *door) {
    if (!door) return;
    
    door->locked = false;
    Entity *light_e = get_entity_by_id(door->world, door->light_id);
    if (light_e) {
        Light *light = (Light *)light_e;
        light->color = DOOR_LIGHT_UNLOCKED_COLOR;
    }
}

void draw_single_door(Door *door) {
    World *world = door->world;
    assert(world);

    set_shader(globals.shader_lighting);
    
    float thickness = 0.0f;
    if (globals.draw_outlines) {
        thickness = door->size.y * 0.1f;
    }
    
    Vector2 door_size = door->size;
    if (door->is_opening) {
        door_size.x = door->visual_width;
    }

    Vector4 color = v4(1, 1, 1, 1);//v4(0.25f, 0.15f, 0.05f, 1.0f);
    Vector2 screen_space_position, screen_space_size;
    
    if (globals.draw_outlines) {
        screen_space_position = world_space_to_screen_space(world, v2(door->position.x + thickness, door->position.y));
        screen_space_size     = world_space_to_screen_space(world, v2(door_size.x - thickness, door_size.y - thickness));
    } else {
        screen_space_position = world_space_to_screen_space(world, door->position);
        screen_space_size     = world_space_to_screen_space(world, door_size);
    }
    
    immediate_quad(screen_space_position, screen_space_size, color);
        
    if (globals.draw_outlines) {
        {
            Vector2 left_outline_position = door->position;
            Vector2 left_outline_size     = v2(thickness, door->size.y);

            immediate_quad(world_space_to_screen_space(world, left_outline_position), world_space_to_screen_space(world, left_outline_size), v4(0, 0, 0, 1));
        }

        {
            Vector2 top_outline_position = door->position + v2(thickness, door->size.y - thickness);
            Vector2 top_outline_size     = v2(door->size.x - thickness, thickness);

            immediate_quad(world_space_to_screen_space(world, top_outline_position), world_space_to_screen_space(world, top_outline_size), v4(0, 0, 0, 1));            
        }
    }

    set_shader(globals.shader_lighting);
}
