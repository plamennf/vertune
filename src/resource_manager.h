#pragma once

struct Shader;
struct Texture;
struct Sound;

Shader *find_or_load_shader(char *name);
Texture *find_or_load_texture(char *name);
Sound *find_or_load_sound(char *name, bool is_looping);
