#pragma once

enum Package_Asset_Type : u8 {
    PACKAGE_ASSET_FONT,
    PACKAGE_ASSET_TEXTURE,
    PACKAGE_ASSET_SOUND,
};

struct Package_Asset_Entry {
    u8 type;
    s64 name_length;
    char *name;
    s64 size;
    u8 *data;
    bool is_looping; // Only valid for sounds.
    int width; // Only valid for textures
    int height; // Only valid for textures
};

struct Package {
    int magic_number;
    int version;
    int num_assets;
    
    Package_Asset_Entry *assets;
};

bool create_package();
bool read_package(Package *package);

Package_Asset_Entry *find_asset_by_name(Package *package, char *name);
