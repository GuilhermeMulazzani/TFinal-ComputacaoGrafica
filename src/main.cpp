//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//   INF01047 Computação Gráfica e Visualização I
//             Prof. Eduardo Gastal
//
//     CÓDIGO FINAL - MINI GOLF ZIG-ZAG (ANIMAÇÃO REALISTA DA BANDEIRA)

#include <cmath>
#include <cstdio>
#include <cstdlib>
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

#include <glad/glad.h>   
#include <GLFW/glfw3.h>  
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp> 

#include <tiny_obj_loader.h>
#include <stb_image.h>

#include "utils.h"
#include "matrices.h"

// Variável externa do framework da UFRGS para controlar a escala real do texto
extern float textscale;

// Função robusta para rotacionar a bola em qualquer eixo 
glm::mat4 Matrix_Rotate_Axis(float angle, glm::vec3 axis) {
    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0f - c;
    
    // Normalização puramente matemática, sem depender de glm::normalize 
    // ou dotproduct() para evitar o erro "Produto escalar não definido para pontos"
    float magnitude = sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    
    // Prevenção contra divisão por zero
    if (magnitude < 0.000001f) {
        return glm::mat4(1.0f); 
    }
    
    float x = axis.x / magnitude;
    float y = axis.y / magnitude;
    float z = axis.z / magnitude;

    return glm::mat4(
        t*x*x + c,   t*x*y + s*z, t*x*z - s*y, 0.0f,
        t*x*y - s*z, t*y*y + c,   t*y*z + s*x, 0.0f,
        t*x*z + s*y, t*y*z - s*x, t*z*z + c,   0.0f,
        0.0f,        0.0f,        0.0f,        1.0f
    );
}

struct ObjModel {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;

    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true) {
        printf("A carregar objetos do ficheiro \"%s\"...\n", filename);
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL) {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos) {
                dirname = fullpath.substr(0, i+1);
                basepath = dirname.c_str();
            }
        }
        std::string warn, err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);
        if (!err.empty()) fprintf(stderr, "\n%s\n", err.c_str());
        if (!ret) throw std::runtime_error("Erro ao carregar o modelo.");
        for (size_t shape = 0; shape < shapes.size(); ++shape) {
            if (shapes[shape].name.empty()) throw std::runtime_error("Objeto sem nome.");
            printf("- Objeto '%s'\n", shapes[shape].name.c_str());
        }
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

struct SceneObject {
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

bool g_LeftMouseButtonPressed = false;
double g_LastCursorPosX = 0.0;
double g_LastCursorPosY = 0.0;

float g_CameraTheta = 0.0f; 
float g_CameraPhi = 0.4f;   
float g_CameraDistance = 1.5f; 

// ====================================================================
// FÍSICA E PISTA (ZIG-ZAG)
// ====================================================================
glm::vec4 g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f); 
glm::vec4 g_BallVelocity = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
float g_BallRadius = 0.08f; 

glm::mat4 g_BallRotationMatrix = glm::mat4(1.0f);

float g_ShotIntensity = 0.0f;
bool g_IsCharging = false;
const float MAX_INTENSITY = 20.0f;
const float CHARGE_SPEED = 12.0f; 
int g_Strokes = 0;

bool g_IsSwinging = false;
float g_StoredIntensity = 0.0f;

glm::vec4 g_HolePosition = glm::vec4(-4.0f, -0.19f, -17.5f, 1.0f);
float g_HoleRadius = 0.10f;
bool g_BallInHole = false;

// ====================================================================
// VARIÁVEL EXCLUSIVA PARA A ALTURA DO TECIDO DA BANDEIRA
// ====================================================================
float g_FlagHeightOffset = -0.4f; 

struct Wall { glm::vec2 p1, p2; };

std::vector<Wall> g_Walls = {
    {{ 1.5f,  1.0f}, { 1.5f, -4.0f}}, 
    {{ 1.5f, -4.0f}, {-0.5f, -7.0f}}, 
    {{-0.5f, -7.0f}, {-0.5f,-11.0f}}, 
    {{-0.5f,-11.0f}, {-2.5f,-14.0f}}, 
    {{-2.5f,-14.0f}, {-2.5f,-19.0f}}, 

    {{-1.5f, -4.0f}, {-1.5f,  1.0f}}, 
    {{-3.5f, -7.0f}, {-1.5f, -4.0f}}, 
    {{-3.5f,-11.0f}, {-3.5f, -7.0f}}, 
    {{-5.5f,-14.0f}, {-3.5f,-11.0f}}, 
    {{-5.5f,-19.0f}, {-5.5f,-14.0f}}, 
    
    {{ 1.5f,  1.0f}, {-1.5f,  1.0f}}, 
    {{-2.5f,-19.0f}, {-5.5f,-19.0f}},

    {{ 1.5f, -2.0f}, { 0.0f, -2.0f}}, // Barreira na primeira reta (bloqueia o lado direito)
    {{-3.5f, -9.0f}, {-1.5f, -9.0f}}
};

GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_object_id_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;
GLuint g_NumLoadedTextures = 0;
GLint g_ball_position_uniform;

int main(int argc, char* argv[])
{
    if (!glfwInit()) std::exit(EXIT_FAILURE);

    glfwSetErrorCallback(ErrorCallback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Golf it Again! - Guilherme M Mulazzani", NULL, NULL);
    if (!window) { glfwTerminate(); std::exit(EXIT_FAILURE); }

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); 

    LoadShadersFromFiles();

    // Texturas
    LoadTextureImage("../../data/madeira_textures/textures/oak_veneer_01_diff_4k.jpg");         
    LoadTextureImage("../../data/quadriculado_chao/textures/floor_tiles_06_diff_4k.jpg");            
    LoadTextureImage("../../data/golf_ball/textures/textura-golf.jpg");    
    LoadTextureImage("../../data/taco_golf/thumbnail.jpg");    
    LoadTextureImage("../../data/rocky_terrain_02_diff_1k.jpg");                                
    LoadTextureImage("../../data/textura_bandeira.jpeg"); 
    LoadTextureImage("../../data/textura_metal.jpg");                                            

    ObjModel planemodel("../../data/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    ObjModel spheremodel("../../data/sphere.obj");
    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    ObjModel golfClubModel("../../data/taco_golf/model.obj");
    ComputeNormals(&golfClubModel);
    BuildTrianglesAndAddToVirtualScene(&golfClubModel);

    ObjModel golfBallModel("../../data/golf_ball/golf_ball.obj");
    ComputeNormals(&golfBallModel);
    BuildTrianglesAndAddToVirtualScene(&golfBallModel);

    ObjModel flagModel("../../data/bandeira_brasil.obj");
    ComputeNormals(&flagModel);
    BuildTrianglesAndAddToVirtualScene(&flagModel);

    ObjModel blockModel("../../data/cube-parede/cube.obj"); // Ajuste o caminho/nome do arquivo se necessário
    ComputeNormals(&blockModel);
    BuildTrianglesAndAddToVirtualScene(&blockModel);
    std::string cubeObjName = blockModel.shapes[0].name;

    TextRendering_Init();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    float flag_scale = 1.0f;
    glm::vec3 flag_min(1e9), flag_max(-1e9);
    for(auto& shape : flagModel.shapes) {
        if(g_VirtualScene.count(shape.name)) {
            flag_min = glm::min(flag_min, g_VirtualScene[shape.name].bbox_min);
            flag_max = glm::max(flag_max, g_VirtualScene[shape.name].bbox_max);
        }
    }
    float f_sz = std::max({flag_max.x - flag_min.x, flag_max.y - flag_min.y, flag_max.z - flag_min.z});
    
    if(f_sz > 0) flag_scale = 1.0f / f_sz; 
    glm::vec3 flag_center = (flag_min + flag_max) / 2.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrameTime = (float)glfwGetTime();
        static float lastFrameTime = 0.0f;
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        if (!g_BallInHole && glm::length(g_BallVelocity) == 0.0f && !g_IsSwinging) {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                g_IsCharging = true;
                g_ShotIntensity += CHARGE_SPEED * deltaTime;
                if (g_ShotIntensity > MAX_INTENSITY) g_ShotIntensity = MAX_INTENSITY;
            } else {
                if (g_IsCharging) {
                    g_IsSwinging = true;
                    g_StoredIntensity = g_ShotIntensity;
                    g_IsCharging = false;
                }
            }
        }

        if (g_IsSwinging) {
            const float SWING_SPEED = 140.0f; 
            g_ShotIntensity -= SWING_SPEED * deltaTime;
            
            if (g_ShotIntensity <= 0.0f) {
                g_ShotIntensity = 0.0f;
                g_IsSwinging = false;
                
                float dir_x = -sin(g_CameraTheta);
                float dir_z = -cos(g_CameraTheta);
                g_BallVelocity = glm::vec4(dir_x, 0.0f, dir_z, 0.0f) * g_StoredIntensity;
                g_Strokes++; 
            }
        }

        if (!g_BallInHole) {
            g_BallVelocity -= g_BallVelocity * 1.0f * deltaTime; 
            if (glm::length(g_BallVelocity) < 0.07f) g_BallVelocity = glm::vec4(0.0f);
            
            glm::vec4 nextPos = g_BallPosition + g_BallVelocity * deltaTime;

            for (auto& w : g_Walls) {
                glm::vec2 ab = w.p2 - w.p1;
                glm::vec2 ac = glm::vec2(nextPos.x, nextPos.z) - w.p1;
                float t = glm::dot(ac, ab) / glm::dot(ab, ab);
                t = glm::clamp(t, 0.0f, 1.0f);
                glm::vec2 closest = w.p1 + t * ab;
                glm::vec2 diff = glm::vec2(nextPos.x, nextPos.z) - closest;
                float dist = glm::length(diff);
                
                if (dist < g_BallRadius) {
                    glm::vec2 n = diff / dist; 
                    nextPos.x += n.x * (g_BallRadius - dist);
                    nextPos.z += n.y * (g_BallRadius - dist);
                    
                    glm::vec2 v(g_BallVelocity.x, g_BallVelocity.z);
                    float v_dot_n = glm::dot(v, n);
                    if (v_dot_n < 0.0f) {
                        v = v - 1.8f * v_dot_n * n; 
                        g_BallVelocity.x = v.x;
                        g_BallVelocity.z = v.y;
                    }
                }
            }
            
            glm::vec3 movement(nextPos.x - g_BallPosition.x, 0.0f, nextPos.z - g_BallPosition.z);
            float dist = glm::length(movement);
            if (dist > 0.0001f) {
                glm::vec3 move_dir = movement / dist;
                glm::vec3 roll_axis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), move_dir);
                float angle = dist / g_BallRadius; 
                g_BallRotationMatrix = Matrix_Rotate_Axis(angle, roll_axis) * g_BallRotationMatrix;
            }

            g_BallPosition = nextPos;

            float distToHole = glm::distance(glm::vec2(g_BallPosition.x, g_BallPosition.z), glm::vec2(g_HolePosition.x, g_HolePosition.z));
            float speed = glm::length(g_BallVelocity);
            
            if (distToHole < g_HoleRadius * 0.4f && speed < 3.0f) {
                g_BallInHole = true; 
            } else if (distToHole < g_HoleRadius && speed < 0.8f) {
                g_BallVelocity -= g_BallVelocity * 3.0f * deltaTime; 
            }
            
        } else {
            g_BallPosition.y -= 1.0f * deltaTime; 
            g_BallVelocity = glm::vec4(0.0f);     
            if (g_BallPosition.y < -0.3f) {       
                g_BallPosition.y = -0.3f;
            }
            if (g_FlagHeightOffset < 0.0f) {
                g_FlagHeightOffset += 0.5f * deltaTime; 
            }
        }

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f); 
            g_BallVelocity = glm::vec4(0.0f);
            g_BallInHole = false;
            g_Strokes = 0; 
            g_FlagHeightOffset = -0.4f; 
            g_IsSwinging = false;
            g_ShotIntensity = 0.0f;
            g_BallRotationMatrix = glm::mat4(1.0f); 
        }

        float ideal_r = g_CameraDistance;
        float cam_y = ideal_r * sin(g_CameraPhi);
        float cam_z = ideal_r * cos(g_CameraPhi) * cos(g_CameraTheta);
        float cam_x = ideal_r * cos(g_CameraPhi) * sin(g_CameraTheta);

        glm::vec4 camera_lookat  = g_BallPosition; 
        if(g_BallInHole) camera_lookat.y = -0.2f;

        glm::vec4 ideal_cam_pos = g_BallPosition + glm::vec4(cam_x, cam_y, cam_z, 0.0f);
        
        float max_dist = ideal_r;
        glm::vec2 ray_origin(g_BallPosition.x, g_BallPosition.z);
        glm::vec2 ray_dir(ideal_cam_pos.x - g_BallPosition.x, ideal_cam_pos.z - g_BallPosition.z);
        
        float ray_len = glm::length(ray_dir);
        if (ray_len > 0.001f) {
            ray_dir /= ray_len; 
            for (auto& w : g_Walls) {
                glm::vec2 v1 = ray_origin - w.p1;
                glm::vec2 v2 = w.p2 - w.p1;
                glm::vec2 v3 = glm::vec2(-ray_dir.y, ray_dir.x);
                float dot_val = glm::dot(v2, v3);
                if (std::abs(dot_val) < 0.00001f) continue;
                
                float t1 = (v2.x * v1.y - v2.y * v1.x) / dot_val;
                float t2 = glm::dot(v1, v3) / dot_val;
                
                if (t1 >= 0.0f && t1 <= max_dist && t2 >= 0.0f && t2 <= 1.0f) {
                    
                    // NOVA LÓGICA: Calcula o lado em que a câmara bateu
                    glm::vec2 wall_dir = glm::normalize(v2);
                    glm::vec2 outward_normal = glm::vec2(-wall_dir.y, wall_dir.x);
                    
                    float approach_angle = glm::dot(ray_dir, outward_normal);
                    float margin = 0.05f; // Margem padrão pequena
                    
                    // Se a câmara bateu pelas "costas" do bloco (lado que tem a espessura visual de 0.30f)
                    if (approach_angle < 0.0f) {
                        // Aumentamos a margem matematicamente para compensar o bloco 3D
                        margin += 0.30f / std::max(0.2f, std::abs(approach_angle));
                    }
                    
                    // Aplica a margem de segurança ao ponto de paragem
                    if (t1 - margin < max_dist) {
                        max_dist = t1 - margin; 
                    }
                }
            }
        }
        
        // Garante limite mínimo para não entrar dentro da bola
        float actual_dist = std::max(0.12f, max_dist); 

        glm::vec4 final_cam_pos = g_BallPosition;
        final_cam_pos.x += ray_dir.x * actual_dist;
        final_cam_pos.y += std::max(0.10f, cam_y * (actual_dist / ideal_r)); 
        final_cam_pos.z += ray_dir.y * actual_dist;

        glm::mat4 view = Matrix_Camera_View(final_cam_pos, camera_lookat - final_cam_pos, glm::vec4(0.0f,1.0f,0.0f,0.0f));
        glm::mat4 projection = Matrix_Perspective(3.141592f / 3.0f, g_ScreenRatio, -0.1f, -50.0f);

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(g_GpuProgramID);

        glUniform4fv(g_ball_position_uniform, 1, glm::value_ptr(g_BallPosition));

        glUniformMatrix4fv(g_view_uniform, 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform, 1 , GL_FALSE , glm::value_ptr(projection));

        #define WALL  0
        #define FLOOR 1
        #define BALL  2
        #define CLUB  3
        #define HOLE  4          
        #define FLAG_FABRIC 5    
        #define FLAG_POLE   6

        // ====================================================================
        // RENDERIZAÇÃO DO CHÃO (FLOOR)
        // ====================================================================
        glUniform1i(g_object_id_uniform, FLOOR);
        glm::mat4 model;
        
        // FIX: Cada placa ganha um desnível de 1 milímetro (Y = -0.200, -0.201...) 
        // para a placa de vídeo não confundir as camadas (Z-fighting).
        // FIX: Aumentamos a escala X para 1.6f, garantindo que o chão passe por debaixo da parede.
        model = Matrix_Translate(0.0f, -0.200f, -1.5f) * Matrix_Scale(1.6f, 1.0f, 2.7f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");
        
        model = Matrix_Translate(-1.0f, -0.201f, -5.5f) * Matrix_Rotate_Y(0.588f) * Matrix_Scale(1.6f, 1.0f, 2.2f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");
        
        model = Matrix_Translate(-2.0f, -0.202f, -9.0f) * Matrix_Scale(1.6f, 1.0f, 2.2f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");
        
        model = Matrix_Translate(-3.0f, -0.203f, -12.5f) * Matrix_Rotate_Y(0.588f) * Matrix_Scale(1.6f, 1.0f, 2.2f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");
        
        model = Matrix_Translate(-4.0f, -0.204f, -16.5f) * Matrix_Scale(1.6f, 1.0f, 2.7f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("the_plane");

        // ====================================================================
        // RENDERIZAÇÃO DAS PAREDES (WALL) COMO BLOCOS
        // ====================================================================
        glUniform1i(g_object_id_uniform, WALL);
        glDisable(GL_CULL_FACE); 

        glm::vec3 cube_pivot(0.0f);
        glm::vec3 cube_size(1.0f);
        
        // Usa a variável em vez do texto fixo
        if(g_VirtualScene.count(cubeObjName)) {
            glm::vec3 b_min = g_VirtualScene[cubeObjName].bbox_min;
            glm::vec3 b_max = g_VirtualScene[cubeObjName].bbox_max;
            cube_pivot = (b_min + b_max) / 2.0f;
            cube_size = b_max - b_min;
            
            if(cube_size.x == 0) cube_size.x = 1.0f;
            if(cube_size.y == 0) cube_size.y = 1.0f;
            if(cube_size.z == 0) cube_size.z = 1.0f;
        }

        int wall_index = 0;
        for (auto& w : g_Walls) {
            wall_index++;
            float wall_len = glm::distance(w.p1, w.p2);
            float cx = (w.p1.x + w.p2.x) * 0.5f;
            float cz = (w.p1.y + w.p2.y) * 0.5f; 
            float angle = atan2(w.p1.x - w.p2.x, w.p1.y - w.p2.y);

            glm::vec2 dir = glm::normalize(w.p2 - w.p1);
            glm::vec2 outward_normal = glm::vec2(-dir.y, dir.x); 
            
            // TRUQUE ANTI-Z-FIGHTING: Encolhe paredes pares em 5mm. 
            // Imperceptível ao olho, mas impede que as texturas das quinas briguem e pisquem.
            float shrink = (wall_index % 2 == 0) ? 0.005f : 0.0f; 
            float height = 0.30f - shrink;    
            float thickness = 0.30f - shrink; 

            // Mantemos o alinhamento com a física intacto
            float base_y = -0.22f; 
            float center_x = cx + outward_normal.x * (0.30f * 0.5f);
            float center_z = cz + outward_normal.y * (0.30f * 0.5f);
            float center_y = base_y + (height * 0.5f);

            // Comprimento final: Apenas metade da espessura para preencher a quina sem vazar
            float final_len = wall_len + (0.30f * 0.5f);

            model = Matrix_Translate(center_x, center_y, center_z) 
                  * Matrix_Rotate_Y(angle)
                  * Matrix_Scale(thickness / cube_size.x, height / cube_size.y, final_len / cube_size.z)
                  * Matrix_Translate(-cube_pivot.x, -cube_pivot.y, -cube_pivot.z);
                  
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            DrawVirtualObject(cubeObjName.c_str()); 
        }
        
        glEnable(GL_CULL_FACE);

        // ====================================================================
        // RENDERIZAÇÃO DO BURACO (Abaulado e cortado no shader)
        // ====================================================================
        glUniform1i(g_object_id_uniform, HOLE);
        model = Matrix_Translate(g_HolePosition.x, g_HolePosition.y - 0.25f, g_HolePosition.z) 
              * Matrix_Scale(g_HoleRadius, 0.3f, g_HoleRadius);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("Cylinder001");

        // ====================================================================
        // RENDERIZAÇÃO DA BANDEIRA
        // ====================================================================
        // Altera para subir ou afundar o mastro inteiro (aviso: altura_base_mastro)
        float altura_base_mastro = g_HolePosition.y + 0.0f; 

        for(auto& shape : flagModel.shapes) {
            glm::mat4 model_flag;
            
            if (shape.name == "object_1") {
                glUniform1i(g_object_id_uniform, FLAG_FABRIC); 
            } else {
                glUniform1i(g_object_id_uniform, FLAG_POLE); 
            }

            if (shape.name == "object_3" || shape.name == "object_6") {
                model_flag = Matrix_Translate(g_HolePosition.x, altura_base_mastro, g_HolePosition.z) 
                           * Matrix_Rotate_X(-1.5708f) 
                           * Matrix_Scale(flag_scale, flag_scale, flag_scale)
                           * Matrix_Translate(-flag_center.x, -flag_center.y, -flag_min.z);
            } else {
                model_flag = Matrix_Translate(g_HolePosition.x, altura_base_mastro + g_FlagHeightOffset, g_HolePosition.z) 
                           * Matrix_Rotate_X(-1.5708f) 
                           * Matrix_Scale(flag_scale, flag_scale, flag_scale)
                           * Matrix_Translate(-flag_center.x, -flag_center.y, -flag_min.z);
            }
            
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model_flag));
            DrawVirtualObject(shape.name.c_str());
        }

        // BOLA (Intocado)
        glUniform1i(g_object_id_uniform, BALL);
        float ball_scale = 1.0f;
        glm::vec3 ball_pivot = glm::vec3(0.0f);
        if(g_VirtualScene.count("golf_ball")) {
            glm::vec3 b_min = g_VirtualScene["golf_ball"].bbox_min;
            glm::vec3 b_max = g_VirtualScene["golf_ball"].bbox_max;
            ball_pivot = (b_min + b_max) / 2.0f; 
            glm::vec3 sz = b_max - b_min;
            float max_dim = std::max({sz.x, sz.y, sz.z});
            if (max_dim > 0) ball_scale = (g_BallRadius * 1.5f) / max_dim;
        }

        model = Matrix_Translate(g_BallPosition.x, g_BallPosition.y, g_BallPosition.z) 
              * g_BallRotationMatrix 
              * Matrix_Scale(ball_scale, ball_scale, ball_scale)
              * Matrix_Translate(-ball_pivot.x, -ball_pivot.y, -ball_pivot.z);
              
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        DrawVirtualObject("golf_ball");

        // TACO (Intocado)
        glUniform1i(g_object_id_uniform, CLUB);
        if (!g_BallInHole && (glm::length(g_BallVelocity) == 0.0f || g_IsSwinging))
        {
            float club_scale = 1.0f;
            glm::vec3 pivot = glm::vec3(0.0f);

            if(g_VirtualScene.count("rdmobj00")) {
                glm::vec3 c_min = g_VirtualScene["rdmobj00"].bbox_min;
                glm::vec3 c_max = g_VirtualScene["rdmobj00"].bbox_max;
                glm::vec3 sz = c_max - c_min;
                if (std::max({sz.x, sz.y, sz.z}) > 0) club_scale = 1.2f / std::max({sz.x, sz.y, sz.z});
                pivot = (c_min + c_max) / 2.0f;
                pivot.y = c_min.y;
                pivot.z = c_max.z; 
            }

            float back_x = sin(g_CameraTheta);
            float back_z = cos(g_CameraTheta);
            float recuo = g_BallRadius + 0.05f + (g_ShotIntensity / MAX_INTENSITY) * 0.8f;
            glm::vec4 clubPos = g_BallPosition + glm::vec4(back_x, 0.0f, back_z, 0.0f) * recuo;
            clubPos.y = g_BallPosition.y + 1.125f;

            model = Matrix_Translate(clubPos.x, clubPos.y, clubPos.z)
                  * Matrix_Rotate_Y(g_CameraTheta)
                  * Matrix_Rotate_X(-1.5708f)
                  * Matrix_Rotate_Z(0.0f)    
                  * Matrix_Scale(club_scale, club_scale, club_scale)
                  * Matrix_Translate(-pivot.x, -pivot.y, -pivot.z); 

            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            DrawVirtualObject("rdmobj00");
            glDisable(GL_BLEND);
        }

        // ====================================================================
        // RENDERIZAÇÃO DO FUNDO DO PLACAR (Tabela de Madeira em 2D UI)
        // ====================================================================
        if (g_BallInHole) {
            glDisable(GL_DEPTH_TEST); // Desativa a profundidade para desenhar sobre todo o cenário 3D

            // 1. Congela a câmera num plano 2D perfeito (Orthographic View)
            glm::mat4 hud_view = Matrix_Identity();
            glm::mat4 hud_proj = Matrix_Identity();
            glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(hud_view));
            glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(hud_proj));

            // 2. Traz a "Luz Neon" temporariamente para frente da tela. 
            // Isso garante que a tabela de madeira não fique escura (já que a bola original está longe, lá no buraco).
            glm::vec4 hud_light = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f);
            glUniform4fv(g_ball_position_uniform, 1, glm::value_ptr(hud_light));

            // 3. Define a textura como madeira (WALL) e desenha o plane
            glUniform1i(g_object_id_uniform, WALL); 
            
            // O plano original é deitado (eixo XZ). Matrix_Rotate_X vira ele de frente para a câmera (XY).
            // Translate e Scale centralizam a placa de madeira exatamentee atrás dos seus textos.
            glm::mat4 model_hud = Matrix_Translate(0.0f, -0.05f, 0.0f) // Centralizado na tela
                                * Matrix_Rotate_X(1.5708f) 
                                * Matrix_Scale(1.35f, 1.0f, 0.55f); // Esticado horizontalmente

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model_hud));
            DrawVirtualObject("the_plane");

            glEnable(GL_DEPTH_TEST); // Restaura o teste de profundidade para o próximo frame
        }

        // ====================================================================
        // INTERFACE DO UTILIZADOR (HUD) - TABELA HORIZONTAL GIGANTE E BOLD
        // ====================================================================
        const int MAP_PAR = 4; 

        // Lambda auxiliar corrigido para escalar a fonte de verdade e arrumar o Negrito
        auto PrintBold = [&](GLFWwindow* win, const std::string& text, float x, float y, float scale) {
            float old_scale = textscale; // Guarda o valor original para não quebrar o FPS
            textscale = scale;           // AGORA SIM o texto vai ficar do tamanho que queremos!

            // Deslocamento microscópico e estático (1.5 milímetros) para engrossar a letra sem separar
            float offset = 0.0015f;

            TextRendering_PrintString(win, text, x, y, scale);
            TextRendering_PrintString(win, text, x + offset, y, scale); 
            TextRendering_PrintString(win, text, x, y + offset, scale); 
            TextRendering_PrintString(win, text, x + offset, y + offset, scale); 
            
            textscale = old_scale; // Devolve o tamanho ao normal
        };

        // 1. Contador Principal de Partida (Aumentado para scale 1.5f)
        char txtInfo[60];
        snprintf(txtInfo, 60, "PISTA: BURACO 01  |  PAR: %d", MAP_PAR);
        PrintBold(window, txtInfo, -0.95f, 0.92f, 1.3f);

        char txtStrokes[40];
        snprintf(txtStrokes, 40, "TACADAS TOTAIS: %d", g_Strokes);
        PrintBold(window, txtStrokes, -0.95f, 0.78f, 1.8f); // Fonte bem grande e legível no canto

        // 2. Barra de Força Visual Dinâmica (Estilo Carregamento)
        if (g_IsCharging) {
            int total_segments = 15;
            int active_segments = (int)((g_ShotIntensity / MAX_INTENSITY) * total_segments);
            std::string bar_str = "FORCA: [";
            for (int i = 0; i < total_segments; i++) {
                bar_str += (i < active_segments) ? "I" : ".";
            }
            bar_str += "] " + std::to_string((int)(g_ShotIntensity * 5)) + "%";
            PrintBold(window, bar_str, -0.95f, -0.85f, 1.5f);
        }

        // 3. Sistema de Vitória com Painel de Madeira Tridimensional
        if (g_BallInHole) {
            // --- CÁLCULO DO TERMO OFICIAL DE GOLFE ---
            std::string golf_term = "";
            if (g_Strokes == 1) {
                golf_term = "HOLE IN ONE!!!";
            } else {
                int score_diff = g_Strokes - MAP_PAR;
                switch (score_diff) {
                    case -3: golf_term = "ALBATROSS!"; break;
                    case -2: golf_term = "EAGLE!!"; break;
                    case -1: golf_term = "BIRDIE!"; break;
                    case 0:  golf_term = "PAR!"; break;
                    case 1:  golf_term = "BOGEY"; break;
                    case 2:  golf_term = "DOUBLE BOGEY"; break;
                    case 3:  golf_term = "TRIPLE BOGEY"; break;
                    default: golf_term = (score_diff > 3) ? "OVER BOGEY" : "FIM DE JOGO!"; break;
                }
            }

            // TEXTOS DO PLACAR HORIZONTAL (Alinhamento limpo sem caracteres de desenho)
            // Título Principal com escala gigante (2.2f)
            std::string msg_vitoria = "CONCLUIDO: " + golf_term;
            PrintBold(window, msg_vitoria, -0.50f, 0.32f, 2.2f);

            // Coordenadas horizontais alinhadas por colunas para a tabela limpa
            float col1_x = -0.55f; // Pista
            float col2_x = -0.25f; // Par
            float col3_x =  0.02f; // Suas Tacadas
            float col4_x =  0.32f; // Resultado

            float row_titles_y = 0.08f;
            float row_values_y = -0.08f;
            float text_scale   = 1.5f; // Fonte muito maior para todas as linhas

            // Cabeçalho da Tabela Horizontal
            PrintBold(window, "BURACO",    col1_x, row_titles_y, text_scale);
            PrintBold(window, "PAR",       col2_x, row_titles_y, text_scale);
            PrintBold(window, "TACADAS",   col3_x, row_titles_y, text_scale);
            PrintBold(window, "STATUS",    col4_x, row_titles_y, text_scale);

            // Valores Reais do Jogador logo abaixo das colunas correspondentes
            char str_par[10], str_strokes[10];
            snprintf(str_par, 10, "%d", MAP_PAR);
            snprintf(str_strokes, 10, "%d", g_Strokes);

            PrintBold(window, "#01",       col1_x + 0.02f, row_values_y, text_scale);
            PrintBold(window, str_par,     col2_x + 0.02f, row_values_y, text_scale);
            PrintBold(window, str_strokes, col3_x + 0.05f, row_values_y, text_scale);
            PrintBold(window, golf_term,   col4_x,         row_values_y, text_scale);

            // Botão de Reinício na parte inferior do painel
            PrintBold(window, "Pressione 'R' para reiniciar", -0.42f, -0.32f, 1.4f);
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
    if ( data == NULL ) {
        fprintf(stderr, "ERRO: Nao foi possivel abrir a imagem \"%s\".\n", filename);
        return; 
    }
    GLuint texture_id; GLuint sampler_id;
    glGenTextures(1, &texture_id); glGenSamplers(1, &sampler_id);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
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
    if(g_VirtualScene.find(object_name) == g_VirtualScene.end()) return;
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);
    glDrawElements(g_VirtualScene[object_name].rendering_mode, static_cast<GLsizei>(g_VirtualScene[object_name].num_indices), GL_UNSIGNED_INT, (void*)(g_VirtualScene[object_name].first_index * sizeof(GLuint)));
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
    g_ball_position_uniform = glGetUniformLocation(g_GpuProgramID, "ball_position");
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage3"), 3);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage4"), 4); 
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage5"), 5);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage6"), 6);
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
        std::vector<int> normal_indices(num_vertices, 0);
        for (size_t vertex_index = 0; vertex_index < vertex_normals.size(); ++vertex_index) {
            if (num_triangles_per_vertex[vertex_index] == 0) continue;
            glm::vec4 n = vertex_normals[vertex_index] / (float)num_triangles_per_vertex[vertex_index]; n /= norm(n);
            model->attrib.normals.push_back( n.x ); model->attrib.normals.push_back( n.y ); model->attrib.normals.push_back( n.z );
            normal_indices[vertex_index] = static_cast<int>((model->attrib.normals.size() / 3) - 1);
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
        glm::vec3 bbox_min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 bbox_max = glm::vec3(std::numeric_limits<float>::lowest()); 
        for (size_t triangle = 0; triangle < model->shapes[shape].mesh.num_face_vertices.size(); ++triangle) {
            for (size_t vertex = 0; vertex < 3; ++vertex) {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex]; 
                indices.push_back(static_cast<GLuint>(first_index + 3*triangle + vertex));
                float vx = model->attrib.vertices[3*idx.vertex_index + 0]; float vy = model->attrib.vertices[3*idx.vertex_index + 1]; float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                model_coefficients.push_back(vx); model_coefficients.push_back(vy); model_coefficients.push_back(vz); model_coefficients.push_back(1.0f);
                bbox_min.x = std::min(bbox_min.x, vx); bbox_min.y = std::min(bbox_min.y, vy); bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx); bbox_max.y = std::max(bbox_max.y, vy); bbox_max.z = std::max(bbox_max.z, vz);
                if ( idx.normal_index != -1 ) { normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 0]); normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 1]); normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 2]); normal_coefficients.push_back(0.0f); }
                if ( idx.texcoord_index != -1 ) { texture_coefficients.push_back(model->attrib.texcoords[2*idx.texcoord_index + 0]); texture_coefficients.push_back(model->attrib.texcoords[2*idx.texcoord_index + 1]); }
            }
        }
        SceneObject theobject; theobject.name = model->shapes[shape].name; theobject.first_index = first_index; theobject.num_indices = indices.size() - first_index; theobject.rendering_mode = GL_TRIANGLES; theobject.vertex_array_object_id = vertex_array_object_id; theobject.bbox_min = bbox_min; theobject.bbox_max = bbox_max; g_VirtualScene[model->shapes[shape].name] = theobject;
    }
    GLuint VBO; glGenBuffers(1, &VBO); glBindBuffer(GL_ARRAY_BUFFER, VBO); glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), model_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(0);
    if ( !normal_coefficients.empty() ) { glGenBuffers(1, &VBO); glBindBuffer(GL_ARRAY_BUFFER, VBO); glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), normal_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(1); }
    if ( !texture_coefficients.empty() ) { glGenBuffers(1, &VBO); glBindBuffer(GL_ARRAY_BUFFER, VBO); glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), texture_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(2); }
    GLuint indices_id; glGenBuffers(1, &indices_id); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id); glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW); glBindVertexArray(0);
}

void LoadShader(const char* filename, GLuint shader_id) {
    std::ifstream file; try { file.exceptions(std::ifstream::failbit); file.open(filename); } catch ( std::exception& /*e*/ ) { std::exit(EXIT_FAILURE); }
    std::stringstream shader; shader << file.rdbuf(); std::string str = shader.str(); const GLchar* shader_string = str.c_str(); const GLint length = static_cast<GLint>( str.length() ); glShaderSource(shader_id, 1, &shader_string, &length); glCompileShader(shader_id);
}

GLuint LoadShader_Vertex(const char* filename) { GLuint id = glCreateShader(GL_VERTEX_SHADER); LoadShader(filename, id); return id; }
GLuint LoadShader_Fragment(const char* filename) { GLuint id = glCreateShader(GL_FRAGMENT_SHADER); LoadShader(filename, id); return id; }
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id) { GLuint program_id = glCreateProgram(); glAttachShader(program_id, vertex_shader_id); glAttachShader(program_id, fragment_shader_id); glLinkProgram(program_id); glDeleteShader(vertex_shader_id); glDeleteShader(fragment_shader_id); return program_id; }
void FramebufferSizeCallback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); g_ScreenRatio = (float)width / height; }
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) { if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) { glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY); g_LeftMouseButtonPressed = true; } if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) g_LeftMouseButtonPressed = false; }
void ErrorCallback(int error, const char* description) {}
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) { g_CameraDistance -= 0.5f * (float)yoffset; if (g_CameraDistance < 0.5f) g_CameraDistance = 0.5f; }
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode) { if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GL_TRUE); }

void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) { 
    if (g_LeftMouseButtonPressed) { 
        g_CameraTheta -= 0.01f * (float)(xpos - g_LastCursorPosX); 
        g_CameraPhi += 0.01f * (float)(ypos - g_LastCursorPosY); 
        float phimax = 3.141592f/2.0f; 
        if (g_CameraPhi > phimax) g_CameraPhi = phimax; 
        if (g_CameraPhi < 0.05f) g_CameraPhi = 0.05f; 
        g_LastCursorPosX = xpos; g_LastCursorPosY = ypos; 
    } 
}