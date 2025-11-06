#pragma once

bool init_rendering(SDL_Window *window, bool vsync);
void swap_buffers();

void set_viewport(int x, int y, int width, int height);

// @TODO: Maybe move these to be inside the shader as extra-parsables.

enum Blend_Mode {
    BLEND_MODE_OFF,
    BLEND_MODE_ALPHA,
};

enum Cull_Mode {
    CULL_MODE_OFF,
    CULL_MODE_BACK,
    CULL_MODE_FRONT,
};

enum Depth_Test_Mode {
    DEPTH_TEST_OFF,
    DEPTH_TEST_LEQUAL,
};

void set_blend_mode(Blend_Mode blend_mode);
void set_cull_mode(Cull_Mode cull_mode);
void set_depth_test_mode(Depth_Test_Mode depth_test_mode);

enum Texture_Format {
    TEXTURE_FORMAT_UNKNOWN,

    TEXTURE_FORMAT_R8,
    TEXTURE_FORMAT_RGBA8,
};

inline int get_bpp(Texture_Format format) {
    switch (format) {
        case TEXTURE_FORMAT_RGBA8: return 4;
        case TEXTURE_FORMAT_R8:    return 1;
    }

    assert(!"Invalid format");
    return 0;
}

struct Texture;
Texture *make_texture();
void release_texture(Texture *texture);
void load_texture_from_data(Texture *texture, int width, int height, Texture_Format format, u8 *data);
Texture *load_texture_from_file(char *filepath);
void update_texture(Texture *texture, int x, int y, int width, int height, u8 *data);
void set_texture(int slot, Texture *texture, bool point_sample = true);

struct Framebuffer;
Framebuffer *make_framebuffer(int width, int height);
void release_framebuffer(Framebuffer *framebuffer);
void blit_framebuffer_to_back_buffer_with_letter_boxing(Framebuffer *framebuffer);
void set_framebuffer(Framebuffer *framebuffer);
void clear_framebuffer(float r, float g, float b, float a);

void immediate_begin();
void immediate_flush();
void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3, Vector4 color);
void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 color);
void immediate_quad(float x, float y, float w, float h, Vector4 color);
void immediate_quad(Vector2 position, Vector2 size, Vector4 color);
void immediate_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color);
void immediate_circle(Vector2 center, float radius, Vector4 color);

struct Shader;
Shader *make_shader();
void release_shader(Shader *shader);
bool load_shader(Shader *shader, char *source, char *debug_name);
void set_shader(Shader *shader);
Shader *get_current_shader();
void refresh_transform();

void rendering_2d(int width, int height);
void rendering_2d(int width, int height, Matrix4 world_to_view_matrix);
void rendering_2d(int width, int height, float y_offset);

struct Dynamic_Font;
void draw_text(Dynamic_Font *font, char *text, int x, int y, Vector4 color);
