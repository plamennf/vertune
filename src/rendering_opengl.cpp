#include "main.h"

#include "rendering.h"

#include <GL/glew.h>

struct Texture {
    int width;
    int height;

    Texture_Format format;
    int bytes_per_pixel;

    GLenum internal_format;
    GLenum source_format;

    GLuint id;
};

struct Framebuffer {
    int width;
    int height;

    GLuint fbo_id;

    GLuint color_id;
};

struct Shader {
    GLuint program_id;

    GLint object_to_proj_matrix_loc;
    GLint view_to_proj_matrix_loc;
    GLint world_to_view_matrix_loc;
    GLint object_to_world_matrix_loc;
};

struct Immediate_Vertex {
    Vector2 position;
    Vector4 color;
    Vector2 uv;
};

static SDL_Window *window;
static Shader *current_shader;

const int MAX_IMMEDIATE_VERTICES = 2400;
static Immediate_Vertex immediate_vertices[MAX_IMMEDIATE_VERTICES];
static int num_immediate_vertices;

static GLuint immediate_vbo;

bool init_rendering(SDL_Window *_window, bool vsync) {
    window = _window;

    globals.gl_context = SDL_GL_CreateContext(window);
    if (!globals.gl_context) {
        logprintf("Failed to create opengl context!\n");
        SDL_DestroyWindow(globals.window);
        return false;
    }
    SDL_GL_MakeCurrent(globals.window, globals.gl_context);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        logprintf("Failed to initialize GLEW!\n");
        return false;
    }

    if (vsync) {
        SDL_GL_SetSwapInterval(1);
        logprintf("vsync: on\n");
    } else {
        SDL_GL_SetSwapInterval(0);
        logprintf("vsync: off\n");
    }

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_MULTISAMPLE);

    glGenBuffers(1, &immediate_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, immediate_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_IMMEDIATE_VERTICES * sizeof(Immediate_Vertex), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    num_immediate_vertices = 0;

    globals.object_to_world_matrix = matrix4_identity();
    globals.view_to_proj_matrix    = matrix4_identity();
    globals.world_to_view_matrix   = matrix4_identity();
    globals.object_to_world_matrix = matrix4_identity();
    
    return true;
}

void swap_buffers() {
    SDL_GL_SwapWindow(window);
}

void set_viewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void set_blend_mode(Blend_Mode blend_mode) {
    switch (blend_mode) {
        case BLEND_MODE_OFF: {
            glDisable(GL_BLEND);
        } break;

        case BLEND_MODE_ALPHA: {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } break;

        default: {
            assert(!"Invalid blend mode");
        } break;
    }
}

void set_cull_mode(Cull_Mode cull_mode) {
    switch (cull_mode) {
        case CULL_MODE_OFF: {
            glDisable(GL_CULL_FACE);
        } break;

        case CULL_MODE_BACK: {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
        } break;

        case CULL_MODE_FRONT: {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glFrontFace(GL_CCW);
        } break;

        default: {
            assert(!"Invalid cull mode");
        } break;
    }
}

void set_depth_test_mode(Depth_Test_Mode depth_test_mode) {
    switch (depth_test_mode) {
        case DEPTH_TEST_OFF: {
            glDisable(GL_DEPTH_TEST);
        } break;

        case DEPTH_TEST_LEQUAL: {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
        } break;

        default: {
            assert(!"Invalid depth mode");
        } break;
    }
}

Texture *make_texture() {
    return (Texture *)malloc(sizeof(Texture));
}

void release_texture(Texture *texture) {
    if (texture->id) {
        glDeleteTextures(1, &texture->id);
        texture->id = 0;
    }
}

static GLenum tf_to_gl_internal_format(Texture_Format format) {
    switch (format) {
        case TEXTURE_FORMAT_RGBA8: return GL_SRGB8_ALPHA8;
        case TEXTURE_FORMAT_R8:    return GL_R8;
    }

    assert(!"Invalid format");
    return 0;
}

static GLenum tf_to_gl_source_format(Texture_Format format) {
    switch (format) {
        case TEXTURE_FORMAT_RGBA8: return GL_RGBA;
        case TEXTURE_FORMAT_R8:    return GL_RED;
    }

    assert(!"Invalid format");
    return 0;
}

void load_texture_from_data(Texture *texture, int width, int height, Texture_Format format, u8 *data) {
    texture->width  = width;
    texture->height = height;

    texture->format          = format;
    texture->bytes_per_pixel = get_bpp(format);

    texture->internal_format = tf_to_gl_internal_format(format);
    texture->source_format   = tf_to_gl_source_format(format);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    glGenTextures(1, &texture->id);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexImage2D(GL_TEXTURE_2D, 0, texture->internal_format, width, height, 0, texture->source_format, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void update_texture(Texture *texture, int x, int y, int width, int height, u8 *data) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, texture->source_format, GL_UNSIGNED_BYTE, data);
}

void set_texture(int slot, Texture *texture, bool point_sample) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture->id);

    if (point_sample) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);        
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

Framebuffer *make_framebuffer(int width, int height) {
    Framebuffer *framebuffer = (Framebuffer *)malloc(sizeof(*framebuffer));

    framebuffer->width  = width;
    framebuffer->height = height;
    
    glGenFramebuffers(1, &framebuffer->fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo_id);

    glGenTextures(1, &framebuffer->color_id);
    glBindTexture(GL_TEXTURE_2D, framebuffer->color_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer->color_id, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logprintf("Framebuffer is not complete!!!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return framebuffer;
}

void release_framebuffer(Framebuffer *framebuffer) {
    if (framebuffer->color_id) {
        glDeleteTextures(1, &framebuffer->color_id);
        framebuffer->color_id = 0;
    }

    if (framebuffer->fbo_id) {
        glDeleteFramebuffers(1, &framebuffer->fbo_id);
        framebuffer->fbo_id = 0;
    }
}

void blit_framebuffer_to_back_buffer_with_letter_boxing(Framebuffer *framebuffer) {
    assert(framebuffer);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->fbo_id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    int dx0 = (globals.window_width  - framebuffer->width)  / 2;
    int dy0 = (globals.window_height - framebuffer->height) / 2;
    int dx1 = dx0 + framebuffer->width;
    int dy1 = dy0 + framebuffer->height;
    
    glBlitFramebuffer(0, 0, framebuffer->width, framebuffer->height,
                      dx0, dy0, dx1, dy1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void set_framebuffer(Framebuffer *framebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo_id);
}

void clear_framebuffer(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void immediate_begin() {
    immediate_flush();
}

static void enable_immediate_vertex_format() {
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void *)offsetof(Immediate_Vertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void *)offsetof(Immediate_Vertex, color));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void *)offsetof(Immediate_Vertex, uv));
    glEnableVertexAttribArray(2);
}

static void disable_immediate_vertex_format() {
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
}

void immediate_flush() {
    if (!num_immediate_vertices) return;

    glBindBuffer(GL_ARRAY_BUFFER, immediate_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, num_immediate_vertices * sizeof(Immediate_Vertex), immediate_vertices);

    enable_immediate_vertex_format();

    glDrawArrays(GL_TRIANGLES, 0, num_immediate_vertices);

    disable_immediate_vertex_format();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    num_immediate_vertices = 0;
}

static void put_vertex(Immediate_Vertex *v, Vector2 position, Vector4 color, Vector2 uv) {
    v->position = position;
    v->color    = color;
    v->uv       = uv;
}

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3, Vector4 color) {
    if (num_immediate_vertices + 6 > MAX_IMMEDIATE_VERTICES) immediate_flush();

    auto v = immediate_vertices + num_immediate_vertices;

    put_vertex(&v[0], p0, color, uv0);
    put_vertex(&v[1], p1, color, uv1);
    put_vertex(&v[2], p2, color, uv2);
    
    put_vertex(&v[3], p0, color, uv0);
    put_vertex(&v[4], p2, color, uv2);
    put_vertex(&v[5], p3, color, uv3);
    
    num_immediate_vertices += 6;
}

void immediate_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 color) {
    Vector2 uv0 = v2(0, 0);
    Vector2 uv1 = v2(1, 0);
    Vector2 uv2 = v2(1, 1);
    Vector2 uv3 = v2(0, 1);

    immediate_quad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, color);
}

void immediate_quad(float x, float y, float w, float h, Vector4 color) {
    Vector2 p0 = v2(x, y);
    Vector2 p1 = v2(x + w, y);
    Vector2 p2 = v2(x + w, y + h);
    Vector2 p3 = v2(x, y + h);

    immediate_quad(p0, p1, p2, p3, color);
}

void immediate_quad(Vector2 position, Vector2 size, Vector4 color) {
    immediate_quad(position.x, position.y, size.x, size.y, color);
}

void immediate_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color) {
    if (num_immediate_vertices + 6 > MAX_IMMEDIATE_VERTICES) immediate_flush();

    auto v = immediate_vertices + num_immediate_vertices;

    Vector2 uv = v2(0, 0);
    
    put_vertex(&v[0], p0, color, uv);
    put_vertex(&v[1], p1, color, uv);
    put_vertex(&v[2], p2, color, uv);

    num_immediate_vertices += 3;
}

void immediate_circle(Vector2 center, float radius, Vector4 color) {
    const int NUM_TRIANGLES = 100;
    float dtheta = TAU / (float)NUM_TRIANGLES;

    for (int i = 0; i < NUM_TRIANGLES; i++) {
        float theta0 = i * dtheta;
        float theta1 = (i+1) * dtheta;

        Vector2 v0 = get_vec2(theta0);
        Vector2 v1 = get_vec2(theta1);

        Vector2 p0 = center;
        Vector2 p1 = center + radius * v0;
        Vector2 p2 = center + radius * v1;

        immediate_triangle(p0, p1, p2, color);
    }
}

Shader *make_shader() {
    return (Shader *)malloc(sizeof(Shader));
}

void release_shader(Shader *shader) {
    if (shader->program_id) {
        glDeleteProgram(shader->program_id);
    }
}

bool load_shader(Shader *shader, char *file_data, char *filepath) {
    if (!file_data) {
        logprintf("Null file data for '%s'.\n", filepath);
        return false;
    }
    
    char *vertex_source[] = {
        "#version 330 core\n#define VERTEX_SHADER\n#define OUT_IN out\n#line 1 1\n",
        file_data
    };

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    defer { glDeleteShader(v); };
    glShaderSource(v, ArrayCount(vertex_source), vertex_source, NULL);
    glCompileShader(v);
    int success = false;
    glGetShaderiv(v, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[4096];
        glGetShaderInfoLog(v, sizeof(info_log), NULL, info_log);
        logprintf("Failed to compile '%s' vertex shader:\n%s\n", filepath, info_log);
        return false;
    }

    char *fragment_source[] = {
        "#version 330 core\n#define FRAGMENT_SHADER\n#define OUT_IN in\n#line 1 1\n",
        file_data
    };

    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    defer { glDeleteShader(f); };
    glShaderSource(f, ArrayCount(fragment_source), fragment_source, NULL);
    glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[4096];
        glGetShaderInfoLog(f, sizeof(info_log), NULL, info_log);
        logprintf("Failed to compile '%s' fragment shader:\n%s\n", filepath, info_log);
        return false;
    }

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[4096];
        glGetProgramInfoLog(p, sizeof(info_log), NULL, info_log);
        logprintf("Failed to link '%s' shader program:\n%s\n", filepath, info_log);
        glDeleteProgram(p);
        return false;
    }
    glValidateProgram(p);
    glGetProgramiv(p, GL_VALIDATE_STATUS, &success);
    if (!success) {
        char info_log[4096];
        glGetProgramInfoLog(p, sizeof(info_log), NULL, info_log);
        logprintf("Failed to validate '%s' shader program:\n%s\n", filepath, info_log);
        glDeleteProgram(p);
        return false;
    }

    shader->program_id = p;

    shader->object_to_proj_matrix = glGetUniformLocation(p, "object_to_proj_matrix");
    shader->view_to_proj_matrix = glGetUniformLocation(p, "view_to_proj_matrix");
    shader->world_to_view_matrix = glGetUniformLocation(p, "world_to_view_matrix");
    shader->object_to_world_matrix = glGetUniformLocation(p, "object_to_world_matrix");
    
    return true;
}

void set_shader(Shader *shader) {
    if (current_shader == shader) return;
    
    current_shader = shader;
    
    if (shader) {
        glUseProgram(shader->program_id);
    } else {
        glUseProgram(0);
        return;
    }

    refresh_transform();
}

Shader *get_current_shader() {
    return current_shader;
}

static void set_matrix4(GLint loc, Matrix4 m) {
    if (loc == -1) return;
    glUniformMatrix4fv(loc, 1, GL_TRUE, &m._11);
}

void refresh_transform() {
    globals.object_to_proj_matrix = globals.view_to_proj_matrix * (globals.world_to_view_matrix * globals.object_to_world_matrix);
    if (current_shader) {
        set_matrix4(current_shader->object_to_proj_matrix_loc, globals.object_to_proj_matrix);
        set_matrix4(current_shader->view_to_proj_matrix_loc, globals.view_to_proj_matrix);
        set_matrix4(current_shader->world_to_view_matrix_loc, globals.world_to_view_matrix);
        set_matrix4(current_shader->object_to_world_matrix_loc, globals.object_to_world_matrix);
    }
}
