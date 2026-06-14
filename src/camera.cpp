#include "main.h"
#include "camera.h"
#include "world.h"
#include "entity.h"

void update_camera(Camera *camera, World *world, float dt) {
    if (camera->intro_active) {
        camera->intro_timer += dt;
        float t = camera->intro_timer / camera->intro_duration;
        if (t >= 1.0f) {
            t = 1.0f;
            camera->intro_active = false;
        }

        float smooth_t = t * t * (3.0f - 2.0f * t);

        camera->position = lerp(camera->intro_start_pos, camera->intro_end_pos, smooth_t);

        float zoom_t = powf(smooth_t, 0.7f);
        camera->zoom     = lerp(camera->intro_start_zoom, camera->intro_end_zoom, smooth_t);
        return;
    }
    
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
    clamp(&camera->target.y, half_size.y, world->size.y - half_size.y + 5.0f);

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
    } else {
        camera->position = camera->target;
    }
}

Matrix4 get_world_to_view_matrix(Camera *camera, World *world) {
    Matrix4 m = matrix4_identity();

    float z = camera->zoom;
    m._11 = z;
    m._22 = z;
    
    Vector2 position = camera->position - v2(VIEW_AREA_WIDTH * 0.5f, VIEW_AREA_HEIGHT * 0.5f);
    Vector2 screen_space_position = world_space_to_screen_space(world, position);

    screen_space_position.x = roundf(screen_space_position.x);
    screen_space_position.y = roundf(screen_space_position.y);
    
    m._14 = -screen_space_position.x * z;
    m._24 = -screen_space_position.y * z;
    m._34 = 0.0f;

    return m;
}
