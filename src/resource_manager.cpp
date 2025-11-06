#include "main.h"
#include "resource_manager.h"
#include "rendering.h"
#include "audio.h"

#include <stdio.h>

static char *texture_directory = "data/textures";
static char *texture_extension = "png";

static char *sound_directory = "data/sounds";
static char *sound_extension = "wav";

template <typename T>
struct Resource_Info {
    char *name;
    T *data;
};

static String_Hash_Table <Resource_Info <Texture>> loaded_textures;
static String_Hash_Table <Resource_Info <Sound>> loaded_sounds;

Texture *find_or_load_texture(char *name) {
    auto _info = loaded_textures.find(name);
    if (_info) return (*_info).data;

#ifdef USE_PACKAGE
    Package_Asset_Entry *entry = find_asset_by_name(&globals.package, name);
    if (!entry || entry->type != PACKAGE_ASSET_TEXTURE) {
        logprintf("No texture '%s' found in asset package.\n", name);
        return NULL;
    }

    Texture *texture = make_texture();
    load_texture_from_data(texture, entry->width, entry->height, TEXTURE_FORMAT_RGBA8, entry->data);
#else
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s.%s", texture_directory, name, texture_extension);
    if (!file_exists(full_path)) {
        logprintf("Unable to find file '%s' in '%s'!\n", name, texture_directory);
        return NULL;
    }

    Texture *texture = load_texture_from_file(full_path);
    if (!texture) {
        free(texture);
        return NULL;
    }
#endif

    Resource_Info <Texture> info;
    info.name      = copy_string(name);
    info.data      = texture;

    loaded_textures.add(name, info);
    return texture;
}

Sound *find_or_load_sound(char *name, bool is_looping) {
    auto _info = loaded_sounds.find(name);
    if (_info) return (*_info).data;

#ifdef USE_PACKAGE
    Package_Asset_Entry *entry = find_asset_by_name(&globals.package, name);
    if (!entry || entry->type != PACKAGE_ASSET_SOUND) {
        logprintf("No sound '%s' found in asset package.\n", name);
        return NULL;
    }

    Sound *sound = load_sound_from_memory(entry->size, entry->data, entry->is_looping);
    if (!sound) {
        return NULL;
    }
#else    
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s.%s", sound_directory, name, sound_extension);
    if (!file_exists(full_path)) {
        logprintf("Unable to find file '%s' in '%s'!\n", name, sound_directory);
        return NULL;
    }

    Sound *sound = load_sound(full_path, is_looping);
    if (!sound) {
        free(sound);
        return NULL;
    }
#endif

    Resource_Info <Sound> info;
    info.name      = copy_string(name);
    info.data      = sound;

    loaded_sounds.add(name, info);
    return sound;
}
