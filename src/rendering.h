#pragma once

bool init_rendering(Window_Type window, bool vsync);
void swap_buffers();

void set_viewport(int x, int y, int width, int height);

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

struct Shader;
Shader *make_shader();
void release_shader(Shader *shader);
bool load_shader(Shader *shader, char *filepath);
void set_shader(Shader *shader);
Shader *get_current_shader();
void refresh_transform();

void rendering_2d(int width, int height);
void rendering_2d(int width, int height, Matrix4 world_to_view_matrix);
