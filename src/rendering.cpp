#include "main.h"
#include "rendering.h"
#include "font.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Texture *load_texture_from_file(char *filepath) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc *data = stbi_load(filepath, &width, &height, &channels, 4);
    if (!data) {
        logprintf("Failed to load image '%s'.\n", filepath);
        return NULL;
    }
    defer { stbi_image_free(data); };

    Texture *texture = make_texture();
    load_texture_from_data(texture, width, height, TEXTURE_FORMAT_RGBA8, data);

    return texture;
}

void rendering_2d(int width, int height) {
    Matrix4 m = matrix4_identity();
    rendering_2d(width, height, m);
}

void rendering_2d(int width, int height, Matrix4 world_to_view_matrix) {
    float w = (float)width;
    if (w < 1.0f) w = 1.0f;
    float h = (float)height;
    if (h < 1.0f) h = 1.0f;

    Matrix4 m = make_orthographic(0.0f, w, 0.0f, h, -1.0f, 1.0f);

    globals.view_to_proj_matrix    = m;
    globals.world_to_view_matrix   = world_to_view_matrix;
    globals.object_to_world_matrix = matrix4_identity();

    refresh_transform();    
}

void draw_text(Dynamic_Font *font, char *text, int x, int y, Vector4 color) {
    Texture *last_texture = NULL;
    
    font->prep_text(text, x, y);
    immediate_begin();
    for (Font_Quad quad : font->font_quads) {
        Vector2 p0 = v2(quad.x0, quad.y0);
        Vector2 p1 = v2(quad.x1, quad.y0);
        Vector2 p2 = v2(quad.x1, quad.y1);
        Vector2 p3 = v2(quad.x0, quad.y1);
        
        Vector2 uv0 = v2(quad.u0, quad.v1);
        Vector2 uv1 = v2(quad.u1, quad.v1);
        Vector2 uv2 = v2(quad.u1, quad.v0);
        Vector2 uv3 = v2(quad.u0, quad.v0);

        if (last_texture != quad.texture) {
            set_texture(0, quad.texture, false);
            last_texture = quad.texture;
        }

        immediate_quad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, color);
    }
    immediate_flush();

    font->font_quads.count = 0;
}
