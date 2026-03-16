#include "main.h"
#include "particles.h"
#include "rendering.h"
#include "world.h"

void update_particles(Particle_System *system, float dt) {
    for (int i = 0; i < system->particles.size();) {
        Particle *particle = &system->particles[i];
        particle->age += dt;
        particle->position += particle->velocity * dt;
        if (particle->age >= particle->lifetime) {
            system->particles[i] = system->particles.back();
            system->particles.pop_back();
            continue;
        }

        i++;
        particle->color.w = 1.0f - (particle->age / particle->lifetime);
    }
}

void draw_particles(Particle_System *system, World *world) {
    for (Particle p : system->particles) {
        Vector2 position = world_space_to_screen_space(world, p.position);
        Vector2 size = world_space_to_screen_space(world, v2(p.size, p.size));
        immediate_quad(position, size, p.color);
    }
}

void emit_jump_particles(Particle_System *system, Vector2 position) {
    for (int i = 0; i < 10; i++) {
        Particle p;
        p.position = position;
        p.velocity = v2((random_float()-0.5f)*4.0f, random_float()*6.0f);
        p.color = v4(1, 1, 1, 1);
        p.lifetime = 0.5f;
        p.age = 0.0f;
        p.size = 0.1f;
        system->particles.push_back(p);
    }
}

void emit_stomp_particles(Particle_System *system, Vector2 position) {
    for (int i = 0; i < 15; i++) {
        Particle p;
        p.position = position;
        p.velocity = v2((random_float()-0.5f)*5.0f, random_float()*8.0f);
        p.color = v4(1, 1, 0, 1);
        p.lifetime = 0.4f;
        p.age = 0.0f;
        p.size = 0.12f;
        system->particles.push_back(p);
    }
}

void emit_blood_particles(Particle_System *system, Vector2 position) {
    for (int i = 0; i < 20; i++) {
        Particle p;
        p.position = position;
        p.velocity = v2((random_float()-0.5f)*3.0f, random_float()*4.0f);
        p.color = v4(0.8f, 0.0f, 0.0f, 1.0f);
        p.lifetime = 0.7f;
        p.age = 0.0f;
        p.size = 0.08f;
        system->particles.push_back(p);
    }
}

void emit_coin_particles(Particle_System *system, Vector2 position) {
    for (int i = 0; i < 12; i++) {
        float angle = random_float() * 2.0f * 3.14159f;
        float speed = 2.0f + random_float() * 5.0f;
        
        Particle p;
        p.position = position;
        p.velocity = v2(cosf(angle) * speed, sinf(angle) * speed);
        p.color = v4(1.0f, 1.0f, 0.0f, 1.0f);
        p.lifetime = 0.4f + random_float() * 0.2f;
        p.age = 0.0f;
        p.size = 0.05f + random_float() * 0.1f;
        system->particles.push_back(p);
    }
}
