#include "main.h"
#include "resource_manager.h"
#include "rendering.h"

#include <stdio.h>
#include <fswatcher/fswatcher.h>

static char *shader_directory = "data/shaders";
#ifdef RENDER_OPENGL
static char *shader_extension = "glsl";
#endif

template <typename T>
struct Resource_Info {
    char *full_path;
    char *name;
    T *data;
};

static String_Hash_Table <Resource_Info <Shader>> loaded_shaders;

static fswatcher_t my_fswatcher;

static bool hotload_callback(fswatcher_event_handler *handler, fswatcher_event_type event_type, const char *source, const char *destination);

void init_resource_manager() {
    my_fswatcher = fswatcher_create(FSWATCHER_CREATE_RECURSIVE, FSWATCHER_EVENT_ALL, "data", NULL);
}

void do_hotloading() {
    fswatcher_event_handler handler = {
        hotload_callback
    };
    
    fswatcher_poll(my_fswatcher, &handler, NULL);
}

Shader *find_or_load_shader(char *name) {
    auto _info = loaded_shaders.find(name);
    if (_info) return (*_info).data;
    
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s.%s", shader_directory, name, shader_extension);
    if (!os_file_exists(full_path)) {
        logprintf("Unable to find file '%s' in '%s'!\n", name, shader_directory);
        return NULL;
    }

    Shader *shader = make_shader();
    if (!load_shader(shader, full_path)) {
        free(shader);
        return NULL;
    }

    Resource_Info <Shader> info;
    info.full_path = copy_string(full_path);
    info.name      = copy_string(name);
    info.data      = shader;

    loaded_shaders.add(name, info);
    return shader;
}

static char *replace_backslashes_with_forwardslashes(char *s) {
    if (!s) return NULL;

    for (char *at = s; *at; at++) {
        if (*at == '\\') *at = '/';
    }

    return s;
}

static bool hotload_callback(fswatcher_event_handler *handler, fswatcher_event_type event_type, const char *_source, const char *_destination) {
    // TODO: Add support for create and remove
    if (event_type != FSWATCHER_EVENT_MODIFY) return false;

    char *source      = replace_backslashes_with_forwardslashes((char *)_source);
    char *destination = replace_backslashes_with_forwardslashes((char *)_destination);
    
    char *extension = strrchr(source, '.');
    if (!extension) {
        //logprintf("[hotload_callback] Invalid file: '%s' has no extension!", source);
        return false;
    }
    extension++;

    if (strings_match(extension, shader_extension)) {
        char *src = source;
        if (strrchr(src, '/')) {
            src = strrchr(src, '/');
            src++;
        }

        char *name = copy_string(src);
        defer { delete [] name; };

        char *dot = strrchr(name, '.');
        if (dot) {
            name[dot - name] = 0;
        }

        Resource_Info <Shader> shader_info;
        auto _info = loaded_shaders.find(name);
        if (_info) shader_info = *_info;
        else {
            logprintf("Unable to find file '%s' in '%s'!\n", name, shader_directory);
            return false;
        }

        release_shader(shader_info.data);
        load_shader(shader_info.data, shader_info.full_path);
    }

    return true;
}
