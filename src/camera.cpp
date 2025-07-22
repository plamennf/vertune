#include "main.h"
#include "camera.h"
#include "world.h"
#include "entity.h"

void update_camera(Camera *camera, World *world, float dt) {
    Entity *e = get_entity_by_id(world, camera->following_id);
    if (!e) return;

    Vector2 dead_zone_min = camera->target - camera->dead_zone_size * 0.5f;
    Vector2 dead_zone_max = camera->target + camera->dead_zone_size * 0.5f;

    if (e->position.x < dead_zone_min.x) {
        camera->target.x = e->position.x + camera->dead_zone_size.x * 0.5f;
    } else if (e->position.x > dead_zone_max.x) {
        camera->target.x = e->position.x - camera->dead_zone_size.x * 0.5f;
    }

    if (e->position.y < dead_zone_min.y) {
        camera->target.y = e->position.y + camera->dead_zone_size.y * 0.5f;
    } else if (e->position.y > dead_zone_max.y) {
        camera->target.y = e->position.y - camera->dead_zone_size.y * 0.5f;
    }

    Vector2 half_size = v2((float)VIEW_AREA_WIDTH * 0.5f, (float)VIEW_AREA_HEIGHT * 0.5f);
    clamp(&camera->target.x, half_size.x, world->size.x - half_size.x);
    clamp(&camera->target.y, half_size.y, world->size.y - half_size.y);

    float acceleration = 3.0f;
    float min_speed = 0.5f;
    float max_speed = 10.0f;
    
    Vector2 diff = camera->target - camera->position;
    float dist = length(diff);
    if (dist > 0.01f) {
        Vector2 direction = normalize_or_zero(diff);
        float speed = dist * acceleration;
        clamp(&speed, min_speed, max_speed);
        
        camera->position += direction * speed * dt;
    }
}

Matrix4 get_world_to_view_matrix(Camera *camera, World *world) {
    Matrix4 m = matrix4_identity();
    
    Vector2 position = camera->position - v2(VIEW_AREA_WIDTH * 0.5f, VIEW_AREA_HEIGHT * 0.5f);
    Vector2 screen_space_position = world_space_to_screen_space(world, position);

    screen_space_position.x = static_cast <float>((int)screen_space_position.x);
    screen_space_position.y = static_cast <float>((int)screen_space_position.y);
    
    m._14 = -screen_space_position.x;
    m._24 = -screen_space_position.y;
    m._34 = 0.0f;

    return m;
}
