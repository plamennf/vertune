#include "../general.h"
#include "../geometry.h"
#include "../array.h"
#include "../hash_table.h"
#include "packager.h"

#include <stdio.h>
#include <stdlib.h>
#include <stb_image.h>

const int PACKAGE_FILE_MAGIC_NUMBER = 0x4153504B;
const int PACKAGE_FILE_VERSION = 1;

struct Span {
    s64 size;
    u8 *data;
};

static inline Span null_span() {
    Span result;

    result.size = 0;
    result.data = NULL;

    return result;
}

static inline Span make_span(s64 size, u8 *data) {
    Span result;

    result.size = size;
    result.data = data;

    return result;
}

static Span my_read_entire_file(char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        logprintf("Failed to read file '%s'\n", filepath);
        return null_span();
    }
    defer { fclose(file); };

    fseek(file, 0, SEEK_END);
    auto length = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8 *data = new u8[length];
    fread(data, 1, length, file);
    fclose(file);

    return make_span(length, data);
}

bool create_package() {
    char *files_to_include[] = {
        "data/fonts/OpenSans-Regular.ttf",
        "data/fonts/Lora-BoldItalic.ttf",
        "data/fonts/Inconsolata-Regular.ttf",
        "data/fonts/Lora-Bold.ttf",
        "data/textures/heart_empty_16x16.png",
        "data/textures/heart_half_16x16.png",
        "data/textures/heart_full_16x16.png",
        "data/textures/restart_available.png",
        "data/textures/restart_taken.png",
        "data/sounds/coin-pickup.wav",
        "data/sounds/damage.wav",
        "data/sounds/death.wav",
        "data/sounds/enemy-kill.wav",
        "data/sounds/exit-menu.wav",
        "data/sounds/jump.wav",
        "data/sounds/level-completed.wav",
        "data/sounds/level-failed.wav",
        "data/sounds/level-music.wav",
        "data/sounds/menu-change-option.wav",
        "data/sounds/menu-music.wav",
        "data/sounds/menu-select.wav",
    };

    Array <Span> loaded_files;
    for (int i = 0; i < ArrayCount(files_to_include); i++) {
        Span span = my_read_entire_file(files_to_include[i]);
        if (span.size <= 0 || span.data == NULL) {
            return false;
        }
        loaded_files.add(span);
    }

    FILE *file = fopen("assets.pak", "wb");
    if (!file) {
        logprintf("Failed to open file 'src/generated_assets.h' for writing!\n");
        return 1;
    }
    defer { fclose(file); };
    
    fwrite(&PACKAGE_FILE_MAGIC_NUMBER, sizeof(int), 1, file);
    fwrite(&PACKAGE_FILE_VERSION, sizeof(int), 1, file);
    
    int num_assets = ArrayCount(files_to_include);
    fwrite(&num_assets, sizeof(int), 1, file);

    int i = 0;
    for (Span span : loaded_files) {
        char *extension = strrchr(files_to_include[i], '.');
        assert(extension);
        
        extension++;

        int type = -1; // 0 - font, 1 - texture, 2 - sound
        if (strings_match(extension, "ttf")) {
            type = PACKAGE_ASSET_FONT;
        } else if (strings_match(extension, "png")) {
            type = PACKAGE_ASSET_TEXTURE;
        } else if (strings_match(extension, "wav")) {
            type = PACKAGE_ASSET_SOUND;
        }

        char *slash = strrchr(files_to_include[i], '/');
        assert(slash);
        slash++;

        s64 name_length = (extension - 1) - slash;
        char *name = new char[name_length];
        memcpy(name, slash, name_length);
        
        fwrite(&type, sizeof(u8), 1, file);
        fwrite(&name_length, sizeof(s64), 1, file);
        fwrite(name, sizeof(char), name_length, file);

        int width = 0, height = 0;
        if (type == PACKAGE_ASSET_TEXTURE) {
            int channels;
            stbi_set_flip_vertically_on_load(1);
            stbi_uc *data = stbi_load(files_to_include[i], &width, &height, &channels, 4);
            if (!data) {
                logprintf("Failed to load image '%s' while creating package!\n", files_to_include[i]);
                return false;
            }
            defer { stbi_image_free(data); };

            s64 total_size = (s64)width * (s64)height * sizeof(u8) * 4;
            fwrite(&total_size, sizeof(s64), 1, file);
            fwrite(data, sizeof(u8), total_size, file);
        } else {
            fwrite(&span.size, sizeof(s64), 1, file);
            fwrite(span.data, sizeof(u8), span.size, file);
        }
        
        if (type == PACKAGE_ASSET_SOUND) {
            u8 is_looping = strstr(files_to_include[i], "music") ? 1 : 0;
            fwrite(&is_looping, sizeof(u8), 1, file);
        } else if (type == PACKAGE_ASSET_TEXTURE) {
            fwrite(&width, sizeof(int), 1, file);
            fwrite(&height, sizeof(int), 1, file);
        }
        
        i++;        
    }

    return true;
}

bool read_package(Package *package) {
    FILE *file = fopen("assets.pak", "rb");
    if (!file) {
        logprintf("Failed to open file '%s' for reading", "assets.pak");
        return false;
    }
    defer { fclose(file); };
    
    int magic_number;
    fread(&magic_number, sizeof(int), 1, file);
    if (magic_number != PACKAGE_FILE_MAGIC_NUMBER) {
        logprintf("Invalid magic number for 'assets.pak'\n");
        return false;
    }

    int version;
    fread(&version, sizeof(int), 1, file);
    if (version <= 0 || version > PACKAGE_FILE_VERSION) {
        logprintf("Invalid version for 'assets.dat'\n");
        return false;
    }

    int num_assets;
    fread(&num_assets, sizeof(int), 1, file);
    if (num_assets <= 0) {
        logprintf("Invalid num_assets for 'assets.dat'\n");
        return false;
    }

    Package_Asset_Entry *assets = new Package_Asset_Entry[num_assets];

    for (int i = 0; i < num_assets; i++) {
        u8 type;
        fread(&type, sizeof(u8), 1, file);
        if (type != PACKAGE_ASSET_FONT &&
            type != PACKAGE_ASSET_TEXTURE &&
            type != PACKAGE_ASSET_SOUND) {
            logprintf("Invalid type for %d asset\n", i);
            return false;
        }

        s64 name_length;
        fread(&name_length, sizeof(s64), 1, file);
        if (name_length <= 0) {
            logprintf("Invalid name length for %d asset\n", i);
            return false;
        }

        char *name = new char[name_length];
        fread(name, sizeof(char), name_length, file);
        
        s64 size;
        fread(&size, sizeof(s64), 1, file);
        if (size <= 0) {
            logprintf("Invalid size for %d asset\n", i);
            return false;
        }

        // TODO: In the future when reading this package file
        // read it whole(or stream chunks) and use offsets into the data
        // instead of what im doing right now
        u8 *data = new u8[size];
        fread(data, sizeof(u8), size, file);

        u8 is_looping = 0;
        if (type == PACKAGE_ASSET_SOUND) {
            fread(&is_looping, sizeof(u8), 1, file);
        }

        int width = 0, height = 0;
        if (type == PACKAGE_ASSET_TEXTURE) {
            fread(&width, sizeof(int), 1, file);
            if (width <= 0) {
                logprintf("Invalid width for %d asset\n", i);
                return false; 
            }

            fread(&height, sizeof(int), 1, file);

            if (height <= 0) {
                return false;
            }
        }

        Package_Asset_Entry entry;
        entry.type        = type;
        entry.name_length = name_length;
        entry.name        = name;
        entry.size        = size;
        entry.data        = data;
        entry.is_looping  = is_looping == 1 ? true : false;
        entry.width       = width;
        entry.height      = height;
        
        assets[i] = entry;
    }

    package->magic_number = magic_number;
    package->version      = version;
    package->num_assets   = num_assets;
    package->assets       = assets;

    return true;
}

Package_Asset_Entry *find_asset_by_name(Package *package, char *name) {
    for (int i = 0; i < package->num_assets; i++) {
        Package_Asset_Entry *entry = &package->assets[i];
        if (strings_match(entry->name, entry->name_length, name)) {
            return entry;
        }
    }

    return NULL;
}
