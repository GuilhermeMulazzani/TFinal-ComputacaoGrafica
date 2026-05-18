//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//   INF01047 Computação Gráfica e Visualização I
//               Prof. Eduardo Gastal
//
//     CÓDIGO BASE PARA O TRABALHO FINAL - PROTÓTIPO MINI GOLF
//

#include <cmath>
#include <cstdio>
#include <cstdlib>

// Headers abaixo são específicos de C++
#include <set>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   
#include <GLFW/glfw3.h>  

// Headers da biblioteca GLM
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>
#include <stb_image.h>

// Headers locais
#include "utils.h"
#include "matrices.h"

// Estrutura do modelo OBJ
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i+1);
                basepath = dirname.c_str();
            }
        }

        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

        if (!err.empty()) fprintf(stderr, "\n%s\n", err.c_str());
        if (!ret) throw std::runtime_error("Erro ao carregar modelo.");
        printf("OK.\n");
    }
};

void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);
void BuildTrianglesAndAddToVirtualScene(ObjModel*); 
void ComputeNormals(ObjModel* model); 
void LoadShadersFromFiles(); 
void LoadTextureImage(const char* filename); 
void DrawVirtualObject(const char* object_name); 
GLuint LoadShader_Vertex(const char* filename);   
GLuint LoadShader_Fragment(const char* filename); 
void LoadShader(const char* filename, GLuint shader_id); 
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); 

void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

struct SceneObject
{
    std::string  name;        
    size_t       first_index; 
    size_t       num_indices; 
    GLenum       rendering_mode; 
    GLuint       vertex_array_object_id; 
    glm::vec3    bbox_min; 
    glm::vec3    bbox_max;
};

std::map<std::string, SceneObject> g_VirtualScene;
std::stack<glm::mat4>  g_MatrixStack;
float g_ScreenRatio = 1.0f;

float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

// ====================================================================
// CORREÇÃO ERRO 2: DECLARAÇÃO DAS VARIÁVEIS GLOBAIS DO MOUSE
// ====================================================================
bool g_LeftMouseButtonPressed = false;
double g_LastCursorPosX = 0.0;
double g_LastCursorPosY = 0.0;

float g_CameraTheta = 0.0f; 
float g_CameraPhi = 0.4f;   
float g_CameraDistance = 4.5f; 

// ====================================================================
// VARIÁVEIS MECÂNICAS DO MINI GOLF
// ====================================================================
glm::vec4 g_BallPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
glm::vec4 g_BallVelocity = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

float g_ShotIntensity = 0.0f;
bool g_IsCharging = false;
const float MAX_INTENSITY = 20.0f;
const float CHARGE_SPEED = 12.0f; 

glm::vec4 g_HolePosition = glm::vec4(0.0f, -0.19f, -6.0f, 1.0f);
float g_HoleRadius = 0.35f;
bool g_BallInHole = false;

bool g_UsePerspectiveProjection = true;
bool g_ShowInfoText = true;

GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_object_id_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;
GLuint g_NumLoadedTextures = 0;

int main(int argc, char* argv[])
{
    if (!glfwInit())
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    glfwSetErrorCallback(ErrorCallback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Golf it Again! - Protótipo Parcial", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        std::exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); 

    LoadShadersFromFiles();

    LoadTextureImage("../../data/red_brick_diff_1k.jpg");      
    LoadTextureImage("../../data/rocky_terrain_02_diff_1k.jpg"); 

    ObjModel spheremodel("../../data/sphere.obj");
    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    ObjModel planemodel("../../data/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    TextRendering_Init();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrameTime = (float)glfwGetTime();
        static float lastFrameTime = 0.0f;
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        if (!g_BallInHole && glm::length(g_BallVelocity) == 0.0f) 
        {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) 
            {
                g_IsCharging = true;
                g_ShotIntensity += CHARGE_SPEED * deltaTime;
                if (g_ShotIntensity > MAX_INTENSITY) g_ShotIntensity = MAX_INTENSITY;
            } 
            else 
            {
                if (g_IsCharging) 
                {
                    float dir_x = -sin(g_CameraTheta);
                    float dir_z = -cos(g_CameraTheta);
                    g_BallVelocity = glm::vec4(dir_x, 0.0f, dir_z, 0.0f) * g_ShotIntensity;
                    g_ShotIntensity = 0.0f;
                    g_IsCharging = false;
                }
            }
        }

        if (!g_BallInHole) 
        {
            g_BallVelocity -= g_BallVelocity * 1.3f * deltaTime; 
            if (glm::length(g_BallVelocity) < 0.07f) g_BallVelocity = glm::vec4(0.0f);
            g_BallPosition += g_BallVelocity * deltaTime;

            if (g_BallPosition.x < -1.85f) { g_BallPosition.x = -1.85f; g_BallVelocity.x *= -1.0f; }
            if (g_BallPosition.x >  1.85f) { g_BallPosition.x =  1.85f; g_BallVelocity.x *= -1.0f; }
            if (g_BallPosition.z < -7.85f) { g_BallPosition.z = -7.85f; g_BallVelocity.z *= -1.0f; }
            if (g_BallPosition.z >  1.85f) { g_BallPosition.z =  1.85f; g_BallVelocity.z *= -1.0f; }

            float distToHole = glm::distance(glm::vec3(g_BallPosition), glm::vec3(g_HolePosition));
            if (distToHole < g_HoleRadius) 
            {
                g_BallInHole = true;
                g_BallVelocity = glm::vec4(0.0f);
                g_BallPosition = g_HolePosition; 
                g_BallPosition.y = -0.4f; 
            }
        }

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) 
        {
            g_BallPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            g_BallVelocity = glm::vec4(0.0f);
            g_BallInHole = false;
        }

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(g_GpuProgramID);

        float r = g_CameraDistance;
        float y = r*sin(g_CameraPhi);
        float z = r*cos(g_CameraPhi)*cos(g_CameraTheta);
        float x = r*cos(g_CameraPhi)*sin(g_CameraTheta);

        glm::vec4 camera_lookat_l  = g_BallPosition; 
        if(g_BallInHole) camera_lookat_l.y = -0.2f;

        glm::vec4 camera_position_c  = g_BallPosition + glm::vec4(x,y,z,0.0f); 
        glm::vec4 camera_view_vector = camera_lookat_l - camera_position_c; 
        glm::vec4 camera_up_vector   = glm::vec4(0.0f,1.0f,0.0f,0.0f); 

        glm::mat4 view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);
        glm::mat4 projection = Matrix_Perspective(3.141592 / 3.0f, g_ScreenRatio, -0.1f, -50.0f);

        glUniformMatrix4fv(g_view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        #define SPHERE 0
        #define PLANE  2

        // ====================================================================
        // CORREÇÃO ERRO 1: ADICIONADO DECLARAÇÃO DE glm::mat4 ANTES DE model
        // ====================================================================
        
        // 1. O Chão da pista
        glm::mat4 model = Matrix_Translate(0.0f, -0.2f, -3.0f) * Matrix_Scale(2.0f, 1.0f, 5.0f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE);
        DrawVirtualObject("the_plane");

        // 2. As Paredes Visuais
        // Parede Esquerda
        model = Matrix_Translate(-2.0f, 0.1f, -3.0f) * Matrix_Rotate_Z(1.5708f) * Matrix_Scale(0.3f, 1.0f, 5.0f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");

        // Parede Direita
        model = Matrix_Translate(2.0f, 0.1f, -3.0f) * Matrix_Rotate_Z(1.5708f) * Matrix_Scale(0.3f, 1.0f, 5.0f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");

        // Parede do Fundo
        model = Matrix_Translate(0.0f, 0.1f, -8.0f) * Matrix_Rotate_X(1.5708f) * Matrix_Scale(2.0f, 1.0f, 0.3f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");

        // Parede Inicial
        model = Matrix_Translate(0.0f, 0.1f, 2.0f) * Matrix_Rotate_X(1.5708f) * Matrix_Scale(2.0f, 1.0f, 0.3f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");

        // 3. O Buraco
        model = Matrix_Translate(g_HolePosition.x, g_HolePosition.y, g_HolePosition.z) * Matrix_Scale(g_HoleRadius, 0.01f, g_HoleRadius);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, SPHERE);
        DrawVirtualObject("the_sphere");

        // 4. A Bola de Golf
        model = Matrix_Translate(g_BallPosition.x, g_BallPosition.y, g_BallPosition.z) * Matrix_Scale(0.18f, 0.18f, 0.18f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, SPHERE);
        DrawVirtualObject("the_sphere");

        // 5. O Taco (Animação de recuo)
        if (!g_BallInHole && glm::length(g_BallVelocity) == 0.0f) 
        {
            float back_x = sin(g_CameraTheta);
            float back_z = cos(g_CameraTheta);
            float recuo = 0.35f + (g_ShotIntensity / MAX_INTENSITY) * 0.8f; 
            
            glm::vec4 clubPos = g_BallPosition + glm::vec4(back_x, 0.0f, back_z, 0.0f) * recuo;
            clubPos.y = 0.2f; 

            model = Matrix_Translate(clubPos.x, clubPos.y, clubPos.z) * Matrix_Scale(0.05f, 0.5f, 0.05f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, SPHERE);
            DrawVirtualObject("the_sphere");
        }

        if (g_IsCharging) {
            char txt[30];
            snprintf(txt, 30, "FORCA: %.1f", g_ShotIntensity);
            TextRendering_PrintString(window, txt, -0.9f, -0.9f, 1.0f);
        }
        if (g_BallInHole) {
            TextRendering_PrintString(window, "CONCLUIDO! APERTE R PARA RESETAR", -0.4f, 0.0f, 1.0f);
        }

        TextRendering_ShowFramesPerSecond(window);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void LoadTextureImage(const char* filename) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);
    if ( data == NULL ) std::exit(EXIT_FAILURE);
    GLuint texture_id; GLuint sampler_id;
    glGenTextures(1, &texture_id); glGenSamplers(1, &sampler_id);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit); glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D); glBindSampler(textureunit, sampler_id);
    stbi_image_free(data); g_NumLoadedTextures += 1;
}

void DrawVirtualObject(const char* object_name) {
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);
    glUniform4f(g_bbox_min_uniform, g_VirtualScene[object_name].bbox_min.x, g_VirtualScene[object_name].bbox_min.y, g_VirtualScene[object_name].bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, g_VirtualScene[object_name].bbox_max.x, g_VirtualScene[object_name].bbox_max.y, g_VirtualScene[object_name].bbox_max.z, 1.0f);
    glDrawElements(g_VirtualScene[object_name].rendering_mode, g_VirtualScene[object_name].num_indices, GL_UNSIGNED_INT, (void*)(g_VirtualScene[object_name].first_index * sizeof(GLuint)));
    glBindVertexArray(0);
}

void LoadShadersFromFiles() {
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");
    if ( g_GpuProgramID != 0 ) glDeleteProgram(g_GpuProgramID);
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);
    g_model_uniform      = glGetUniformLocation(g_GpuProgramID, "model");
    g_view_uniform       = glGetUniformLocation(g_GpuProgramID, "view"); 
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); 
    g_object_id_uniform  = glGetUniformLocation(g_GpuProgramID, "object_id"); 
    g_bbox_min_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2);
    glUseProgram(0);
}

void PushMatrix(glm::mat4 M) { g_MatrixStack.push(M); }
void PopMatrix(glm::mat4& M) { if ( g_MatrixStack.empty() ) M = Matrix_Identity(); else { M = g_MatrixStack.top(); g_MatrixStack.pop(); } }

void ComputeNormals(ObjModel* model) {
    if ( !model->attrib.normals.empty() ) return;
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
        for (size_t triangle = 0; triangle < model->shapes[shape].mesh.num_face_vertices.size(); ++triangle) {
            sgroup_ids.insert(model->shapes[shape].mesh.smoothing_group_ids[triangle]);
        }
    }
    size_t num_vertices = model->attrib.vertices.size() / 3; model->attrib.normals.reserve( 3*num_vertices );
    for (const unsigned int & sgroup : sgroup_ids) {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0); std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f));
        for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
            for (size_t triangle = 0; triangle < model->shapes[shape].mesh.num_face_vertices.size(); ++triangle) {
                if (model->shapes[shape].mesh.smoothing_group_ids[triangle] != sgroup) continue;
                glm::vec4 v[3];
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    v[vertex] = glm::vec4(model->attrib.vertices[3*idx.vertex_index + 0], model->attrib.vertices[3*idx.vertex_index + 1], model->attrib.vertices[3*idx.vertex_index + 2], 1.0);
                }
                glm::vec4 n = crossproduct(v[1]-v[0], v[2]-v[0]);
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1; vertex_normals[idx.vertex_index] += n;
                }
            }
        }
        std::vector<size_t> normal_indices(num_vertices, 0);
        for (size_t vertex_index = 0; vertex_index < vertex_normals.size(); ++vertex_index) {
            if (num_triangles_per_vertex[vertex_index] == 0) continue;
            glm::vec4 n = vertex_normals[vertex_index] / (float)num_triangles_per_vertex[vertex_index]; n /= norm(n);
            model->attrib.normals.push_back( n.x ); model->attrib.normals.push_back( n.y ); model->attrib.normals.push_back( n.z );
            normal_indices[vertex_index] = (model->attrib.normals.size() / 3) - 1;
        }
        for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
            for (size_t triangle = 0; triangle < model->shapes[shape].mesh.num_face_vertices.size(); ++triangle) {
                if (model->shapes[shape].mesh.smoothing_group_ids[triangle] != sgroup) continue;
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index = normal_indices[ idx.vertex_index ];
                }
            }
        }
    }
}

void BuildTrianglesAndAddToVirtualScene(ObjModel* model) {
    GLuint vertex_array_object_id; glGenVertexArrays(1, &vertex_array_object_id); glBindVertexArray(vertex_array_object_id);
    std::vector<GLuint> indices; std::vector<float> model_coefficients, normal_coefficients, texture_coefficients;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
        size_t first_index = indices.size();
        for (size_t triangle = 0; triangle < model->shapes[shape].mesh.num_face_vertices.size(); ++triangle) {
            for (size_t vertex = 0; vertex < 3; ++vertex) {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex]; indices.push_back(first_index + 3*triangle + vertex);
                float vx = model->attrib.vertices[3*idx.vertex_index + 0]; float vy = model->attrib.vertices[3*idx.vertex_index + 1]; float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                model_coefficients.push_back(vx); model_coefficients.push_back(vy); model_coefficients.push_back(vz); model_coefficients.push_back(1.0f);
                if ( idx.normal_index != -1 ) { normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 0]); normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 1]); normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 2]); normal_coefficients.push_back(0.0f); }
                if ( idx.texcoord_index != -1 ) { texture_coefficients.push_back(model->attrib.texcoords[2*idx.texcoord_index + 0]); texture_coefficients.push_back(model->attrib.texcoords[2*idx.texcoord_index + 1]); }
            }
        }
        SceneObject theobject; theobject.name = model->shapes[shape].name; theobject.first_index = first_index; theobject.num_indices = indices.size() - first_index; theobject.rendering_mode = GL_TRIANGLES; theobject.vertex_array_object_id = vertex_array_object_id; theobject.bbox_min = glm::vec3(-1.0f); theobject.bbox_max = glm::vec3(1.0f); g_VirtualScene[model->shapes[shape].name] = theobject;
    }
    GLuint VBO; glGenBuffers(1, &VBO); glBindBuffer(GL_ARRAY_BUFFER, VBO); glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), model_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(0);
    if ( !normal_coefficients.empty() ) { glGenBuffers(1, &VBO); glBindBuffer(GL_ARRAY_BUFFER, VBO); glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), normal_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(1); }
    if ( !texture_coefficients.empty() ) { glGenBuffers(1, &VBO); glBindBuffer(GL_ARRAY_BUFFER, VBO); glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), texture_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(2); }
    GLuint indices_id; glGenBuffers(1, &indices_id); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id); glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW); glBindVertexArray(0);
}

void LoadShader(const char* filename, GLuint shader_id) {
    std::ifstream file; try { file.exceptions(std::ifstream::failbit); file.open(filename); } catch ( std::exception& e ) { std::exit(EXIT_FAILURE); }
    std::stringstream shader; shader << file.rdbuf(); std::string str = shader.str(); const GLchar* shader_string = str.c_str(); const GLint length = static_cast<GLint>( str.length() ); glShaderSource(shader_id, 1, &shader_string, &length); glCompileShader(shader_id);
}
GLuint LoadShader_Vertex(const char* filename) { GLuint id = glCreateShader(GL_VERTEX_SHADER); LoadShader(filename, id); return id; }
GLuint LoadShader_Fragment(const char* filename) { GLuint id = glCreateShader(GL_FRAGMENT_SHADER); LoadShader(filename, id); return id; }
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id) { GLuint program_id = glCreateProgram(); glAttachShader(program_id, vertex_shader_id); glAttachShader(program_id, fragment_shader_id); glLinkProgram(program_id); glDeleteShader(vertex_shader_id); glDeleteShader(fragment_shader_id); return program_id; }
void FramebufferSizeCallback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); g_ScreenRatio = (float)width / height; }
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) { if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) { glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY); g_LeftMouseButtonPressed = true; } if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) g_LeftMouseButtonPressed = false; }
void ErrorCallback(int error, const char* description) {}
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) { g_CameraDistance -= 0.1f*yoffset; if (g_CameraDistance < 0.1f) g_CameraDistance = 0.1f; }
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode) { if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GL_TRUE); }
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) { if (g_LeftMouseButtonPressed) { g_CameraTheta -= 0.01f * (xpos - g_LastCursorPosX); g_CameraPhi += 0.01f * (ypos - g_LastCursorPosY); float phimax = 3.141592f/2; if (g_CameraPhi > phimax) g_CameraPhi = phimax; if (g_CameraPhi < -phimax) g_CameraPhi = -phimax; g_LastCursorPosX = xpos; g_LastCursorPosY = ypos; } }
