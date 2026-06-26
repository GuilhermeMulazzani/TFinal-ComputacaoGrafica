//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//   INF01047 Computação Gráfica e Visualização I
//             Prof. Eduardo Gastal
//
//     CÓDIGO FINAL - MINI GOLF MULTIPLAYER / MULTI-LEVEL / RANDOM MAPS

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
#include <ctime>

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

extern float textscale;

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4244) // Ignora o aviso de conversão int64 para int32
    #pragma warning(disable: 4267) // Ignora avisos similares de size_t
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#ifdef _MSC_VER
    #pragma warning(pop) // Restaura os avisos para o restante do SEU código
#endif

glm::mat4 Matrix_Rotate_Axis(float angle, glm::vec3 axis) {
    float c = cos(angle); float s = sin(angle); float t = 1.0f - c;
    float magnitude = sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (magnitude < 0.000001f) return glm::mat4(1.0f); 
    float x = axis.x / magnitude; float y = axis.y / magnitude; float z = axis.z / magnitude;
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

void PushMatrix(glm::mat4 M); void PopMatrix(glm::mat4& M);
void BuildTrianglesAndAddToVirtualScene(ObjModel*); 
void ComputeNormals(ObjModel* model); void LoadShadersFromFiles(); 
void LoadTextureImage(const char* filename); void DrawVirtualObject(const char* object_name); 
GLuint LoadShader_Vertex(const char* filename); GLuint LoadShader_Fragment(const char* filename); 
void LoadShader(const char* filename, GLuint shader_id); 
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); 
void TextRendering_Init(); float TextRendering_LineHeight(GLFWwindow* window);
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
    std::string  name; size_t first_index; size_t num_indices; 
    GLenum rendering_mode; GLuint vertex_array_object_id; 
    glm::vec3 bbox_min; glm::vec3 bbox_max;
};

std::map<std::string, SceneObject> g_VirtualScene;
std::stack<glm::mat4>  g_MatrixStack;
float g_ScreenRatio = 1.0f;
bool g_LeftMouseButtonPressed = false;
double g_LastCursorPosX = 0.0; double g_LastCursorPosY = 0.0;

float g_CameraTheta = 0.0f; float g_CameraPhi = 0.4f; float g_CameraDistance = 1.5f; 

float g_CameraSensitivity = 0.01f;
float g_CameraFOV = 3.141592f / 3.0f; 
bool g_IsRandomMode = false;
int g_RandomModeTotalHoles = 4;   
int g_RandomModeCurrentHole = 1;  
// Variáveis Globais do Áudio
ma_engine g_AudioEngine;
ma_sound g_MusicBGM;
ma_sound g_SndTacada;
ma_sound g_SndQuique;
ma_sound g_SndCaindo;
bool g_AudioOK = false;

// Controle de Volume Padrão (50%)
float g_MusicVolume = 0.5f;
float g_SFXVolume   = 0.5f;

#define WALL  0
#define FLOOR 1
#define BALL  2
#define CLUB  3
#define HOLE  4          
#define FLAG_FABRIC 5    
#define FLAG_POLE   6
#define HILL 7
#define HUD_BAR 8

glm::vec3 g_BallColors[9] = {
    glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 0.2f, 0.2f), glm::vec3(0.2f, 1.0f, 0.2f), 
    glm::vec3(0.2f, 0.4f, 1.0f), glm::vec3(1.0f, 0.8f, 0.1f), glm::vec3(0.6f, 0.1f, 0.8f), 
    glm::vec3(1.0f, 0.4f, 0.7f), glm::vec3(1.0f, 0.5f, 0.0f), glm::vec3(0.6f, 0.6f, 0.6f)  
};
int g_CurrentBallColorIndex = 0;

int g_CurrentLevel = 0; 
bool g_CampaignFinished = false; 

glm::vec4 g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f); 
glm::vec4 g_SpawnPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f);
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
float g_HoleRadius = 0.13f; 
bool g_BallInHole = false;

float g_HoleCooldown = 0.0f;
std::vector<int> g_ScoreHistory;
std::vector<int> g_ParHistory;
float g_FlagHeightOffset = -0.4f; 

// ====================================================================
// ARQUITETURA UNIFICADA DE GEOMETRIA 
// ====================================================================
struct Wall { glm::vec2 p1, p2; }; std::vector<Wall> g_Walls;
struct HillDef { float x, z, radius, height; }; std::vector<HillDef> g_LevelHills;
struct FloorDef { float cx, cz, sx, sz, y, pitch; }; std::vector<FloorDef> g_LevelFloors;
struct RampDef { float z_start, z_end, height_start, height_end; }; std::vector<RampDef> g_LevelRamps;
struct MovingWallDef { int wall_index; float center_x, center_z; float range, speed; bool move_in_x; }; std::vector<MovingWallDef> g_MovingObstacles;

float RandomFloat(float min, float max) {
    return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/(max-min)));
}

void LoadLevel(int level) {
    g_Strokes = 0; g_BallVelocity = glm::vec4(0.0f); g_BallRotationMatrix = glm::mat4(1.0f);
    g_BallInHole = false; g_HoleCooldown = 0.0f; g_IsSwinging = false;
    g_ShotIntensity = 0.0f; g_FlagHeightOffset = -0.4f; g_CameraTheta = 0.0f; g_CameraPhi = 0.4f;

    if ((level == 1 && !g_IsRandomMode) || (level >= 4 && g_RandomModeCurrentHole == 1)) {
        g_ScoreHistory.clear(); g_ParHistory.clear();
    }

    g_Walls.clear(); g_LevelRamps.clear(); g_LevelHills.clear(); g_MovingObstacles.clear(); g_LevelFloors.clear();

    if (level <= 0) return; 

    if (level == 1) {
        g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f);
        g_HolePosition = glm::vec4(-4.0f, -0.19f, -17.5f, 1.0f);
        g_Walls = {
            {{ 1.5f,  1.0f}, { 1.5f, -4.0f}}, {{-0.5f, -7.0f}, {-0.5f,-11.0f}}, 
            {{ 1.5f, -4.0f}, {-0.5f, -7.0f}}, {{-0.5f,-11.0f}, {-2.5f,-14.0f}}, 
            {{-2.5f,-14.0f}, {-2.5f,-19.0f}}, {{-1.5f, -4.0f}, {-1.5f,  1.0f}}, 
            {{-3.5f, -7.0f}, {-1.5f, -4.0f}}, {{-3.5f,-11.0f}, {-3.5f, -7.0f}}, 
            {{-5.5f,-14.0f}, {-3.5f,-11.0f}}, {{-5.5f,-19.0f}, {-5.5f,-14.0f}}, 
            {{ 1.5f,  1.0f}, {-1.5f,  1.0f}}, {{-2.5f,-19.0f}, {-5.5f,-19.0f}},
            {{ 1.5f, -2.0f}, { 0.0f, -2.0f}}, {{-3.5f, -9.0f}, {-1.5f, -9.0f}}  
        };
        g_LevelFloors.push_back({0.0f, -1.5f, 1.5f, 2.5f, -0.2f, 0.0f});
        g_LevelFloors.push_back({-1.0f, -5.5f, 2.5f, 1.5f, -0.2f, 0.0f});
        g_LevelFloors.push_back({-2.0f, -9.0f, 1.5f, 2.0f, -0.2f, 0.0f});
        g_LevelFloors.push_back({-4.0f, -12.5f, 3.5f, 1.5f, -0.2f, 0.0f});
        g_LevelFloors.push_back({-4.0f, -16.5f, 1.5f, 2.5f, -0.2f, 0.0f});
        
    } else if (level == 2) {
        g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f);
        g_HolePosition = glm::vec4(0.0f, -0.19f, -16.0f, 1.0f);
        
        // CORREÇÃO 3: Estruturação Correta da Fase 2 (Sincronização Física x Visual)
        // 1. Início
        g_Walls.push_back({{ 1.5f,  1.0f}, { 1.5f, -5.0f}}); g_Walls.push_back({{-1.5f, -5.0f}, {-1.5f,  1.0f}}); g_Walls.push_back({{ 1.5f,  1.0f}, {-1.5f,  1.0f}});
        // 2. Rampa Descendo
        g_Walls.push_back({{ 1.5f, -5.0f}, { 1.5f, -8.0f}}); g_Walls.push_back({{-1.5f, -8.0f}, {-1.5f, -5.0f}});
        
        // 3. O Abismo / Ponte Fina (Fica no centro, e criamos quinas fechadas para não atravessar)
        g_Walls.push_back({{ 1.5f, -8.0f}, { 1.0f, -8.0f}}); g_Walls.push_back({{-1.5f, -8.0f}, {-1.0f, -8.0f}}); // Fechamento Superior
        g_Walls.push_back({{ 1.0f, -11.0f}, { 1.5f, -11.0f}}); g_Walls.push_back({{-1.0f, -11.0f}, {-1.5f, -11.0f}}); // Fechamento Inferior
        
        // 4. Rampa Subindo
        g_Walls.push_back({{ 1.5f, -11.0f}, { 1.5f, -14.0f}}); g_Walls.push_back({{-1.5f, -14.0f}, {-1.5f, -11.0f}});
        // 5. Final Platform
        g_Walls.push_back({{ 1.5f, -14.0f}, { 2.0f, -14.0f}}); g_Walls.push_back({{-1.5f, -14.0f}, {-2.0f, -14.0f}}); // Alarga para 2.0
        g_Walls.push_back({{ 2.0f, -14.0f}, { 2.0f, -18.0f}}); g_Walls.push_back({{-2.0f, -18.0f}, {-2.0f, -14.0f}}); 
        g_Walls.push_back({{ 2.0f, -18.0f}, {-2.0f, -18.0f}}); // Fundo

        g_LevelRamps.push_back({-5.0f, -8.0f, -0.2f, -0.8f}); 
        g_LevelRamps.push_back({-11.0f, -14.0f, -0.8f, -0.2f});
        
        g_LevelFloors.push_back({0.0f, -2.0f, 1.5f, 3.0f, -0.2f, 0.0f});
        float d_hyp = sqrt(3.0f*3.0f + 0.6f*0.6f) / 2.0f; float d_pitch = atan2(-0.6f, 3.0f);
        g_LevelFloors.push_back({0.0f, -6.5f, 1.5f, d_hyp, -0.5f, d_pitch}); 
        g_LevelFloors.push_back({0.0f, -9.5f, 1.0f, 1.5f, -0.8f, 0.0f}); // Ponte
        float a_hyp = sqrt(3.0f*3.0f + 0.6f*0.6f) / 2.0f; float a_pitch = atan2(0.6f, 3.0f);
        g_LevelFloors.push_back({0.0f, -12.5f, 1.5f, a_hyp, -0.5f, a_pitch}); 
        g_LevelFloors.push_back({0.0f, -16.0f, 2.0f, 2.0f, -0.2f, 0.0f});

    } else if (level == 3) {
        g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f);
        g_HolePosition = glm::vec4(0.0f, 0.41f, -22.0f, 1.0f); 
        
        g_LevelRamps.push_back({-8.0f, -12.0f, -0.2f, 0.4f}); 
        g_LevelHills.push_back({0.0f, -4.0f, 1.4f, 0.28f});   

        g_Walls.push_back({{ 2.5f,  2.0f}, { 2.5f, -8.0f}});   g_Walls.push_back({{-2.5f, -8.0f}, {-2.5f,  2.0f}}); 
        g_Walls.push_back({{ 2.5f,  2.0f}, {-2.5f,  2.0f}});   
        g_Walls.push_back({{ 2.5f, -8.0f}, { 2.5f, -12.0f}});  g_Walls.push_back({{-2.5f, -12.0f}, {-2.5f, -8.0f}});
        g_Walls.push_back({{ 2.5f, -12.0f}, { 2.5f, -25.0f}}); g_Walls.push_back({{-2.5f, -25.0f}, {-2.5f, -12.0f}}); 
        g_Walls.push_back({{ 2.5f, -25.0f}, {-2.5f, -25.0f}});
        
        g_Walls.push_back({{-1.0f, -6.0f}, {-0.95f, -6.0f}}); 
        g_Walls.push_back({{ 1.0f, -6.0f}, { 1.05f, -6.0f}});
        g_Walls.push_back({{-1.0f, -16.0f}, {1.0f, -16.0f}});
        g_MovingObstacles.push_back({(int)g_Walls.size() - 1, 0.0f, -16.0f, 1.1f, 2.3f, true});

        float hx = g_HolePosition.x; float hz = g_HolePosition.z;
        g_Walls.push_back({{hx - 0.6f, hz - 0.6f}, {hx + 0.6f, hz - 0.6f}}); 
        g_Walls.push_back({{hx - 0.6f, hz - 0.6f}, {hx - 0.6f, hz + 0.4f}}); 
        g_Walls.push_back({{hx + 0.6f, hz - 0.6f}, {hx + 0.6f, hz + 0.4f}}); 

        g_LevelFloors.push_back({0.0f, -3.0f, 2.5f, 5.0f, -0.2f, 0.0f}); 
        g_LevelFloors.push_back({0.0f, -10.0f, 2.5f, 2.05f, 0.10f, 0.149f});
        g_LevelFloors.push_back({0.0f, -18.5f, 2.5f, 6.5f, 0.40f, 0.0f}); 
        
    } else if (level >= 4) {
        g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f);
        float width = 3.0f; float z_cursor = 2.0f; float y_cursor = -0.2f;
        
        g_Walls.push_back({{width, z_cursor}, {-width, z_cursor}}); 
        
        int num_sections = (rand() % 3) + 3; 
        for (int i=0; i<num_sections; i++) {
            int type = rand() % 3; 
            // CORREÇÃO 1: NUNCA gera rampa no primeiro passo!
            if (i == 0) type = 0; 

            float length = RandomFloat(5.0f, 10.0f);
            
            if (type == 0 || i == num_sections - 1) { 
                g_Walls.push_back({{width, z_cursor}, {width, z_cursor - length}});
                g_Walls.push_back({{-width, z_cursor - length}, {-width, z_cursor}});
                g_LevelFloors.push_back({0.0f, z_cursor - length/2.0f, width, length/2.0f, y_cursor, 0.0f});
                
                int obs_type = rand() % 3;
                if (obs_type == 1 && length > 6.0f) { 
                    g_Walls.push_back({{width, z_cursor - length*0.3f}, {0.0f, z_cursor - length*0.3f}});
                    g_Walls.push_back({{-width, z_cursor - length*0.6f}, {0.0f, z_cursor - length*0.6f}});
                } else if (obs_type == 2) { 
                    float cx = RandomFloat(-width/2.0f, width/2.0f); float cz = z_cursor - length/2.0f;
                    g_Walls.push_back({{cx-0.5f, cz-0.5f}, {cx+0.5f, cz-0.5f}});
                    g_Walls.push_back({{cx+0.5f, cz-0.5f}, {cx+0.5f, cz+0.5f}});
                    g_Walls.push_back({{cx+0.5f, cz+0.5f}, {cx-0.5f, cz+0.5f}});
                    g_Walls.push_back({{cx-0.5f, cz+0.5f}, {cx-0.5f, cz-0.5f}});
                }
                if (rand() % 100 < 30) g_LevelHills.push_back({ RandomFloat(-1.5f, 1.5f), z_cursor - length/2.0f, RandomFloat(1.0f, 2.0f), RandomFloat(0.2f, 0.4f) });
                z_cursor -= length;
            } else if (type == 1) { 
                float next_y = y_cursor + 0.6f;
                g_Walls.push_back({{width, z_cursor}, {width, z_cursor - length}}); g_Walls.push_back({{-width, z_cursor - length}, {-width, z_cursor}});
                float pitch = atan2(0.6f, length); float hyp = sqrt(length*length + 0.6f*0.6f);
                g_LevelFloors.push_back({0.0f, z_cursor - length/2.0f, width, hyp/2.0f, y_cursor + 0.3f, pitch});
                g_LevelRamps.push_back({z_cursor, z_cursor - length, y_cursor, next_y});
                y_cursor = next_y; z_cursor -= length;
            } else if (type == 2) { 
                float next_y = y_cursor - 0.6f;
                g_Walls.push_back({{width, z_cursor}, {width, z_cursor - length}}); g_Walls.push_back({{-width, z_cursor - length}, {-width, z_cursor}});
                float pitch = atan2(-0.6f, length); float hyp = sqrt(length*length + 0.6f*0.6f);
                g_LevelFloors.push_back({0.0f, z_cursor - length/2.0f, width, hyp/2.0f, y_cursor - 0.3f, pitch});
                g_LevelRamps.push_back({z_cursor, z_cursor - length, y_cursor, next_y});
                y_cursor = next_y; z_cursor -= length;
            }
        }
        g_Walls.push_back({{-width, z_cursor}, {width, z_cursor}}); 
        g_HolePosition = glm::vec4(RandomFloat(-width/2.0f, width/2.0f), y_cursor + 0.01f, z_cursor + 1.5f, 1.0f);
        
        float hx = g_HolePosition.x; float hz = g_HolePosition.z;
        g_Walls.push_back({{hx - 0.6f, hz - 0.6f}, {hx + 0.6f, hz - 0.6f}}); 
        g_Walls.push_back({{hx - 0.6f, hz - 0.6f}, {hx - 0.6f, hz + 0.4f}}); 
        g_Walls.push_back({{hx + 0.6f, hz - 0.6f}, {hx + 0.6f, hz + 0.4f}});
    }
    
    g_SpawnPosition = g_BallPosition; 
}

float GetFloorHeight(float x, float z) {
    float floor_y = -20.0f; // Abismo por padrão. Se estiver fora da pista geométrica, cai!
    
    for (auto& f : g_LevelFloors) {
        if (x >= f.cx - f.sx - 0.15f && x <= f.cx + f.sx + 0.15f && z <= f.cz + f.sz + 0.15f && z >= f.cz - f.sz - 0.15f) {
            float cy = f.y - (z - f.cz) * tan(f.pitch); 
            if (cy > floor_y) floor_y = cy; 
        }
    }
    
    for (auto& h : g_LevelHills) {
        float dist = sqrt((x - h.x)*(x - h.x) + (z - h.z)*(z - h.z));
        float effective_r = h.radius + (g_BallRadius * 0.1f); 
        if (dist < effective_r) {
            float n_dist = dist / effective_r;
            float hill_y = floor_y + sqrt(1.0f - n_dist * n_dist) * h.height;
            if (hill_y > floor_y) floor_y = hill_y;
        }
    }
    return floor_y;
}

GLuint g_GpuProgramID = 0;
GLint g_model_uniform, g_view_uniform, g_projection_uniform, g_object_id_uniform;
GLint g_bbox_min_uniform, g_bbox_max_uniform, g_ball_position_uniform;
GLint g_ball_color_uniform, g_hole_position_uniform;
GLuint g_NumLoadedTextures = 0;

int main(int argc, char* argv[])
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    if (!glfwInit()) std::exit(EXIT_FAILURE);

    glfwSetErrorCallback(ErrorCallback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Golf it Again! - Fisicas Corrigidas", NULL, NULL);
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

    LoadTextureImage("../../data/madeira_textures/textures/oak_veneer_01_diff_4k.jpg");        
    LoadTextureImage("../../data/quadriculado_chao/textures/floor_tiles_06_diff_4k.jpg");            
    LoadTextureImage("../../data/golf_ball/textures/textura-golf.jpg");    
    LoadTextureImage("../../data/taco_golf/thumbnail.jpg");    
    LoadTextureImage("../../data/textura_buraco.jpg");                                
    LoadTextureImage("../../data/textura_bandeira.jpeg"); 
    LoadTextureImage("../../data/textura_metal.jpg");    
    LoadTextureImage("../../data/rocky_terrain_02_diff_1k.jpg");                                        

    ObjModel planemodel("../../data/plane.obj"); ComputeNormals(&planemodel); BuildTrianglesAndAddToVirtualScene(&planemodel);
    ObjModel spheremodel("../../data/sphere.obj"); ComputeNormals(&spheremodel); BuildTrianglesAndAddToVirtualScene(&spheremodel);
    ObjModel golfClubModel("../../data/taco_golf/model.obj"); ComputeNormals(&golfClubModel); BuildTrianglesAndAddToVirtualScene(&golfClubModel);
    ObjModel golfBallModel("../../data/golf_ball/golf_ball.obj"); ComputeNormals(&golfBallModel); BuildTrianglesAndAddToVirtualScene(&golfBallModel);
    ObjModel flagModel("../../data/bandeira_brasil.obj"); ComputeNormals(&flagModel); BuildTrianglesAndAddToVirtualScene(&flagModel);
    ObjModel blockModel("../../data/retangulo.obj"); ComputeNormals(&blockModel); BuildTrianglesAndAddToVirtualScene(&blockModel);
    ObjModel holeModel("../../data/buraco.obj"); ComputeNormals(&holeModel); BuildTrianglesAndAddToVirtualScene(&holeModel);

    TextRendering_Init();

    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);

    float flag_scale = 1.0f; glm::vec3 flag_min(1e9), flag_max(-1e9);
    for(auto& shape : flagModel.shapes) {
        if(g_VirtualScene.count(shape.name)) {
            flag_min = glm::min(flag_min, g_VirtualScene[shape.name].bbox_min);
            flag_max = glm::max(flag_max, g_VirtualScene[shape.name].bbox_max);
        }
    }
    float f_sz = std::max({flag_max.x - flag_min.x, flag_max.y - flag_min.y, flag_max.z - flag_min.z});
    if(f_sz > 0) flag_scale = 1.0f / f_sz; 
    glm::vec3 flag_center = (flag_min + flag_max) / 2.0f;

    float menuInputCooldown = 0.0f;
    
    // Inicializa o Áudio e carrega todos os sons ANTES do jogo começar
    if (ma_engine_init(NULL, &g_AudioEngine) == MA_SUCCESS) {
        g_AudioOK = true;
        
        // Música
        if (ma_sound_init_from_file(&g_AudioEngine, "../../data/musica_ambiente.mp3", 0, NULL, NULL, &g_MusicBGM) == MA_SUCCESS) {
            ma_sound_set_volume(&g_MusicBGM, g_MusicVolume);
            ma_sound_set_looping(&g_MusicBGM, MA_TRUE);
            ma_sound_start(&g_MusicBGM);
        }
        
        // Tacada
        if (ma_sound_init_from_file(&g_AudioEngine, "../../data/tacada.wav", 0, NULL, NULL, &g_SndTacada) == MA_SUCCESS) {
            ma_sound_set_volume(&g_SndTacada, g_SFXVolume);
        }

        // Quique na Parede
        if (ma_sound_init_from_file(&g_AudioEngine, "../../data/quique.wav", 0, NULL, NULL, &g_SndQuique) == MA_SUCCESS) {
            ma_sound_set_volume(&g_SndQuique, g_SFXVolume);
        }

        // Som da bola caindo no buraco
        if (ma_sound_init_from_file(&g_AudioEngine, "../../data/bola_caindo.wav", 0, NULL, NULL, &g_SndCaindo) == MA_SUCCESS) {
            ma_sound_set_volume(&g_SndCaindo, g_SFXVolume);
        }
    }

    while (!glfwWindowShouldClose(window))
    {
        float currentFrameTime = (float)glfwGetTime();
        static float lastFrameTime = 0.0f; float deltaTime = currentFrameTime - lastFrameTime; lastFrameTime = currentFrameTime;
        
        if (menuInputCooldown > 0.0f) menuInputCooldown -= deltaTime;

        auto PrintBold = [&](GLFWwindow* win, const std::string& text, float x, float y, float scale) {
            float old_scale = textscale; textscale = scale; float offset = 0.0015f;
            TextRendering_PrintString(win, text, x, y, scale); TextRendering_PrintString(win, text, x + offset, y, scale); 
            TextRendering_PrintString(win, text, x, y + offset, scale); TextRendering_PrintString(win, text, x + offset, y + offset, scale); 
            textscale = old_scale; 
        };

        if (g_CurrentLevel <= 0) {
            glClearColor(0.05f, 0.15f, 0.10f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glm::mat4 hud_view = Matrix_Identity(); glm::mat4 hud_proj = Matrix_Identity();
            glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(hud_view));
            glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(hud_proj));

            if (g_CurrentLevel == 0) {
                PrintBold(window, "MINI GOLF IT AGAIN!", -0.65f, 0.5f, 3.0f);
                
                // Todos alinhados em X = -0.4f
                PrintBold(window, "[ 1 ] - JOGAR", -0.4f, 0.15f, 2.0f);
                PrintBold(window, "[ 2 ] - OPCOES", -0.4f, -0.05f, 2.0f);
                PrintBold(window, "[ 3 ] - PERSONALIZAR BOLA", -0.4f, -0.25f, 2.0f);
                
                if (g_CampaignFinished) PrintBold(window, "- MODO INFINITO DESBLOQUEADO -", -0.55f, -0.5f, 1.5f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f;
                        if (g_CampaignFinished) g_CurrentLevel = -3; 
                        else { g_IsRandomMode = false; g_CurrentLevel = 1; LoadLevel(g_CurrentLevel); }
                    }
                    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) { menuInputCooldown = 0.5f; g_CurrentLevel = -1; }
                    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) { menuInputCooldown = 0.5f; g_CurrentLevel = -2; }
                }
            } else if (g_CurrentLevel == -1) {
                PrintBold(window, "OPCOES DO SISTEMA", -0.45f, 0.6f, 3.0f);
                
                char fovStr[64]; snprintf(fovStr, 64, "FOV da Camera: %.0f graus [ Q / E ]", g_CameraFOV * (180.0f/3.14159f));
                PrintBold(window, fovStr, -0.5f, 0.2f, 1.5f);
                
                char sensStr[64]; snprintf(sensStr, 64, "Sensibilidade Mouse: %.3f [ Z / C ]", g_CameraSensitivity);
                PrintBold(window, sensStr, -0.5f, 0.0f, 1.5f);

                // --- NOVOS CONTROLES DE ÁUDIO ---
                char musStr[64]; snprintf(musStr, 64, "Volume Musica: %d%% [ U / I ]", (int)(g_MusicVolume * 100));
                PrintBold(window, musStr, -0.5f, -0.2f, 1.5f);
                
                char sfxStr[64]; snprintf(sfxStr, 64, "Volume Efeitos: %d%% [ J / K ]", (int)(g_SFXVolume * 100));
                PrintBold(window, sfxStr, -0.5f, -0.4f, 1.5f);

                PrintBold(window, "[ V ] - VOLTAR", -0.2f, -0.7f, 2.0f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { g_CameraFOV -= 0.02f; menuInputCooldown = 0.05f; }
                    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { g_CameraFOV += 0.02f; menuInputCooldown = 0.05f; }
                    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) { g_CameraSensitivity -= 0.001f; menuInputCooldown = 0.05f; }
                    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) { g_CameraSensitivity += 0.001f; menuInputCooldown = 0.05f; }
                    
                    // Ajuste de Música (U diminui, I aumenta)
                    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) { 
                        g_MusicVolume -= 0.05f; if(g_MusicVolume < 0.0f) g_MusicVolume = 0.0f; 
                        if(g_AudioOK) ma_sound_set_volume(&g_MusicBGM, g_MusicVolume);
                        menuInputCooldown = 0.05f; 
                    }
                    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) { 
                        g_MusicVolume += 0.05f; if(g_MusicVolume > 1.0f) g_MusicVolume = 1.0f; 
                        if(g_AudioOK) ma_sound_set_volume(&g_MusicBGM, g_MusicVolume);
                        menuInputCooldown = 0.05f; 
                    }
                    
                    // Ajuste de Efeitos Sonoros (J diminui, K aumenta)
                    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) { 
                        g_SFXVolume -= 0.05f; if(g_SFXVolume < 0.0f) g_SFXVolume = 0.0f; 
                        if(g_AudioOK) { ma_sound_set_volume(&g_SndTacada, g_SFXVolume); ma_sound_set_volume(&g_SndQuique, g_SFXVolume); }
                        menuInputCooldown = 0.05f; 
                    }
                    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) { 
                        g_SFXVolume += 0.05f; if(g_SFXVolume > 1.0f) g_SFXVolume = 1.0f; 
                        if(g_AudioOK) { ma_sound_set_volume(&g_SndTacada, g_SFXVolume); ma_sound_set_volume(&g_SndQuique, g_SFXVolume); }
                        menuInputCooldown = 0.05f; 
                    }

                    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) { g_CurrentLevel = 0; menuInputCooldown = 0.5f; }
                }
                
                if (g_CameraFOV < 0.5f) g_CameraFOV = 0.5f; if (g_CameraFOV > 2.5f) g_CameraFOV = 2.5f;
                if (g_CameraSensitivity < 0.001f) g_CameraSensitivity = 0.001f; if (g_CameraSensitivity > 0.05f) g_CameraSensitivity = 0.05f;

            } else if (g_CurrentLevel == -2) {
                PrintBold(window, "PERSONALIZAR BOLA", -0.45f, 0.7f, 3.0f);
                std::string corNome = "BRANCA";
                if(g_CurrentBallColorIndex==1)corNome="VERMELHA"; else if(g_CurrentBallColorIndex==2)corNome="VERDE NEON";
                else if(g_CurrentBallColorIndex==3)corNome="AZUL CELESTE"; else if(g_CurrentBallColorIndex==4)corNome="DOURADA";
                else if(g_CurrentBallColorIndex==5)corNome="ROXA"; else if(g_CurrentBallColorIndex==6)corNome="ROSA";
                else if(g_CurrentBallColorIndex==7)corNome="LARANJA"; else if(g_CurrentBallColorIndex==8)corNome="CINZA";

                char colorStr[64]; snprintf(colorStr, 64, "< [ A ] - COR: %s - [ D ] >", corNome.c_str());
                PrintBold(window, colorStr, -0.55f, -0.6f, 2.0f); PrintBold(window, "[ V ] - VOLTAR", -0.2f, -0.85f, 2.0f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { g_CurrentBallColorIndex = (g_CurrentBallColorIndex - 1 + 9) % 9; menuInputCooldown = 0.2f; }
                    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { g_CurrentBallColorIndex = (g_CurrentBallColorIndex + 1) % 9; menuInputCooldown = 0.2f; }
                    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) { g_CurrentLevel = 0; menuInputCooldown = 0.5f; }
                }

                glUseProgram(g_GpuProgramID); glEnable(GL_DEPTH_TEST);
                glm::vec4 studio_light = glm::vec4(0.0f, 2.0f, 2.0f, 1.0f); glUniform4fv(g_ball_position_uniform, 1, glm::value_ptr(studio_light));
                glm::mat4 view = Matrix_Translate(0.0f, -0.1f, -1.2f); glm::mat4 proj = Matrix_Perspective(g_CameraFOV, g_ScreenRatio, -0.1f, -10.0f);
                glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view)); glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(proj));
                glUniform1i(g_object_id_uniform, BALL); glUniform3fv(g_ball_color_uniform, 1, glm::value_ptr(g_BallColors[g_CurrentBallColorIndex]));

                float preview_scale = 1.0f; glm::vec3 preview_pivot(0.0f);
                if(g_VirtualScene.count("golf_ball")) {
                    glm::vec3 b_min = g_VirtualScene["golf_ball"].bbox_min; glm::vec3 b_max = g_VirtualScene["golf_ball"].bbox_max;
                    preview_pivot = (b_min + b_max) / 2.0f; float max_dim = std::max({b_max.x-b_min.x, b_max.y-b_min.y, b_max.z-b_min.z});
                    if (max_dim > 0) preview_scale = 0.4f / max_dim; 
                }
                glm::mat4 model = Matrix_Rotate_Y((float)glfwGetTime() * 1.5f) * Matrix_Rotate_Z(0.3f) * Matrix_Scale(preview_scale, preview_scale, preview_scale) * Matrix_Translate(-preview_pivot.x, -preview_pivot.y, -preview_pivot.z);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model)); DrawVirtualObject("golf_ball"); glDisable(GL_DEPTH_TEST);
                
            } else if (g_CurrentLevel == -3) {
                PrintBold(window, "SELECIONE O MODO DE JOGO", -0.7f, 0.5f, 2.2f);
                
                // Todos alinhados em X = -0.6f com escala 1.6f
                PrintBold(window, "[ 1 ] - MODO CLASSICO (FASES 1-3)", -0.6f, 0.1f, 1.6f);
                PrintBold(window, "[ 2 ] - MODO RANDOMICO (INFINITO)", -0.6f, -0.1f, 1.6f);
                PrintBold(window, "[ V ] - VOLTAR", -0.6f, -0.4f, 1.6f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) { menuInputCooldown = 0.5f; g_IsRandomMode = false; g_CurrentLevel = 1; LoadLevel(g_CurrentLevel); }
                    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) { menuInputCooldown = 0.5f; g_CurrentLevel = -4; }
                    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) { menuInputCooldown = 0.5f; g_CurrentLevel = 0;  }
                }
            } else if (g_CurrentLevel == -4) {
                PrintBold(window, "CONFIGURAR MODO RANDOMICO", -0.6f, 0.5f, 2.5f);
                char holesStr[64]; snprintf(holesStr, 64, "QUANTIDADE DE BURACOS: %d", g_RandomModeTotalHoles); PrintBold(window, holesStr, -0.45f, 0.1f, 1.8f);
                PrintBold(window, "Pressione [ A ] para diminuir ou [ D ] para aumentar", -0.65f, -0.08f, 1.2f);
                PrintBold(window, "[ ENTER ] - INICIAR PARTIDA", -0.4f, -0.35f, 1.8f); PrintBold(window, "[ V ] - VOLTAR", -0.15f, -0.55f, 1.8f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { g_RandomModeTotalHoles--; if (g_RandomModeTotalHoles < 1) g_RandomModeTotalHoles = 1; menuInputCooldown = 0.15f; }
                    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { g_RandomModeTotalHoles++; if (g_RandomModeTotalHoles > 18) g_RandomModeTotalHoles = 18; menuInputCooldown = 0.15f; }
                    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) { menuInputCooldown = 0.5f; g_IsRandomMode = true; g_RandomModeCurrentHole = 1; g_CurrentLevel = 4; LoadLevel(g_CurrentLevel); }
                    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) { menuInputCooldown = 0.5f; g_CurrentLevel = -3; }
                }
            }
            glfwSwapBuffers(window); glfwPollEvents(); continue; 
        }

        for (auto& mo : g_MovingObstacles) {
            float offset = sin(currentFrameTime * mo.speed) * mo.range; float w_len = 1.0f; 
            if (mo.move_in_x) { g_Walls[mo.wall_index].p1 = {mo.center_x + offset - w_len, mo.center_z}; g_Walls[mo.wall_index].p2 = {mo.center_x + offset + w_len, mo.center_z}; }
        }

        float currentSpeed = sqrt(g_BallVelocity.x * g_BallVelocity.x + g_BallVelocity.z * g_BallVelocity.z);

        if (!g_BallInHole && currentSpeed < 0.05f && abs(g_BallVelocity.y) < 0.05f && !g_IsSwinging) {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                g_IsCharging = true; g_ShotIntensity += CHARGE_SPEED * deltaTime;
                if (g_ShotIntensity > MAX_INTENSITY) g_ShotIntensity = MAX_INTENSITY;
            } else if (g_IsCharging) {
                g_IsSwinging = true; g_StoredIntensity = g_ShotIntensity; g_IsCharging = false;
            }
        }

        if (g_IsSwinging) {
            const float SWING_SPEED = 140.0f; g_ShotIntensity -= SWING_SPEED * deltaTime;
            if (g_ShotIntensity <= 0.0f) {
                g_ShotIntensity = 0.0f; g_IsSwinging = false;
                g_BallVelocity = glm::vec4(-sin(g_CameraTheta), 0.0f, -cos(g_CameraTheta), 0.0f) * g_StoredIntensity;
                g_Strokes++; 
                
                // Toca Som da Tacada Controlado
                if (g_AudioOK) {
                    ma_sound_seek_to_pcm_frame(&g_SndTacada, 0); // Reseta o som
                    ma_sound_start(&g_SndTacada);
                }
            }
        }

        if (!g_BallInHole) {
            g_BallVelocity.x -= g_BallVelocity.x * 1.0f * deltaTime; g_BallVelocity.z -= g_BallVelocity.z * 1.0f * deltaTime; 
            float floor_y = GetFloorHeight(g_BallPosition.x, g_BallPosition.z);
            
            bool on_ramp = false;
            for (auto& f : g_LevelFloors) {
                if (g_BallPosition.x >= f.cx - f.sx && g_BallPosition.x <= f.cx + f.sx && g_BallPosition.z <= f.cz + f.sz && g_BallPosition.z >= f.cz - f.sz) {
                    if (f.pitch != 0.0f) {
                        on_ramp = true; float gravity_z = tan(f.pitch) * 9.8f; g_BallVelocity.z += gravity_z * deltaTime;
                    }
                }
            }

            float speedCheck = sqrt(g_BallVelocity.x * g_BallVelocity.x + g_BallVelocity.z * g_BallVelocity.z);
            if (!on_ramp && speedCheck < 0.07f && abs(g_BallVelocity.y) < 0.05f) { g_BallVelocity.x = 0.0f; g_BallVelocity.z = 0.0f; }

            g_BallVelocity.y -= 9.8f * deltaTime; g_BallPosition.y += g_BallVelocity.y * deltaTime;

            if (g_BallPosition.y - g_BallRadius <= floor_y) {
                g_BallPosition.y = floor_y + g_BallRadius;
                if (g_BallVelocity.y < -2.0f) g_BallVelocity.y = -g_BallVelocity.y * 0.3f; else g_BallVelocity.y = 0.0f;
            }

            // CORREÇÃO 2: Morte e Respawn ao cair nos Abismos
            if (g_BallPosition.y < -4.0f) { 
                g_BallPosition = g_SpawnPosition; g_BallVelocity = glm::vec4(0.0f); g_Strokes++; 
            }
            
            glm::vec4 nextPos = g_BallPosition; nextPos.x += g_BallVelocity.x * deltaTime; nextPos.z += g_BallVelocity.z * deltaTime;

            for (auto& w : g_Walls) {
                glm::vec2 ab = w.p2 - w.p1; glm::vec2 ac = glm::vec2(nextPos.x, nextPos.z) - w.p1;
                float t = (ac.x * ab.x + ac.y * ab.y) / (ab.x * ab.x + ab.y * ab.y); t = std::max(0.0f, std::min(t, 1.0f)); 
                glm::vec2 closest = w.p1 + t * ab; glm::vec2 diff = glm::vec2(nextPos.x, nextPos.z) - closest;
                float dist = sqrt(diff.x * diff.x + diff.y * diff.y);
                float wall_thickness_half = 0.15f; float effective_radius = g_BallRadius + wall_thickness_half;

                if (dist < effective_radius && dist > 0.00001f) {
                    float wall_floor = GetFloorHeight(closest.x, closest.y);
                    if (g_BallPosition.y + g_BallRadius >= wall_floor && g_BallPosition.y - g_BallRadius <= wall_floor + 0.45f) {
                        glm::vec2 n(diff.x / dist, diff.y / dist);
                        nextPos.x = closest.x + n.x * effective_radius; nextPos.z = closest.y + n.y * effective_radius;
                        glm::vec2 v(g_BallVelocity.x, g_BallVelocity.z); float v_dot_n = v.x * n.x + v.y * n.y;
                        if (v_dot_n < 0.0f) {
                            v.x = v.x - 1.8f * v_dot_n * n.x; v.y = v.y - 1.8f * v_dot_n * n.y; 
                            g_BallVelocity.x = v.x; g_BallVelocity.z = v.y;

                            // Toca o Som da Parede apenas se o impacto for forte o suficiente
                            if (g_AudioOK && abs(v_dot_n) > 0.3f) {
                                ma_sound_seek_to_pcm_frame(&g_SndQuique, 0); // Reseta
                                ma_sound_start(&g_SndQuique);
                            }
                        }
                    }
                }
            }

            glm::vec2 to_pole(nextPos.x - g_HolePosition.x, nextPos.z - g_HolePosition.z);
            float dist_p = sqrt(to_pole.x*to_pole.x + to_pole.y*to_pole.y); float pole_radius = 0.015f; 
            
            if (dist_p < g_BallRadius + pole_radius && g_BallPosition.y > floor_y - 0.1f) {
                glm::vec2 n = to_pole / dist_p;
                nextPos.x += n.x * ((g_BallRadius + pole_radius) - dist_p); nextPos.z += n.y * ((g_BallRadius + pole_radius) - dist_p);
                float dot_v_n = g_BallVelocity.x * n.x + g_BallVelocity.z * n.y;
                if (dot_v_n < 0.0f) { g_BallVelocity.x -= 1.5f * dot_v_n * n.x; g_BallVelocity.z -= 1.5f * dot_v_n * n.y; }
            }

            glm::vec3 movement(nextPos.x - g_BallPosition.x, 0.0f, nextPos.z - g_BallPosition.z);
            float dist_moved = sqrt(movement.x * movement.x + movement.z * movement.z);
            if (dist_moved > 0.0001f) {
                glm::vec3 move_dir(movement.x / dist_moved, 0.0f, movement.z / dist_moved);
                glm::vec3 roll_axis(move_dir.z, 0.0f, -move_dir.x); float angle = dist_moved / g_BallRadius; 
                g_BallRotationMatrix = Matrix_Rotate_Axis(angle, roll_axis) * g_BallRotationMatrix;
            }

            g_BallPosition.x = nextPos.x; g_BallPosition.z = nextPos.z;
            float h_dx = g_HolePosition.x - g_BallPosition.x; float h_dz = g_HolePosition.z - g_BallPosition.z;
            float distToHole = sqrt(h_dx * h_dx + h_dz * h_dz); float speed = sqrt(g_BallVelocity.x * g_BallVelocity.x + g_BallVelocity.z * g_BallVelocity.z);
            
            if (distToHole < g_HoleRadius * 1.1f && g_BallPosition.y <= floor_y + 0.15f) {
                if (speed < 5.5f && !g_BallInHole) {
                    g_BallInHole = true; g_HoleCooldown = 2.5f;

                    // --- TOCA O SOM DA BOLA CAINDO ---
                    if (g_AudioOK) {
                        ma_sound_seek_to_pcm_frame(&g_SndCaindo, 0);
                        ma_sound_start(&g_SndCaindo);
                    }

                    g_BallVelocity.y = -2.5f; g_BallVelocity.x *= 0.05f; g_BallVelocity.z *= 0.05f;
                    g_ScoreHistory.push_back(g_Strokes);
                    int MAP_PAR = (g_CurrentLevel == 1) ? 4 : (g_CurrentLevel == 2 ? 3 : (g_CurrentLevel == 3 ? 5 : 4)); 
                    g_ParHistory.push_back(MAP_PAR);
                } else if (distToHole > 0.01f && speed >= 5.5f) {
                    g_BallVelocity.x += (h_dx / distToHole) * 1.5f * deltaTime; g_BallVelocity.z += (h_dz / distToHole) * 1.5f * deltaTime;
                }
            }
            
        } else {
            glm::vec2 toCenter = glm::vec2(g_HolePosition.x - g_BallPosition.x, g_HolePosition.z - g_BallPosition.z);
            g_BallPosition.x += toCenter.x * 5.0f * deltaTime; g_BallPosition.z += toCenter.y * 5.0f * deltaTime;
            g_BallVelocity.y -= 5.0f * deltaTime; g_BallPosition.y += g_BallVelocity.y * deltaTime;
            float floor_y = GetFloorHeight(g_HolePosition.x, g_HolePosition.z);
            if (g_BallPosition.y < floor_y - 0.25f) { g_BallPosition.y = floor_y - 0.25f; g_BallVelocity = glm::vec4(0.0f); }
            if (g_FlagHeightOffset < 0.0f) g_FlagHeightOffset += 0.5f * deltaTime; 
            if (g_HoleCooldown > 0.0f) g_HoleCooldown -= deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) LoadLevel(g_CurrentLevel);

        float ideal_r = g_CameraDistance; float cam_y = ideal_r * sin(g_CameraPhi); float cam_z = ideal_r * cos(g_CameraPhi) * cos(g_CameraTheta); float cam_x = ideal_r * cos(g_CameraPhi) * sin(g_CameraTheta);
        glm::vec4 camera_lookat  = g_BallPosition; if(g_BallInHole) camera_lookat.y = g_HolePosition.y;
        glm::vec4 ideal_cam_pos = g_BallPosition + glm::vec4(cam_x, cam_y, cam_z, 0.0f);
        
        float max_dist = ideal_r; glm::vec2 ray_origin(g_BallPosition.x, g_BallPosition.z); glm::vec2 ray_dir(ideal_cam_pos.x - g_BallPosition.x, ideal_cam_pos.z - g_BallPosition.z);
        float ray_len = sqrt(ray_dir.x * ray_dir.x + ray_dir.y * ray_dir.y);
        if (ray_len > 0.001f) { ray_dir.x /= ray_len; ray_dir.y /= ray_len; } else { ray_dir.x = 0.001f; ray_dir.y = 0.001f; }
            
        for (auto& w : g_Walls) {
            glm::vec2 v1 = ray_origin - w.p1; glm::vec2 v2 = w.p2 - w.p1; glm::vec2 v3 = glm::vec2(-ray_dir.y, ray_dir.x);
            float dot_v2_v3 = v2.x * v3.x + v2.y * v3.y;
            if (std::abs(dot_v2_v3) < 0.00001f) continue;
            float t1 = (v2.x * v1.y - v2.y * v1.x) / dot_v2_v3; float t2 = (v1.x * v3.x + v1.y * v3.y) / dot_v2_v3;
            if (t1 >= 0.0f && t1 <= max_dist && t2 >= 0.0f && t2 <= 1.0f) {
                float v2_len = sqrt(v2.x * v2.x + v2.y * v2.y);
                if (v2_len > 0.0001f) {
                    glm::vec2 wall_dir(v2.x / v2_len, v2.y / v2_len); glm::vec2 outward_normal(-wall_dir.y, wall_dir.x);
                    float approach_angle = ray_dir.x * outward_normal.x + ray_dir.y * outward_normal.y; float margin = 0.45f; 
                    if (approach_angle < 0.0f) margin += 0.30f / std::max(0.2f, std::abs(approach_angle));
                    if (t1 - margin < max_dist) max_dist = t1 - margin; 
                }
            }
        }
        
        float actual_dist = std::max(0.12f, max_dist); glm::vec4 final_cam_pos = g_BallPosition;
        final_cam_pos.x += ray_dir.x * actual_dist; final_cam_pos.y += std::max(0.10f, cam_y * (actual_dist / ideal_r)); final_cam_pos.z += ray_dir.y * actual_dist;
        glm::mat4 view = Matrix_Camera_View(final_cam_pos, camera_lookat - final_cam_pos, glm::vec4(0.0f,1.0f,0.0f,0.0f)); glm::mat4 projection = Matrix_Perspective(g_CameraFOV, g_ScreenRatio, -0.1f, -50.0f);

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glUseProgram(g_GpuProgramID);

        glUniform4fv(g_ball_position_uniform, 1, glm::value_ptr(g_BallPosition));
        glUniform4fv(g_hole_position_uniform, 1, glm::value_ptr(g_HolePosition)); 
        glUniformMatrix4fv(g_view_uniform, 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform, 1 , GL_FALSE , glm::value_ptr(projection));

        glUniform1i(g_object_id_uniform, FLOOR);
        glm::mat4 model;
        for (auto& f : g_LevelFloors) {
            model = Matrix_Translate(f.cx, f.y, f.cz) * Matrix_Rotate_X(f.pitch) * Matrix_Scale(f.sx, 1.0f, f.sz);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model)); DrawVirtualObject("the_plane");
        }

        glUniform1i(g_object_id_uniform, HILL);
        for (auto& h : g_LevelHills) {
            float base_y = GetFloorHeight(h.x, h.z) - h.height; 
            model = Matrix_Translate(h.x, base_y - 0.05f, h.z) * Matrix_Scale(h.radius, h.height, h.radius);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model)); DrawVirtualObject("the_sphere"); 
        }

        glUniform1i(g_object_id_uniform, WALL);
        glDisable(GL_CULL_FACE); 
        glm::vec3 cube_pivot(0.0f); glm::vec3 cube_size(1.0f);
        if(g_VirtualScene.count("Cube")) {
            glm::vec3 b_min = g_VirtualScene["Cube"].bbox_min; glm::vec3 b_max = g_VirtualScene["Cube"].bbox_max;
            cube_pivot = (b_min + b_max) / 2.0f; cube_size = b_max - b_min;
            if(cube_size.x == 0) cube_size.x = 1.0f; if(cube_size.y == 0) cube_size.y = 1.0f; if(cube_size.z == 0) cube_size.z = 1.0f;
        }

        // RENDERIZAÇÃO DAS PAREDES INCLINADAS
        int wall_index = 0;
        for (auto& w : g_Walls) {
            wall_index++;
            float w_dx = w.p2.x - w.p1.x; float w_dy = w.p2.y - w.p1.y;
            float wall_len = sqrt(w_dx * w_dx + w_dy * w_dy);
            
            float cx = (w.p1.x + w.p2.x) * 0.5f; float cz = (w.p1.y + w.p2.y) * 0.5f; 
            float angle_y = atan2(w.p1.x - w.p2.x, w.p1.y - w.p2.y);
            
            float shrink = (wall_index % 2 == 0) ? 0.005f : 0.0f; 
            float height = 0.35f - shrink; float thickness = 0.30f - shrink; float final_len = wall_len + 0.30f;

            float y1 = GetFloorHeight(w.p1.x, w.p1.y); float y2 = GetFloorHeight(w.p2.x, w.p2.y);
            if (y1 <= -5.0f) y1 = GetFloorHeight(cx, cz); if (y2 <= -5.0f) y2 = GetFloorHeight(cx, cz);
            if (y1 <= -5.0f) y1 = -0.2f; if (y2 <= -5.0f) y2 = -0.2f;

            float pitch_x = atan2(y2 - y1, wall_len);
            float base_y = (y1 + y2) * 0.5f - 0.02f; float center_y = base_y + (height * 0.5f);

            model = Matrix_Translate(cx, center_y, cz) * Matrix_Rotate_Y(angle_y) * Matrix_Rotate_X(pitch_x) 
                  * Matrix_Scale(thickness / cube_size.x, height / cube_size.y, final_len / cube_size.z) * Matrix_Translate(-cube_pivot.x, -cube_pivot.y, -cube_pivot.z);
                  
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model)); DrawVirtualObject("Cube"); 
        }
        glEnable(GL_CULL_FACE);

        glUniform1i(g_object_id_uniform, HOLE);
        float local_floor = GetFloorHeight(g_HolePosition.x, g_HolePosition.z);
        model = Matrix_Translate(g_HolePosition.x, local_floor - 0.285f, g_HolePosition.z) * Matrix_Scale(g_HoleRadius, 0.3f, g_HoleRadius);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("Cylinder"); 

        float altura_base_mastro = local_floor; 
        for(auto& shape : flagModel.shapes) {
            glm::mat4 model_flag;
            if (shape.name == "object_1") glUniform1i(g_object_id_uniform, FLAG_FABRIC); else glUniform1i(g_object_id_uniform, FLAG_POLE); 
            if (shape.name == "object_3" || shape.name == "object_6") {
                model_flag = Matrix_Translate(g_HolePosition.x, altura_base_mastro, g_HolePosition.z) * Matrix_Rotate_X(-1.5708f) * Matrix_Scale(flag_scale, flag_scale, flag_scale) * Matrix_Translate(-flag_center.x, -flag_center.y, -flag_min.z);
            } else {
                model_flag = Matrix_Translate(g_HolePosition.x, altura_base_mastro + g_FlagHeightOffset, g_HolePosition.z) * Matrix_Rotate_X(-1.5708f) * Matrix_Scale(flag_scale, flag_scale, flag_scale) * Matrix_Translate(-flag_center.x, -flag_center.y, -flag_min.z);
            }
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model_flag)); DrawVirtualObject(shape.name.c_str());
        }

        glUniform1i(g_object_id_uniform, BALL);
        glUniform3fv(g_ball_color_uniform, 1, glm::value_ptr(g_BallColors[g_CurrentBallColorIndex]));
        float ball_scale = 1.0f; glm::vec3 ball_pivot(0.0f);
        if(g_VirtualScene.count("golf_ball")) {
            glm::vec3 b_min = g_VirtualScene["golf_ball"].bbox_min; glm::vec3 b_max = g_VirtualScene["golf_ball"].bbox_max;
            ball_pivot = (b_min + b_max) / 2.0f; float max_dim = std::max({b_max.x-b_min.x, b_max.y-b_min.y, b_max.z-b_min.z});
            if (max_dim > 0) ball_scale = (g_BallRadius * 1.5f) / max_dim;
        }

        model = Matrix_Translate(g_BallPosition.x, g_BallPosition.y, g_BallPosition.z) * g_BallRotationMatrix * Matrix_Scale(ball_scale, ball_scale, ball_scale) * Matrix_Translate(-ball_pivot.x, -ball_pivot.y, -ball_pivot.z);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("golf_ball");

        glUniform1i(g_object_id_uniform, CLUB);
        if (!g_BallInHole && (currentSpeed == 0.0f || g_IsSwinging) && abs(g_BallVelocity.y) < 0.05f) {
            float club_scale = 1.0f; glm::vec3 pivot(0.0f);
            if(g_VirtualScene.count("rdmobj00")) {
                glm::vec3 c_min = g_VirtualScene["rdmobj00"].bbox_min; glm::vec3 c_max = g_VirtualScene["rdmobj00"].bbox_max;
                glm::vec3 sz = c_max - c_min;
                if (std::max({sz.x, sz.y, sz.z}) > 0) club_scale = 1.2f / std::max({sz.x, sz.y, sz.z});
                pivot = (c_min + c_max) / 2.0f; pivot.y = c_min.y; pivot.z = c_max.z; 
            }
            float back_x = sin(g_CameraTheta); float back_z = cos(g_CameraTheta);
            float recuo = g_BallRadius + 0.05f + (g_ShotIntensity / MAX_INTENSITY) * 0.8f;
            glm::vec4 clubPos = g_BallPosition + glm::vec4(back_x, 0.0f, back_z, 0.0f) * recuo;
            clubPos.y = g_BallPosition.y + 1.125f;

            model = Matrix_Translate(clubPos.x, clubPos.y, clubPos.z) * Matrix_Rotate_Y(g_CameraTheta) * Matrix_Rotate_X(-1.5708f) * Matrix_Scale(club_scale, club_scale, club_scale) * Matrix_Translate(-pivot.x, -pivot.y, -pivot.z); 
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); DrawVirtualObject("rdmobj00"); glDisable(GL_BLEND);
        }

        if (g_IsCharging) {
            glDisable(GL_DEPTH_TEST);
            glm::mat4 hud_view = Matrix_Identity(); glm::mat4 hud_proj = Matrix_Identity();
            glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(hud_view));
            glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(hud_proj));
            glUniform1i(g_object_id_uniform, HUD_BAR); 
            
            glUniform3fv(g_ball_color_uniform, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.2f)));
            glm::mat4 model_bg = Matrix_Translate(0.0f, -0.75f, 0.0f) * Matrix_Rotate_X(1.5708f) * Matrix_Scale(0.5f, 1.0f, 0.03f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model_bg)); DrawVirtualObject("the_plane");
            
            float power_ratio = g_ShotIntensity / MAX_INTENSITY;
            glm::vec3 bar_color = glm::vec3(power_ratio, 1.0f - power_ratio, 0.0f);
            glUniform3fv(g_ball_color_uniform, 1, glm::value_ptr(bar_color));
            
            float fg_scale = 0.5f * power_ratio; float fg_center = -0.5f + fg_scale;
            glm::mat4 model_fg = Matrix_Translate(fg_center, -0.75f, 0.01f) * Matrix_Rotate_X(1.5708f) * Matrix_Scale(fg_scale, 1.0f, 0.03f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model_fg)); DrawVirtualObject("the_plane");
            glEnable(GL_DEPTH_TEST);

            std::string bar_str = "FORCA: " + std::to_string((int)(power_ratio * 100)) + "%";
            PrintBold(window, bar_str, -0.12f, -0.7f, 1.5f);
        }

        if (g_BallInHole) {
            if (g_HoleCooldown > 0.0f) {
                PrintBold(window, "BOLA NA CACAPA!", -0.3f, 0.0f, 2.5f);
            } else {
                glDisable(GL_DEPTH_TEST); 
                glm::mat4 hud_view = Matrix_Identity(); glm::mat4 hud_proj = Matrix_Identity();
                glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(hud_view));
                glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(hud_proj));
                glm::vec4 hud_light = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f);
                glUniform4fv(g_ball_position_uniform, 1, glm::value_ptr(hud_light));
                glUniform1i(g_object_id_uniform, HUD_BAR); 
                glUniform3fv(g_ball_color_uniform, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
                glm::mat4 model_hud = Matrix_Translate(0.0f, 0.0f, 0.0f) * Matrix_Rotate_X(1.5708f) * Matrix_Scale(1.4f, 1.0f, 1.0f); 
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model_hud)); DrawVirtualObject("the_plane");
                glEnable(GL_DEPTH_TEST); 

                PrintBold(window, "RANKING DO CAMPEONATO", -0.45f, 0.7f, 2.5f);
                
                float start_y = 0.4f;
                int total_strokes = 0;
                int total_par = 0;

                // Cabeçalho da Tabela
                PrintBold(window, "BURACO | PAR | TACADAS | +/-", -0.6f, start_y + 0.15f, 1.5f);

                for (size_t i = 0; i < g_ScoreHistory.size(); i++) {
                    int strokes = g_ScoreHistory[i];
                    int par = g_ParHistory[i];
                    int diff = strokes - par; // Diferencial: +/- em relação ao Par
                    
                    char buf[128]; 
                    snprintf(buf, 128, "   %02d  |  %d  |    %d    |  %+d", (int)(i+1), par, strokes, diff);
                    PrintBold(window, buf, -0.6f, start_y - (i * 0.15f), 1.5f);
                    
                    total_strokes += strokes;
                    total_par += par;
                }
                
                // Total acumulado
                char buf_tot[128]; 
                int total_diff = total_strokes - total_par;
                snprintf(buf_tot, 128, "TOTAL: %d TACADAS (PAR %d) | DIF: %+d", total_strokes, total_par, total_diff);
                PrintBold(window, buf_tot, -0.4f, start_y - (g_ScoreHistory.size() * 0.15f) - 0.1f, 1.8f);
                PrintBold(window, "[ ENTER ] PARA CONTINUAR", -0.35f, -0.8f, 1.5f);

                if (menuInputCooldown <= 0.0f && glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
                    menuInputCooldown = 0.5f;
                    if (g_IsRandomMode) {
                        if (g_RandomModeCurrentHole >= g_RandomModeTotalHoles) { g_IsRandomMode = false; g_CurrentLevel = 0; } 
                        else { g_RandomModeCurrentHole++; g_CurrentLevel = 4; LoadLevel(g_CurrentLevel); }
                    } else {
                        if (g_CurrentLevel == 3) { g_CampaignFinished = true; g_CurrentLevel = 0; } 
                        else { g_CurrentLevel++; LoadLevel(g_CurrentLevel); }
                    }
                }
            }
        }

        if (!g_BallInHole) {
            int MAP_PAR = (g_CurrentLevel == 1) ? 4 : (g_CurrentLevel == 2 ? 3 : (g_CurrentLevel == 3 ? 5 : 4)); 
            char txtInfo[60]; 
            if (g_IsRandomMode) snprintf(txtInfo, 60, "PISTA: RANDOM %02d/%02d  |  PAR: %d", g_RandomModeCurrentHole, g_RandomModeTotalHoles, MAP_PAR);
            else snprintf(txtInfo, 60, "PISTA: BURACO 0%d  |  PAR: %d", g_CurrentLevel, MAP_PAR);
            PrintBold(window, txtInfo, -0.95f, 0.92f, 1.3f);
            char txtStrokes[40]; snprintf(txtStrokes, 40, "TACADAS TOTAIS: %d", g_Strokes);
            PrintBold(window, txtStrokes, -0.95f, 0.78f, 1.8f); 
        }

        TextRendering_ShowFramesPerSecond(window);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Loop principal da Janela estava AQUI em cima. Isso é para descarregar a memória no final!
    if (g_AudioOK) {
        ma_sound_uninit(&g_SndTacada);
        ma_sound_uninit(&g_SndQuique);
        ma_sound_uninit(&g_SndCaindo); 
        ma_sound_uninit(&g_MusicBGM);
        ma_engine_uninit(&g_AudioEngine);
    }
    
    glfwTerminate();
    return 0;
}

void LoadTextureImage(const char* filename) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);
    if ( data == NULL ) { fprintf(stderr, "ERRO: Nao foi possivel abrir a imagem \"%s\".\n", filename); return; }
    GLuint texture_id; GLuint sampler_id;
    glGenTextures(1, &texture_id); glGenSamplers(1, &sampler_id);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_REPEAT); glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); GLuint textureunit = g_NumLoadedTextures;
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
    g_model_uniform = glGetUniformLocation(g_GpuProgramID, "model"); g_view_uniform = glGetUniformLocation(g_GpuProgramID, "view"); 
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); g_object_id_uniform = glGetUniformLocation(g_GpuProgramID, "object_id"); 
    g_bbox_min_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_min"); g_bbox_max_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    g_ball_position_uniform = glGetUniformLocation(g_GpuProgramID, "ball_position");
    g_ball_color_uniform = glGetUniformLocation(g_GpuProgramID, "ball_color"); 
    g_hole_position_uniform = glGetUniformLocation(g_GpuProgramID, "hole_position"); 
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0); glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2); glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage3"), 3);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage4"), 4); glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage5"), 5);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage6"), 6); glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage7"), 7);
    glUseProgram(0);
}

void PushMatrix(glm::mat4 M) { g_MatrixStack.push(M); }
void PopMatrix(glm::mat4& M) { if ( g_MatrixStack.empty() ) M = Matrix_Identity(); else { M = g_MatrixStack.top(); g_MatrixStack.pop(); } }

void ComputeNormals(ObjModel* model) {
    if ( !model->attrib.normals.empty() ) return;
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        for (size_t triangle = 0; triangle < model->shapes[shape].mesh.num_face_vertices.size(); ++triangle)
            sgroup_ids.insert(model->shapes[shape].mesh.smoothing_group_ids[triangle]);
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
        g_CameraTheta -= g_CameraSensitivity * (float)(xpos - g_LastCursorPosX); 
        g_CameraPhi += g_CameraSensitivity * (float)(ypos - g_LastCursorPosY); 
        float phimax = 1.56f; if (g_CameraPhi > phimax) g_CameraPhi = phimax; if (g_CameraPhi < 0.05f) g_CameraPhi = 0.05f; 
        g_LastCursorPosX = xpos; g_LastCursorPosY = ypos; 
    } 
}