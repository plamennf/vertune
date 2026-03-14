#pragma once

struct World;

struct Particle {
    Vector2 position;
    Vector2 velocity;
    Vector4 color;
    float lifetime;
    float age;
    float size;
};

struct Particle_System {
    Array <Particle> particles;
};

void update_particles(Particle_System *system, float dt);
void draw_particles(Particle_System *system, World *world);

void emit_jump_particles(Particle_System *system, Vector2 position);
void emit_stomp_particles(Particle_System *system, Vector2 position);
void emit_blood_particles(Particle_System *system, Vector2 position);
void emit_coin_particles(Particle_System *system, Vector2 position);
