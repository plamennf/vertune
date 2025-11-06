#include "main.h"
#include "audio.h"

static SDL_AudioDeviceID audio_device;

static Array <Sound *> current_sounds;

static void SDLCALL audio_callback(void *userdata, Uint8 *stream, int len) {
    SDL_memset(stream, 0, len);

    for (int i = 0; i < current_sounds.count; i++) {
        Sound *sound = current_sounds[i];
        if (!sound || !sound->playing)
            continue;

        Uint32 remaining = sound->length - sound->position;
        Uint32 to_copy = (remaining > (Uint32)len) ? (Uint32)len : remaining;

        // Mix audio into the output stream
        SDL_MixAudioFormat(
            stream,
            sound->buffer + sound->position,
            sound->spec.format,
            to_copy,
            (int)(128 * sound->volume)
        );

        sound->position += to_copy;

        if (sound->position >= sound->length) {
            if (sound->looping) {
                sound->position = 0;
            } else {
                sound->playing = false;
                sound->position = 0;
            }
        }
    }
}

bool init_audio() {
    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);

    desired.freq = 48000;
    desired.format = AUDIO_F32;
    desired.channels = 2;
    desired.samples = 4096; // Buffer size
    desired.callback = audio_callback;
    desired.userdata = NULL;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (!audio_device) {
        logprintf("Failed to open audio device: %s", SDL_GetError());
        return false;
    }
    
    SDL_PauseAudioDevice(audio_device, 0); // Start playback

    logprintf("Audio device ID: %u\n", (unsigned)audio_device);
    
    return true;
}

void destroy_audio() {
    SDL_CloseAudioDevice(audio_device);
    
    for (Sound *sound : current_sounds) {
        free_sound(sound);
    }
}

Sound *load_sound(char *filepath, bool looping) {
    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;

    if (!SDL_LoadWAV(filepath, &spec, &buf, &len)) {
        logprintf("Failed to load sound %s: %s\n", filepath, SDL_GetError());
        return NULL;
    }

    // --- Convert to match device format ---
    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt,
                          spec.format, spec.channels, spec.freq,
                          AUDIO_F32, 2, 48000) < 0) {
        logprintf("Failed to build audio CVT for %s: %s\n", filepath, SDL_GetError());
        SDL_FreeWAV(buf);
        return NULL;
    }

    cvt.len = len;
    cvt.buf = (Uint8 *)SDL_malloc(len * cvt.len_mult);
    SDL_memcpy(cvt.buf, buf, len);
    SDL_FreeWAV(buf);

    if (SDL_ConvertAudio(&cvt) < 0) {
        logprintf("Audio conversion failed for %s: %s\n", filepath, SDL_GetError());
        SDL_free(cvt.buf);
        return NULL;
    }

    Sound *sound = new Sound();
    sound->spec.format = AUDIO_F32;
    sound->spec.channels = 2;
    sound->spec.freq = 48000;
    sound->buffer = cvt.buf;
    sound->length = cvt.len_cvt;
    sound->position = 0;
    sound->playing = false;
    sound->looping = looping;
    sound->volume  = 0.5f;

    logprintf("Loaded sound: %s len=%u format=%u\n",
              filepath, sound->length, sound->spec.format);

    return sound;
}

Sound *load_sound_from_memory(s64 data_size, u8 *data, bool looping) {
    if (!data || data_size <= 0) return NULL;

    SDL_RWops *rw = SDL_RWFromMem(data, (int)data_size);
    if (!rw) {
        logprintf("Failed to create SDL_RWops from memory: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;

    if (!SDL_LoadWAV_RW(rw, 1, &spec, &buf, &len)) {
        logprintf("Failed to load WAV from memory: %s\n", SDL_GetError());
        SDL_RWclose(rw);
        return NULL;
    }

    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt,
                          spec.format, spec.channels, spec.freq,
                          AUDIO_F32, 2, 48000) < 0) {
        logprintf("Failed to build audio CVT for memory WAV: %s\n", SDL_GetError());
        SDL_FreeWAV(buf);
        return NULL;
    }

    cvt.len = len;
    cvt.buf = (Uint8 *)SDL_malloc(len * cvt.len_mult);
    SDL_memcpy(cvt.buf, buf, len);
    SDL_FreeWAV(buf);

    if (SDL_ConvertAudio(&cvt) < 0) {
        logprintf("Audio conversion failed for memory WAV: %s\n", SDL_GetError());
        SDL_free(cvt.buf);
        return NULL;
    }

    Sound *sound = new Sound();
    sound->spec.format = AUDIO_F32;
    sound->spec.channels = 2;
    sound->spec.freq = 48000;
    sound->buffer = cvt.buf;
    sound->length = cvt.len_cvt;
    sound->position = 0;
    sound->playing = false;
    sound->looping = looping;
    sound->volume  = 0.5f;

    logprintf("Loaded sound from memory, len=%u format=%u\n", sound->length, sound->spec.format);
    return sound;
}

void play_sound(Sound *sound) {
    if (!sound) return;
    
    sound->playing = true;
    sound->position = 0;

    if (sound->looping) {
        sound->volume = globals.master_volume * globals.music_volume;
    } else {
        sound->volume = globals.master_volume * globals.sfx_volume;
    }
    
    SDL_LockAudioDevice(audio_device);
    for (int i = 0; i < current_sounds.count; i++) {
        if (current_sounds[i] == sound) {
            SDL_UnlockAudioDevice(audio_device);
            return;
        }
    }
    current_sounds.add(sound);
    SDL_UnlockAudioDevice(audio_device);
}

void stop_sound(Sound *sound) {
    if (!sound) return;

    sound->playing = false;
}

void free_sound(Sound *sound) {
    if (!sound) return;
    if (!sound->buffer) return;
    
    SDL_free(sound->buffer);
}

void update_volumes() {
    for (Sound *sound : current_sounds) {
        if (sound->looping) {
            sound->volume = globals.master_volume * globals.music_volume;
        } else {
            sound->volume = globals.master_volume * globals.sfx_volume;
        }
    }
}
