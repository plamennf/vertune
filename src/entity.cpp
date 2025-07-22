#include "main.h"
#include "entity.h"
#include "world.h"
#include "rendering.h"

const float GRAVITY = -30.0f;
const float MOVE_SPEED = 5.0f;
const float JUMP_FORCE = 15.0f;
const float MAX_FALL_SPEED = -25.0f;
const float FAST_FALL_MULTIPLIER = 1.5f;

void update_single_hero(Hero *hero, float dt) {    
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

    hero->position += hero->velocity * dt;

    if (hero->position.y <= 0.0f) {
        hero->position.y   = 0.0f;
        hero->velocity.y   = 0.0f;
        hero->is_on_ground = true;
    }

    if (hero->position.x < 0.0f) {
        hero->position.x = 0.0f;
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
