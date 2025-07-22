#include "main.h"

#ifdef RENDER_OPENGL

#include "rendering.h"
#include "glex.h"

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

static Window_Type window;
static Shader *current_shader;

const int MAX_IMMEDIATE_VERTICES = 2400;
static Immediate_Vertex immediate_vertices[MAX_IMMEDIATE_VERTICES];
static int num_immediate_vertices;

static GLuint immediate_vbo;

bool init_rendering(Window_Type _window, bool vsync) {
    window = _window;
    
    if (!os_create_opengl_context(window, 3, 3, true)) return false;
    if (os_opengl_set_vsync(vsync)) {
        if (vsync) {
            logprintf("vsync: on\n");
        } else {
            logprintf("vsync: off\n");
        }
    } else {
        logprintf("Couldn't set vsync");
    }
    init_gl_extensions();

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
    os_opengl_swap_buffers(window);
}

void set_viewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
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

Shader *make_shader() {
    return (Shader *)malloc(sizeof(Shader));
}

bool load_shader(Shader *shader, char *filepath) {
    char *file_data = read_entire_file(filepath);
    if (!file_data) {
        logprintf("Failed to read file '%s'.\n", filepath);
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

#define GUL(name) shader->##name##_loc = glGetUniformLocation(p, #name)

    GUL(object_to_proj_matrix);
    GUL(view_to_proj_matrix);
    GUL(world_to_view_matrix);
    GUL(object_to_world_matrix);
    
#undef GUL
    
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

#endif
