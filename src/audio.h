#pragma once

struct Sound {
    SDL_AudioSpec spec;
    u8 *buffer;
    u32 length;
    u32 position;
    bool playing;
    bool looping;
    float volume;
};

bool init_audio();
void destroy_audio();

Sound *load_sound(char *filepath, bool looping);
Sound *load_sound_from_memory(s64 data_size, u8 *data, bool looping);
void play_sound(Sound *sound);
void stop_sound(Sound *sound);

void free_sound(Sound *sound);

void update_volumes();
