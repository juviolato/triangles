//////////////////////////////////////////////////////////////////////////////
//
//  Triangles.cpp
//
//////////////////////////////////////////////////////////////////////////////

#include <GL3/gl3.h>
#include <GL3/gl3w.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cstdlib>
#include <vector>
#include <iostream>
#include <fstream>

#define MAX_MATERIALS 10
#define BUFFER_OFFSET(a) ((void*)(a))

typedef struct {
    GLenum       type;
    const char* filename;
    GLuint       shader;
} ShaderInfo;

typedef struct {
    glm::vec3 a;
    glm::vec3 b;
    glm::vec3 c;
    glm::vec3 n[3];
    glm::vec3 face_normal;
    unsigned char colors[3][3];
} Triangle;

typedef struct Obj {
    GLfloat* vertices;
    GLfloat* vertex_normals;
    GLfloat* colors;
    int      triangle_count;

    Obj(int vertex_count)
    {
        vertices = new GLfloat[9 * vertex_count];
        vertex_normals = new GLfloat[9 * vertex_count];
        colors = new GLfloat[12 * vertex_count];
    }
} Object;

enum buffer_ids { vertex_buffer = 0, normals_buffer = 1, colors_buffer = 2, n_buffers = 3 };
enum attribute_ids { v_position = 0, v_color = 1, v_normal_vertex = 2 };

GLuint  vao_id;
GLuint  vbos[n_buffers];
GLuint  n_vertices;

int triangle_count;
int material_count;

std::vector<Object*> scene;

static const GLchar* ReadShader(const char* filename)
{
#ifdef WIN32
    FILE* infile;
    fopen_s(&infile, filename, "rb");
#else
    FILE* infile = fopen(filename, "rb");
#endif // WIN32

    if (!infile) {
#ifdef _DEBUG
        std::cerr << "Unable to open file '" << filename << "'" << std::endl;
#endif /* DEBUG */
        return NULL;
    }

    fseek(infile, 0, SEEK_END);
    int len = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    GLchar* source = new GLchar[len + 1];

    fread(source, 1, len, infile);
    fclose(infile);

    source[len] = 0;

    return const_cast<const GLchar*>(source);
}

GLuint LoadShaders(ShaderInfo* shaders)
{
    if (shaders == NULL) { return 0; }

    GLuint program = glCreateProgram();

    ShaderInfo* entry = shaders;
    while (entry->type != GL_NONE) {
        GLuint shader = glCreateShader(entry->type);

        entry->shader = shader;

        const GLchar* source = ReadShader(entry->filename);
        if (source == NULL) {
            for (entry = shaders; entry->type != GL_NONE; ++entry) {
                glDeleteShader(entry->shader);
                entry->shader = 0;
            }

            return 0;
        }

        glShaderSource(shader, 1, &source, NULL);
        delete[] source;

        glCompileShader(shader);

        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLsizei len;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

            GLchar* log = new GLchar[len + 1];
            glGetShaderInfoLog(shader, len, &len, log);
            std::cerr << "Shader compilation failed: " << log << std::endl;
            delete[] log;

            return 0;
        }

        glAttachShader(program, shader);

        ++entry;
    }

    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLsizei len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        GLchar* log = new GLchar[len + 1];
        glGetProgramInfoLog(program, len, &len, log);
        std::cerr << "Shader linking failed: " << log << std::endl;
        delete[] log;

        for (entry = shaders; entry->type != GL_NONE; ++entry) {
            glDeleteShader(entry->shader);
            entry->shader = 0;
        }

        return 0;
    }

    return program;
}

void read_model(const char* filename)
{
    glm::vec3 ambient[MAX_MATERIALS], diffuse[MAX_MATERIALS], specular[MAX_MATERIALS];
    float shine[MAX_MATERIALS];
    int color_index[3];
    char ch;

    FILE* file_ptr = fopen(filename, "r");
    if (file_ptr == nullptr)
        exit(1);

    fscanf(file_ptr, "%c", &ch);
    while (ch != '\n')  // skip first line
        fscanf(file_ptr, "%c", &ch);
    
    fscanf(file_ptr, "# triangles = %d\n", &triangle_count);
    fscanf(file_ptr, "Material count = %d\n", &material_count);

    for (int i = 0; i < material_count; i++)
    {
        fscanf(file_ptr, "ambient color %f %f %f\n", &(ambient[i].x), &(ambient[i].y), &(ambient[i].z));
        fscanf(file_ptr, "diffuse color %f %f %f\n", &(diffuse[i].x), &(diffuse[i].y), &(diffuse[i].z));
        fscanf(file_ptr, "specular color %f %f %f\n", &(specular[i].x), &(specular[i].y), &(specular[i].z));
        fscanf(file_ptr, "material shine %f\n", &(shine[i]));
    }

    fscanf(file_ptr, "%c", &ch);
    while (ch != '\n')  // skip comment
        fscanf(file_ptr, "%c", &ch);

    // allocate triangles
    Triangle* triangles = new Triangle[triangle_count];

    for (int i = 0; i < triangle_count; i++)
    {
        fscanf(file_ptr, "v0 %f %f %f %f %f %f %d\n",
                &(triangles[i].a.x), &(triangles[i].a.y), &(triangles[i].a.z),
                &(triangles[i].n[0].x), &(triangles[i].n[0].y), &(triangles[i].n[0].z),
                &(color_index[0]));
        fscanf(file_ptr, "v1 %f %f %f %f %f %f %d\n",
                &(triangles[i].b.x), &(triangles[i].b.y), &(triangles[i].b.z),
                &(triangles[i].n[1].x), &(triangles[i].n[1].y), &(triangles[i].n[1].z),
                &(color_index[1]));
        fscanf(file_ptr, "v2 %f %f %f %f %f %f %d\n",
                &(triangles[i].c.x), &(triangles[i].c.y), &(triangles[i].c.z),
                &(triangles[i].n[2].x), &(triangles[i].n[2].y), &(triangles[i].n[2].z),
                &(color_index[2]));
        fscanf(file_ptr, "face normal %f %f %f\n",
                &(triangles[i].face_normal.x), &(triangles[i].face_normal.y),
                &(triangles[i].face_normal.z));

        for (int j = 0; j < 3; j++)
        {
            triangles[i].colors[j][0] =
                (unsigned char)(int)(255 * (diffuse[color_index[j]].x));
            triangles[i].colors[j][1] =
                (unsigned char)(int)(255 * (diffuse[color_index[j]].y));
            triangles[i].colors[j][2] =
                (unsigned char)(int)(255 * (diffuse[color_index[j]].z));
            triangles[i].colors[j][3] = 1.0;
        }
    }
    fclose(file_ptr);

    Object* obj = new Object(triangle_count);
    for (int i = 0; i < triangle_count; i++)
    {
        obj->vertices[9*i]       = triangles[i].a.x;
        obj->vertices[9*i + 1]   = triangles[i].a.y;
        obj->vertices[9*i + 2]   = triangles[i].a.z;
        obj->vertices[9*i + 3]   = triangles[i].b.x;
        obj->vertices[9*i + 4]   = triangles[i].b.y;
        obj->vertices[9*i + 5]   = triangles[i].b.z;
        obj->vertices[9*i + 6]   = triangles[i].c.x;
        obj->vertices[9*i + 7]   = triangles[i].c.y;
        obj->vertices[9*i + 8]   = triangles[i].c.z;

        obj->vertex_normals[9*i]     = triangles[i].n[0].x;
        obj->vertex_normals[9*i + 1] = triangles[i].n[0].y;
        obj->vertex_normals[9*i + 2] = triangles[i].n[0].z;
        obj->vertex_normals[9*i + 3] = triangles[i].n[1].x;
        obj->vertex_normals[9*i + 4] = triangles[i].n[1].y;
        obj->vertex_normals[9*i + 5] = triangles[i].n[1].z;
        obj->vertex_normals[9*i + 6] = triangles[i].n[2].x;
        obj->vertex_normals[9*i + 7] = triangles[i].n[2].y;
        obj->vertex_normals[9*i + 8] = triangles[i].n[2].z;

        obj->colors[12*i]     = triangles[i].colors[0][0];
        obj->colors[12*i + 1] = triangles[i].colors[0][1];
        obj->colors[12*i + 2] = triangles[i].colors[0][2];
        obj->colors[12*i + 3] = triangles[i].colors[0][3];
        obj->colors[12*i + 4] = triangles[i].colors[1][0];
        obj->colors[12*i + 5] = triangles[i].colors[1][1];
        obj->colors[12*i + 6] = triangles[i].colors[1][2];
        obj->colors[12*i + 7] = triangles[i].colors[1][3];
        obj->colors[12*i + 8] = triangles[i].colors[2][0];
        obj->colors[12*i + 9] = triangles[i].colors[2][1];
        obj->colors[12*i + 10] = triangles[i].colors[2][2];
        obj->colors[12*i + 11] = triangles[i].colors[2][3];
    }
    obj->triangle_count = triangle_count;
    scene.push_back(obj);
}

int main( int argc, char** argv )
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(800, 600, "CMP143", NULL, NULL);

    glfwMakeContextCurrent(window);
    gl3wInit();

    glGenVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);

    glCreateBuffers(n_buffers, vbos);

    read_model("../cube.in");

    // load vertex coordinates
    glBindBuffer(GL_ARRAY_BUFFER, vbos[vertex_buffer]);
    glBufferStorage(GL_ARRAY_BUFFER, scene[0]->triangle_count*9*sizeof(GL_FLOAT),
                    scene[0]->vertices, GL_DYNAMIC_STORAGE_BIT);

    glVertexAttribPointer(v_position, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(v_position);

    // load normal coordinates
    glBindBuffer(GL_ARRAY_BUFFER, vbos[normals_buffer]);
    glBufferStorage(GL_ARRAY_BUFFER, scene[0]->triangle_count*9*sizeof(GL_FLOAT),
                    scene[0]->vertex_normals, GL_DYNAMIC_STORAGE_BIT);

    glVertexAttribPointer(v_normal_vertex, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(v_normal_vertex);

    // load color information
    glBindBuffer(GL_ARRAY_BUFFER, vbos[colors_buffer]);
    glBufferStorage(GL_ARRAY_BUFFER, scene[0]->triangle_count*12*sizeof(GL_FLOAT),
                    scene[0]->colors, GL_DYNAMIC_STORAGE_BIT);

    glVertexAttribPointer(v_color, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(v_color);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    ShaderInfo  shaders[] =
    {
        { GL_VERTEX_SHADER, "../triangles.vert" },
        { GL_FRAGMENT_SHADER, "../triangles.frag" },
        { GL_NONE, NULL }
    };

    GLuint program = LoadShaders(shaders);
    glUseProgram(program);

    while (!glfwWindowShouldClose(window))
    {
        static const float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };

        glClearBufferfv(GL_COLOR, 0, black);

        glBindVertexArray(vao_id);
        glDrawArrays(GL_TRIANGLES, 0, n_vertices);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
}
