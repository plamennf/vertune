#include "main.h"
#include "entity.h"
#include "world.h"
#include "rendering.h"
#include "tilemap.h"

const float GRAVITY = -30.0f;
const float MOVE_SPEED = 5.0f;
const float JUMP_FORCE = 15.0f;
const float MAX_FALL_SPEED = -25.0f;
const float FAST_FALL_MULTIPLIER = 1.5f;

void update_single_hero(Hero *hero, float dt) {
    World *world = hero->world;
    assert(world);

    Tilemap *tilemap = world->tilemap;
    assert(tilemap);
    
    float input_x = 0.0f;
    if (is_key_down('A')) { input_x -= 1.0f; hero->is_facing_right = false; }
    if (is_key_down('D')) { input_x += 1.0f; hero->is_facing_right = true; }

    hero->velocity.x = input_x * MOVE_SPEED;

    if (is_key_down('W') && hero->is_on_ground) {
        hero->velocity.y   = JUMP_FORCE;
        hero->is_on_ground = false;
    }

    hero->velocity.y += GRAVITY * dt;

    if (!hero->is_on_ground && is_key_pressed('S')) {
        hero->velocity.y += GRAVITY * (FAST_FALL_MULTIPLIER - 1.0f) * dt;
    }

    hero->velocity.y = Max(hero->velocity.y, MAX_FALL_SPEED);

    Vector2 new_position = hero->position + hero->velocity * dt;

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
            Rectangle2 enemy_rect = { enemy->position.x, enemy->position.y, enemy->size.x, enemy->size.y };
            if (are_intersecting(hero_rect, enemy_rect)) {
                schedule_for_destruction(enemy);
                new_position.y = (int)new_position.y + 1.0f;
                hero->velocity.y = JUMP_FORCE * 1.5f;
                hero->is_on_ground = false;
                break;
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
    for (Projectile *projectile : world->by_type._Projectile) {
        if (projectile->scheduled_for_destruction) continue;

        if (are_rect_and_circle_colliding(hero_rect, projectile->position, projectile->radius)) {
            damage_hero(hero, 0.5);
            schedule_for_destruction(projectile);
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
    
    immediate_quad(body_screen_space_position, body_screen_space_size, v4(0, 0, 0, 1));

    immediate_quad(left_eye_screen_space_position,  screen_space_eye_size, v4(1, 1, 1, 1));
    immediate_quad(right_eye_screen_space_position, screen_space_eye_size, v4(1, 1, 1, 1));
}

void damage_hero(Hero *hero, double damage_amount) {
    hero->health -= damage_amount;
    if (hero->health <= 0.0) {
        hero->health = 0.0;
        schedule_for_destruction(hero);
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
    
    if (!enemy->is_facing_right) {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x, enemy->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x, enemy->position.y + enemy->size.y * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            new_position.x = (int)new_position.x + 1.0f;
            enemy->is_facing_right = true;
        }

        if (new_position.x < 0.0f) {
            new_position.x = (int)new_position.x + 1.0f;
            enemy->is_facing_right = true;
        }
    } else {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x + enemy->size.x, enemy->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x + enemy->size.x, enemy->position.y + enemy->size.y * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            new_position.x = static_cast <float>((int)new_position.x);
            enemy->is_facing_right = false;
        }

        if (new_position.x > world->size.x - enemy->radius * 2.0f) {
            new_position.x = world->size.x - enemy->radius * 2.0f;
            enemy->is_facing_right = false;
        }
    }

    enemy->position = new_position + v2(0.5f, 0.5f);

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

void draw_single_enemy(Enemy *enemy) {
    World *world = enemy->world;
    assert(world);

    Vector2 screen_space_position = world_space_to_screen_space(world, enemy->position);
    Vector2 screen_space_size     = world_space_to_screen_space(world, v2(0, enemy->radius));

    immediate_circle(screen_space_position, screen_space_size.y, enemy->color);
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
    if (!projectile->is_facing_right) {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x, projectile->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x, projectile->position.y + projectile->size.y * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            has_collided = true;
        }

        if (new_position.x < 0.0f) {
            has_collided = true;
        }
    } else {
        u8 tile1_id = get_tile_id_at(tilemap, v2(new_position.x + projectile->size.x, projectile->position.y));
        u8 tile2_id = get_tile_id_at(tilemap, v2(new_position.x + projectile->size.x, projectile->position.y + projectile->size.y * 0.9f));
        if (is_tile_id_collidable(tilemap, tile1_id) || is_tile_id_collidable(tilemap, tile2_id)) {
            has_collided = true;
        }

        if (new_position.x > world->size.x - projectile->radius * 2.0f) {
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

    Vector2 screen_space_position = world_space_to_screen_space(world, projectile->position);
    Vector2 screen_space_size     = world_space_to_screen_space(world, v2(0, projectile->radius));

    immediate_circle(screen_space_position, screen_space_size.y, projectile->color);    
}
