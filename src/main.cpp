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

glm::mat4 Matrix_Rotate_Axis(float angle, glm::vec3 axis) {
    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0f - c;
    
    float magnitude = sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (magnitude < 0.000001f) return glm::mat4(1.0f); 
    
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
// NOVAS VARIÁVEIS DE OPÇÕES E CUSTOMIZAÇÃO
// ====================================================================
float g_CameraSensitivity = 0.01f;
float g_CameraFOV = 3.141592f / 3.0f; // 60 graus padrão
// Controle do Modo Randômico Customizado
bool g_IsRandomMode = false;
int g_RandomModeTotalHoles = 4;   // Quantidade padrão de buracos
int g_RandomModeCurrentHole = 1;  // Contador de progresso

// ====================================================================
// IDS DE OBJETOS PARA O SHADER (TORNADOS GLOBAIS)
// ====================================================================
#define WALL  0
#define FLOOR 1
#define BALL  2
#define CLUB  3
#define HOLE  4          
#define FLAG_FABRIC 5    
#define FLAG_POLE   6

glm::vec3 g_BallColors[9] = {
    glm::vec3(1.0f, 1.0f, 1.0f), // 0: Branca (Padrão)
    glm::vec3(1.0f, 0.2f, 0.2f), // 1: Vermelha
    glm::vec3(0.2f, 1.0f, 0.2f), // 2: Verde Néon
    glm::vec3(0.2f, 0.4f, 1.0f), // 3: Azul Celeste
    glm::vec3(1.0f, 0.8f, 0.1f),  // 4: Dourada
    glm::vec3(0.6f, 0.1f, 0.8f), // 5: Roxa
    glm::vec3(1.0f, 0.4f, 0.7f), // 6: Rosa
    glm::vec3(1.0f, 0.5f, 0.0f), // 7: Laranja
    glm::vec3(0.6f, 0.6f, 0.6f)  // 8: Cinza
};
int g_CurrentBallColorIndex = 0;

// ====================================================================
// FÍSICA E GESTÃO DE NÍVEIS
// ====================================================================
int g_CurrentLevel = 0; 
bool g_CampaignFinished = false; 

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
// RAIZ DO BURACO: Ajustado para 0.13f para combinar com o shader da grama e dar espaço real pra bola
float g_HoleRadius = 0.13f; 
bool g_BallInHole = false;

float g_FlagHeightOffset = -0.4f; 

struct Wall { glm::vec2 p1, p2; };
std::vector<Wall> g_Walls;

float RandomFloat(float min, float max) {
    return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/(max-min)));
}

void LoadLevel(int level) {
    g_Strokes = 0;
    g_BallVelocity = glm::vec4(0.0f);
    g_BallRotationMatrix = glm::mat4(1.0f);
    g_BallInHole = false;
    g_IsSwinging = false;
    g_ShotIntensity = 0.0f;
    g_FlagHeightOffset = -0.4f;
    
    g_CameraTheta = 0.0f;
    g_CameraPhi = 0.4f;

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
    } else if (level == 2) {
        g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f);
        g_HolePosition = glm::vec4(0.0f, -0.19f, -16.0f, 1.0f);
        g_Walls = {
            {{ 1.5f,  1.0f}, { 1.5f, -8.0f}}, 
            {{-1.5f, -8.0f}, {-1.5f,  1.0f}},
            {{ 1.5f,  1.0f}, {-1.5f,  1.0f}},
            {{ 2.0f, -11.0f}, { 2.0f, -18.0f}}, 
            {{-2.0f, -18.0f}, {-2.0f, -11.0f}},
            {{ 2.0f, -18.0f}, {-2.0f, -18.0f}}
        };
    } else if (level == 3) {
        g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f);
        g_HolePosition = glm::vec4(0.0f, -0.19f, -15.0f, 1.0f);
        g_Walls = {
            {{ 1.5f,  1.0f}, {-1.5f,  1.0f}}, 
            {{-2.0f, -10.0f}, {2.0f, -10.0f}} 
        };
    } else if (level >= 4) {
        g_BallPosition = glm::vec4(0.0f, -0.12f, 0.0f, 1.0f);
        g_HolePosition = glm::vec4(RandomFloat(-3.5f, 3.5f), -0.19f, RandomFloat(-15.0f, -5.0f), 1.0f);
        g_Walls.clear();
        g_Walls.push_back({{ 4.5f,  1.5f}, {-4.5f,  1.5f}}); 
        g_Walls.push_back({{ 4.5f, -17.5f}, { 4.5f,  1.5f}}); 
        g_Walls.push_back({{-4.5f,  1.5f}, {-4.5f, -17.5f}}); 
        g_Walls.push_back({{-4.5f, -17.5f}, { 4.5f, -17.5f}}); 
        
        int num_obstacles = (rand() % 4) + 4; 
        for(int i = 0; i < num_obstacles; i++) {
            float ox = RandomFloat(-3.5f, 3.5f);
            float oz = RandomFloat(-14.0f, -2.0f);
            float length = RandomFloat(1.5f, 3.0f);
            if(rand() % 2 == 0) {
                g_Walls.push_back({{ox, oz}, {ox + length, oz}});
            } else {
                g_Walls.push_back({{ox, oz}, {ox, oz - length}});
            }
        }
    }
}

float GetFloorHeight(float x, float z, int level) {
    if (level == 1 || level >= 4) {
        return -0.2f;
    } else if (level == 2) {
        if (z > -5.0f) return -0.2f; 
        if (z > -8.0f) return -0.2f + ((-5.0f - z) * 0.2f); 
        if (z < -11.0f && z > -20.0f && x > -2.0f && x < 2.0f) return -0.2f; 
        return -20.0f; 
    } else if (level == 3) {
        if (x < -6.0f || x > 6.0f || z > 2.0f || z < -20.0f) return -20.0f; 
        float distToHill = sqrt(x*x + (z+7.0f)*(z+7.0f));
        if (distToHill < 3.0f) {
            return -0.2f + (cos(distToHill * (3.14159f / 3.0f)) + 1.0f) * 0.12f;
        }
        return -0.2f;
    }
    return -0.2f;
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

    // Texturas
    LoadTextureImage("../../data/madeira_textures/textures/oak_veneer_01_diff_4k.jpg");        
    LoadTextureImage("../../data/quadriculado_chao/textures/floor_tiles_06_diff_4k.jpg");            
    LoadTextureImage("../../data/golf_ball/textures/textura-golf.jpg");    
    LoadTextureImage("../../data/taco_golf/thumbnail.jpg");    
    LoadTextureImage("../../data/textura_buraco.jpg");                                
    LoadTextureImage("../../data/textura_bandeira.jpeg"); 
    LoadTextureImage("../../data/textura_metal.jpg");                                            

    ObjModel planemodel("../../data/plane.obj"); ComputeNormals(&planemodel); BuildTrianglesAndAddToVirtualScene(&planemodel);
    ObjModel spheremodel("../../data/sphere.obj"); ComputeNormals(&spheremodel); BuildTrianglesAndAddToVirtualScene(&spheremodel);
    ObjModel golfClubModel("../../data/taco_golf/model.obj"); ComputeNormals(&golfClubModel); BuildTrianglesAndAddToVirtualScene(&golfClubModel);
    ObjModel golfBallModel("../../data/golf_ball/golf_ball.obj"); ComputeNormals(&golfBallModel); BuildTrianglesAndAddToVirtualScene(&golfBallModel);
    ObjModel flagModel("../../data/bandeira_brasil.obj"); ComputeNormals(&flagModel); BuildTrianglesAndAddToVirtualScene(&flagModel);
    ObjModel blockModel("../../data/retangulo.obj"); ComputeNormals(&blockModel); BuildTrianglesAndAddToVirtualScene(&blockModel);
    ObjModel holeModel("../../data/buraco.obj"); ComputeNormals(&holeModel); BuildTrianglesAndAddToVirtualScene(&holeModel);

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

    float menuInputCooldown = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrameTime = (float)glfwGetTime();
        static float lastFrameTime = 0.0f;
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        
        if (menuInputCooldown > 0.0f) menuInputCooldown -= deltaTime;

        auto PrintBold = [&](GLFWwindow* win, const std::string& text, float x, float y, float scale) {
            float old_scale = textscale; textscale = scale; float offset = 0.0015f;
            TextRendering_PrintString(win, text, x, y, scale); TextRendering_PrintString(win, text, x + offset, y, scale); 
            TextRendering_PrintString(win, text, x, y + offset, scale); TextRendering_PrintString(win, text, x + offset, y + offset, scale); 
            textscale = old_scale; 
        };

        // ==========================================
        // RENDERIZAÇÃO DO MENU PRINCIPAL / OPÇÕES
        // ==========================================
        if (g_CurrentLevel <= 0) {
            glClearColor(0.05f, 0.15f, 0.10f, 1.0f); 
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            glm::mat4 hud_view = Matrix_Identity(); glm::mat4 hud_proj = Matrix_Identity();
            glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(hud_view));
            glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(hud_proj));

            if (g_CurrentLevel == 0) {
                PrintBold(window, "MINI GOLF IT AGAIN!", -0.6f, 0.5f, 3.0f);
                PrintBold(window, "[ 1 ] - JOGAR", -0.2f, 0.15f, 2.0f);
                PrintBold(window, "[ 2 ] - OPCOES", -0.2f, -0.05f, 2.0f);
                PrintBold(window, "[ 3 ] - PERSONALIZAR BOLA", -0.4f, -0.25f, 2.0f);
                
                if (g_CampaignFinished) {
                    PrintBold(window, "- MODO INFINITO (RANDOM) DESBLOQUEADO -", -0.55f, -0.5f, 1.5f);
                }

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f;
                        if (g_CampaignFinished) {
                            g_CurrentLevel = -3; // Abre o menu de modos
                        } else {
                            g_IsRandomMode = false;
                            g_CurrentLevel = 1;  // Vai direto para o modo clássico
                            LoadLevel(g_CurrentLevel);
                        }
                    }
                    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f; g_CurrentLevel = -1; 
                    }
                    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f; g_CurrentLevel = -2; 
                    }
                }
            } else if (g_CurrentLevel == -1) {
                PrintBold(window, "OPCOES DO SISTEMA", -0.45f, 0.6f, 3.0f);
                
                char fovStr[64]; snprintf(fovStr, 64, "FOV da Camera: %.0f graus [ Q / E ]", g_CameraFOV * (180.0f/3.14159f));
                PrintBold(window, fovStr, -0.4f, 0.2f, 1.5f);
                
                char sensStr[64]; snprintf(sensStr, 64, "Sensibilidade do Mouse: %.3f [ Z / C ]", g_CameraSensitivity);
                PrintBold(window, sensStr, -0.4f, 0.0f, 1.5f);
                
                PrintBold(window, "[ V ] - VOLTAR", -0.2f, -0.4f, 2.0f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { g_CameraFOV -= 0.02f; menuInputCooldown = 0.05f; }
                    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { g_CameraFOV += 0.02f; menuInputCooldown = 0.05f; }
                    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) { g_CameraSensitivity -= 0.001f; menuInputCooldown = 0.05f; }
                    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) { g_CameraSensitivity += 0.001f; menuInputCooldown = 0.05f; }
                    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) { g_CurrentLevel = 0; menuInputCooldown = 0.5f; }
                }
                
                // Trava os limites para não estragar a câmera
                if (g_CameraFOV < 0.5f) g_CameraFOV = 0.5f;
                if (g_CameraFOV > 2.5f) g_CameraFOV = 2.5f;
                if (g_CameraSensitivity < 0.001f) g_CameraSensitivity = 0.001f;
                if (g_CameraSensitivity > 0.05f) g_CameraSensitivity = 0.05f;

            } else if (g_CurrentLevel == -2) {
                PrintBold(window, "PERSONALIZAR BOLA", -0.45f, 0.7f, 3.0f);
                
                std::string corNome = "BRANCA";
                if(g_CurrentBallColorIndex == 1) corNome = "VERMELHA";
                else if(g_CurrentBallColorIndex == 2) corNome = "VERDE NEON";
                else if(g_CurrentBallColorIndex == 3) corNome = "AZUL CELESTE";
                else if(g_CurrentBallColorIndex == 4) corNome = "DOURADA";
                else if(g_CurrentBallColorIndex == 5) corNome = "ROXA";
                else if(g_CurrentBallColorIndex == 6) corNome = "ROSA";
                else if(g_CurrentBallColorIndex == 7) corNome = "LARANJA";
                else if(g_CurrentBallColorIndex == 8) corNome = "CINZA";

                char colorStr[64]; snprintf(colorStr, 64, "< [ A ] - COR: %s - [ D ] >", corNome.c_str());
                PrintBold(window, colorStr, -0.55f, -0.6f, 2.0f);
                PrintBold(window, "[ V ] - VOLTAR", -0.2f, -0.85f, 2.0f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                        g_CurrentBallColorIndex = (g_CurrentBallColorIndex - 1 + 9) % 9; // <-- Alterado para 9
                        menuInputCooldown = 0.2f;
                    }
                    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                        g_CurrentBallColorIndex = (g_CurrentBallColorIndex + 1) % 9; // <-- Alterado para 9
                        menuInputCooldown = 0.2f;
                    }
                    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
                        g_CurrentLevel = 0; menuInputCooldown = 0.5f;
                    }
                }

                // ==========================================
                // ESTÚDIO 3D DE PRÉ-VISUALIZAÇÃO AO VIVO
                // ==========================================
                
                // 1. REATIVAR O SHADER 3D PRINCIPAL (Isso resolve a bola invisível!)
                glUseProgram(g_GpuProgramID); 
                glEnable(GL_DEPTH_TEST);
                
                // 2. Criar uma "luz de estúdio" artificial (para a bola não ficar preta)
                glm::vec4 studio_light = glm::vec4(0.0f, 2.0f, 2.0f, 1.0f);
                glUniform4fv(g_ball_position_uniform, 1, glm::value_ptr(studio_light));

                // 3. Configurar Câmera do Menu
                glm::mat4 view = Matrix_Translate(0.0f, -0.1f, -1.2f); // Câmera estática
                glm::mat4 proj = Matrix_Perspective(g_CameraFOV, g_ScreenRatio, -0.1f, -10.0f);
                glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(proj));
                
                // 4. Aplicar ID e Cor Dinâmica
                glUniform1i(g_object_id_uniform, BALL);
                glUniform3fv(g_ball_color_uniform, 1, glm::value_ptr(g_BallColors[g_CurrentBallColorIndex]));

                // 5. Calcula o tamanho ideal para o Menu e gira a bola
                float preview_scale = 1.0f; 
                glm::vec3 preview_pivot(0.0f);
                
                if(g_VirtualScene.count("golf_ball")) {
                    glm::vec3 b_min = g_VirtualScene["golf_ball"].bbox_min; 
                    glm::vec3 b_max = g_VirtualScene["golf_ball"].bbox_max;
                    preview_pivot = (b_min + b_max) / 2.0f; 
                    float max_dim = std::max({b_max.x-b_min.x, b_max.y-b_min.y, b_max.z-b_min.z});
                    if (max_dim > 0) preview_scale = 0.4f / max_dim; // 0.4f mantém a bola num tamanho perfeito na tela
                }

                glm::mat4 model = Matrix_Rotate_Y((float)glfwGetTime() * 1.5f) 
                                * Matrix_Rotate_Z(0.3f) 
                                * Matrix_Scale(preview_scale, preview_scale, preview_scale)
                                * Matrix_Translate(-preview_pivot.x, -preview_pivot.y, -preview_pivot.z);
                
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                
                // 6. Desenhar!
                DrawVirtualObject("golf_ball");
                
                glDisable(GL_DEPTH_TEST);
                
            // === AQUI FICA A CHAVE QUE FECHA O MENU -2 CORRETAMENTE ===
            } else if (g_CurrentLevel == -3) {
                // ==========================================
                // SEGUNDO MENU: SELEÇÃO DE MODO DE JOGO
                // ==========================================
                PrintBold(window, "SELECIONE O MODO DE JOGO", -0.55f, 0.5f, 2.5f);
                PrintBold(window, "[ 1 ] - MODO CLASSICO (FASES 1-3)", -0.5f, 0.1f, 1.8f);
                PrintBold(window, "[ 2 ] - MODO RANDOMICO (INFINITO)", -0.5f, -0.1f, 1.8f);
                PrintBold(window, "[ V ] - VOLTAR", -0.2f, -0.4f, 2.0f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f;
                        g_IsRandomMode = false;
                        g_CurrentLevel = 1;
                        LoadLevel(g_CurrentLevel);
                    }
                    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f;
                        g_CurrentLevel = -4; // Vai configurar a quantidade de buracos
                    }
                    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f;
                        g_CurrentLevel = 0;  // Volta para a tela inicial
                    }
                }
            } else if (g_CurrentLevel == -4) {
                // ==========================================
                // MENU DE CONFIGURAÇÃO DE BURACOS
                // ==========================================
                PrintBold(window, "CONFIGURAR MODO RANDOMICO", -0.6f, 0.5f, 2.5f);
                
                char holesStr[64];
                snprintf(holesStr, 64, "QUANTIDADE DE BURACOS: %d", g_RandomModeTotalHoles);
                PrintBold(window, holesStr, -0.45f, 0.1f, 1.8f);
                PrintBold(window, "Pressione [ A ] para diminuir ou [ D ] para aumentar", -0.65f, -0.08f, 1.2f);
                
                PrintBold(window, "[ ENTER ] - INICIAR PARTIDA", -0.4f, -0.35f, 1.8f);
                PrintBold(window, "[ V ] - VOLTAR", -0.15f, -0.55f, 1.8f);

                if (menuInputCooldown <= 0.0f) {
                    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                        g_RandomModeTotalHoles--;
                        if (g_RandomModeTotalHoles < 1) g_RandomModeTotalHoles = 1;
                        menuInputCooldown = 0.15f;
                    }
                    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                        g_RandomModeTotalHoles++;
                        if (g_RandomModeTotalHoles > 18) g_RandomModeTotalHoles = 18; // Limite padrão de golfe
                        menuInputCooldown = 0.15f;
                    }
                    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f;
                        g_IsRandomMode = true;
                        g_RandomModeCurrentHole = 1;
                        g_CurrentLevel = 4; // Nível 4 ativa o mapa randômico procedural
                        LoadLevel(g_CurrentLevel);
                    }
                    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
                        menuInputCooldown = 0.5f;
                        g_CurrentLevel = -3; // Volta para escolha clássico/random
                    }
                }
            }
            
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue; 
        }

        // ==========================================
        // FÍSICA DO JOGO 
        // ==========================================
        if (g_CurrentLevel == 3 && g_Walls.size() >= 2) {
            float mw_x = sin(currentFrameTime * 1.5f) * 3.5f;
            g_Walls[1].p1 = {mw_x - 2.0f, -10.0f};
            g_Walls[1].p2 = {mw_x + 2.0f, -10.0f};
        }

        float currentSpeed = sqrt(g_BallVelocity.x * g_BallVelocity.x + g_BallVelocity.z * g_BallVelocity.z);

        if (!g_BallInHole && currentSpeed < 0.05f && abs(g_BallVelocity.y) < 0.05f && !g_IsSwinging) {
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
            g_BallVelocity.x -= g_BallVelocity.x * 1.0f * deltaTime; 
            g_BallVelocity.z -= g_BallVelocity.z * 1.0f * deltaTime; 
            float speedCheck = sqrt(g_BallVelocity.x * g_BallVelocity.x + g_BallVelocity.z * g_BallVelocity.z);
            if (speedCheck < 0.07f && abs(g_BallVelocity.y) < 0.05f) {
                g_BallVelocity.x = 0.0f; g_BallVelocity.z = 0.0f;
            }
            
            float floor_y = GetFloorHeight(g_BallPosition.x, g_BallPosition.z, g_CurrentLevel);
            g_BallVelocity.y -= 9.8f * deltaTime; 
            g_BallPosition.y += g_BallVelocity.y * deltaTime;

            if (g_BallPosition.y - g_BallRadius <= floor_y) {
                g_BallPosition.y = floor_y + g_BallRadius;
                if (g_BallVelocity.y < 0.0f) {
                    g_BallVelocity.y = -g_BallVelocity.y * 0.4f; 
                    if (g_BallVelocity.y < 0.3f) g_BallVelocity.y = 0.0f;
                }
            }

            if (g_CurrentLevel == 2 && g_BallPosition.z < -5.0f && g_BallPosition.z > -8.0f) {
                g_BallVelocity.z += 4.5f * deltaTime; 
            }
            if (g_CurrentLevel == 3 && floor_y > -0.2f) { 
                float dx = g_BallPosition.x; float dz = g_BallPosition.z + 7.0f;
                float dist = sqrt(dx*dx + dz*dz);
                if (dist > 0.01f) {
                    g_BallVelocity.x += (dx / dist) * 2.5f * deltaTime; 
                    g_BallVelocity.z += (dz / dist) * 2.5f * deltaTime;
                }
            }

            if (g_BallPosition.y < -5.0f) { LoadLevel(g_CurrentLevel); }

            glm::vec4 nextPos = g_BallPosition;
            nextPos.x += g_BallVelocity.x * deltaTime;
            nextPos.z += g_BallVelocity.z * deltaTime;

            // Colisão Universal: Linha de espessura (Cápsula) para Paredes e Obstáculos
            for (auto& w : g_Walls) {
                glm::vec2 ab = w.p2 - w.p1;
                glm::vec2 ac = glm::vec2(nextPos.x, nextPos.z) - w.p1;
                
                // Projeção do ponto da bola no segmento de reta (trava entre 0 e 1 para as quinas)
                float t = (ac.x * ab.x + ac.y * ab.y) / (ab.x * ab.x + ab.y * ab.y);
                t = std::max(0.0f, std::min(t, 1.0f)); 
                
                glm::vec2 closest = w.p1 + t * ab;
                glm::vec2 diff = glm::vec2(nextPos.x, nextPos.z) - closest;
                float dist = sqrt(diff.x * diff.x + diff.y * diff.y);
                
                float wall_thickness_half = 0.15f; 
                float effective_radius = g_BallRadius + wall_thickness_half;

                // Se a distância for menor que o raio da bola + metade da espessura da parede
                if (dist < effective_radius && dist > 0.00001f && g_BallPosition.y < floor_y + 0.4f) {
                    // Calcula a Normal de impacto
                    glm::vec2 n(diff.x / dist, diff.y / dist);
                    
                    // 1. RESOLUÇÃO ESTÁTICA: Empurra a bola imediatamente para fora da parede (evita engolir a bola)
                    nextPos.x = closest.x + n.x * effective_radius;
                    nextPos.z = closest.y + n.y * effective_radius;
                    
                    // 2. RESOLUÇÃO DINÂMICA: Intercepta a velocidade e faz o Quique (Bounce)
                    glm::vec2 v(g_BallVelocity.x, g_BallVelocity.z);
                    float v_dot_n = v.x * n.x + v.y * n.y;
                    
                    // Só quica se a bola estiver indo na direção da parede
                    if (v_dot_n < 0.0f) {
                        v.x = v.x - 1.8f * v_dot_n * n.x; // 1.8f é a restituição (energia mantida)
                        v.y = v.y - 1.8f * v_dot_n * n.y; 
                        g_BallVelocity.x = v.x; 
                        g_BallVelocity.z = v.y;
                    }
                }
            }

            // Colisão Exclusiva para o Mastro da Bandeira
            glm::vec2 to_pole(nextPos.x - g_HolePosition.x, nextPos.z - g_HolePosition.z);
            float dist_p = sqrt(to_pole.x*to_pole.x + to_pole.y*to_pole.y);
            float pole_radius = 0.015f; // Mastro bem afinado na física para a bola poder cair
            
            // Só colide no mastro se a bola estiver sobrevoando ou caindo, mas se estiver muito funda, ignoramos
            if (dist_p < g_BallRadius + pole_radius && g_BallPosition.y > floor_y - 0.1f) {
                glm::vec2 n = to_pole / dist_p;
                nextPos.x += n.x * ((g_BallRadius + pole_radius) - dist_p);
                nextPos.z += n.y * ((g_BallRadius + pole_radius) - dist_p);
                
                float dot_v_n = g_BallVelocity.x * n.x + g_BallVelocity.z * n.y;
                if (dot_v_n < 0.0f) {
                    g_BallVelocity.x -= 1.5f * dot_v_n * n.x; // Quique suavizado
                    g_BallVelocity.z -= 1.5f * dot_v_n * n.y;
                }
            }

            glm::vec3 movement(nextPos.x - g_BallPosition.x, 0.0f, nextPos.z - g_BallPosition.z);
            float dist_moved = sqrt(movement.x * movement.x + movement.z * movement.z);
            if (dist_moved > 0.0001f) {
                glm::vec3 move_dir(movement.x / dist_moved, 0.0f, movement.z / dist_moved);
                glm::vec3 roll_axis(move_dir.z, 0.0f, -move_dir.x); 
                float angle = dist_moved / g_BallRadius; 
                g_BallRotationMatrix = Matrix_Rotate_Axis(angle, roll_axis) * g_BallRotationMatrix;
            }

            g_BallPosition.x = nextPos.x;
            g_BallPosition.z = nextPos.z;

            // QUEDA NO BURACO (Ajustada para o novo diâmetro maior)
            float h_dx = g_HolePosition.x - g_BallPosition.x;
            float h_dz = g_HolePosition.z - g_BallPosition.z;
            float distToHole = sqrt(h_dx * h_dx + h_dz * h_dz);
            float speed = sqrt(g_BallVelocity.x * g_BallVelocity.x + g_BallVelocity.z * g_BallVelocity.z);
            float capture_radius = g_HoleRadius * 0.95f; 
            
            if (g_BallPosition.y <= floor_y + 0.1f && ((distToHole <= capture_radius && speed < 1.5f) || (distToHole <= capture_radius * 0.5f && speed < 4.0f))) {
                g_BallInHole = true; 
                g_BallVelocity.y = -1.0f; // Empurra a bola rapidamente pro fundo
                g_BallVelocity.x *= 0.3f;
                g_BallVelocity.z *= 0.3f;
            } else if (distToHole < g_HoleRadius * 1.5f && speed < 2.0f && g_BallPosition.y <= floor_y + 0.1f) {
                if (distToHole > 0.0001f) {
                    g_BallVelocity.x += (h_dx / distToHole) * 3.0f * deltaTime; // Atração gravitacional do buraco
                    g_BallVelocity.z += (h_dz / distToHole) * 3.0f * deltaTime;
                }
            }
            
        } else {
            glm::vec2 toCenter = glm::vec2(g_HolePosition.x - g_BallPosition.x, g_HolePosition.z - g_BallPosition.z);
            g_BallPosition.x += toCenter.x * 5.0f * deltaTime;
            g_BallPosition.z += toCenter.y * 5.0f * deltaTime;
            
            g_BallVelocity.y -= 5.0f * deltaTime; 
            g_BallPosition.y += g_BallVelocity.y * deltaTime;
            
            float floor_y = GetFloorHeight(g_HolePosition.x, g_HolePosition.z, g_CurrentLevel);
            if (g_BallPosition.y < floor_y - 0.25f) {       
                g_BallPosition.y = floor_y - 0.25f; // Para no fundo do buraco
                g_BallVelocity = glm::vec4(0.0f);
            }
            if (g_FlagHeightOffset < 0.0f) g_FlagHeightOffset += 0.5f * deltaTime; 
        }

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) LoadLevel(g_CurrentLevel);
        
        if (g_BallInHole && menuInputCooldown <= 0.0f && glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            menuInputCooldown = 0.5f;
            if (g_IsRandomMode) {
                // Fluxo do Modo Randômico Customizado
                if (g_RandomModeCurrentHole >= g_RandomModeTotalHoles) {
                    g_IsRandomMode = false;
                    g_CurrentLevel = 0; // Fim do campeonato, volta ao menu principal
                } else {
                    g_RandomModeCurrentHole++;
                    g_CurrentLevel = 4; // Mantém no 4 para gerar uma nova pista procedural limpa
                    LoadLevel(g_CurrentLevel); 
                }
            } else {
                // Fluxo do Modo Clássico Padrão
                if (g_CurrentLevel == 3) {
                    g_CampaignFinished = true;
                    g_CurrentLevel = 0; 
                } else {
                    g_CurrentLevel++;
                    LoadLevel(g_CurrentLevel); 
                }
            }
        }

        // ==========================================
        // CÂMERA E RENDERIZAÇÃO 3D
        // ==========================================
        float ideal_r = g_CameraDistance;
        float cam_y = ideal_r * sin(g_CameraPhi);
        float cam_z = ideal_r * cos(g_CameraPhi) * cos(g_CameraTheta);
        float cam_x = ideal_r * cos(g_CameraPhi) * sin(g_CameraTheta);

        glm::vec4 camera_lookat  = g_BallPosition; 
        if(g_BallInHole) camera_lookat.y = g_HolePosition.y;

        glm::vec4 ideal_cam_pos = g_BallPosition + glm::vec4(cam_x, cam_y, cam_z, 0.0f);
        
        float max_dist = ideal_r;
        glm::vec2 ray_origin(g_BallPosition.x, g_BallPosition.z);
        glm::vec2 ray_dir(ideal_cam_pos.x - g_BallPosition.x, ideal_cam_pos.z - g_BallPosition.z);
        
        float ray_len = sqrt(ray_dir.x * ray_dir.x + ray_dir.y * ray_dir.y);
        if (ray_len > 0.001f) {
            ray_dir.x /= ray_len; ray_dir.y /= ray_len; 
        } else {
            ray_dir.x = 0.001f; ray_dir.y = 0.001f;
        }
            
        for (auto& w : g_Walls) {
            glm::vec2 v1 = ray_origin - w.p1;
            glm::vec2 v2 = w.p2 - w.p1;
            glm::vec2 v3 = glm::vec2(-ray_dir.y, ray_dir.x);
            
            float dot_v2_v3 = v2.x * v3.x + v2.y * v3.y;
            if (std::abs(dot_v2_v3) < 0.00001f) continue;
            
            float t1 = (v2.x * v1.y - v2.y * v1.x) / dot_v2_v3;
            float t2 = (v1.x * v3.x + v1.y * v3.y) / dot_v2_v3;
            
            if (t1 >= 0.0f && t1 <= max_dist && t2 >= 0.0f && t2 <= 1.0f) {
                float v2_len = sqrt(v2.x * v2.x + v2.y * v2.y);
                if (v2_len > 0.0001f) {
                    glm::vec2 wall_dir(v2.x / v2_len, v2.y / v2_len);
                    glm::vec2 outward_normal(-wall_dir.y, wall_dir.x);
                    float approach_angle = ray_dir.x * outward_normal.x + ray_dir.y * outward_normal.y;
                    float margin = 0.45f; 
                    if (approach_angle < 0.0f) margin += 0.30f / std::max(0.2f, std::abs(approach_angle));
                    if (t1 - margin < max_dist) max_dist = t1 - margin; 
                }
            }
        }
        
        float actual_dist = std::max(0.12f, max_dist); 
        glm::vec4 final_cam_pos = g_BallPosition;
        final_cam_pos.x += ray_dir.x * actual_dist;
        final_cam_pos.y += std::max(0.10f, cam_y * (actual_dist / ideal_r)); 
        final_cam_pos.z += ray_dir.y * actual_dist;

        glm::mat4 view = Matrix_Camera_View(final_cam_pos, camera_lookat - final_cam_pos, glm::vec4(0.0f,1.0f,0.0f,0.0f));
        glm::mat4 projection = Matrix_Perspective(g_CameraFOV, g_ScreenRatio, -0.1f, -50.0f);

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Força os estados corretos para o cenário 3D, limpando vazamentos de texto/HUD
        glDisable(GL_BLEND);       // <--- ESTA LINHA MATA O BUG DA TRANSLUCIDEZ
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glUseProgram(g_GpuProgramID);

        glUniform4fv(g_ball_position_uniform, 1, glm::value_ptr(g_BallPosition));
        glUniform4fv(g_hole_position_uniform, 1, glm::value_ptr(g_HolePosition)); // <-- ADICIONAR
        glUniformMatrix4fv(g_view_uniform, 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform, 1 , GL_FALSE , glm::value_ptr(projection));

        glUniform1i(g_object_id_uniform, FLOOR);
        glm::mat4 model;
        
        if (g_CurrentLevel == 1) {
            model = Matrix_Translate(0.0f, -0.200f, -1.5f) * Matrix_Scale(1.6f, 1.0f, 2.7f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
            model = Matrix_Translate(-1.0f, -0.201f, -5.5f) * Matrix_Rotate_Y(0.588f) * Matrix_Scale(1.6f, 1.0f, 2.2f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
            model = Matrix_Translate(-2.0f, -0.202f, -9.0f) * Matrix_Scale(1.6f, 1.0f, 2.2f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
            model = Matrix_Translate(-3.0f, -0.203f, -12.5f) * Matrix_Rotate_Y(0.588f) * Matrix_Scale(1.6f, 1.0f, 2.2f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
            model = Matrix_Translate(-4.0f, -0.204f, -16.5f) * Matrix_Scale(1.6f, 1.0f, 2.7f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
        } else if (g_CurrentLevel == 2) {
            model = Matrix_Translate(0.0f, -0.20f, -2.5f) * Matrix_Scale(1.6f, 1.0f, 2.6f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
            model = Matrix_Translate(0.0f, 0.10f, -6.5f) * Matrix_Rotate_X(0.197f) * Matrix_Scale(1.6f, 1.0f, 1.6f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
            model = Matrix_Translate(0.0f, -0.20f, -14.5f) * Matrix_Scale(2.1f, 1.0f, 3.6f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
        } else if (g_CurrentLevel == 3) {
            model = Matrix_Translate(0.0f, -0.20f, -10.0f) * Matrix_Scale(6.0f, 1.0f, 12.0f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
            
            model = Matrix_Translate(0.0f, -0.32f, -7.0f) * Matrix_Scale(3.0f, 0.25f, 3.0f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); 
            // CORRIGIDO: de "sphere" para "the_sphere"
            DrawVirtualObject("the_sphere"); 
            
        } else if (g_CurrentLevel >= 4) {
            // Chão redimensionado para combinar com os limites menores da colisão!
            model = Matrix_Translate(0.0f, -0.20f, -5.5f) * Matrix_Scale(3.5f, 1.0f, 7.5f);
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("the_plane");
        }

        glUniform1i(g_object_id_uniform, WALL);
        glDisable(GL_CULL_FACE); 

        glm::vec3 cube_pivot(0.0f); glm::vec3 cube_size(1.0f);
        if(g_VirtualScene.count("Cube")) {
            glm::vec3 b_min = g_VirtualScene["Cube"].bbox_min; glm::vec3 b_max = g_VirtualScene["Cube"].bbox_max;
            cube_pivot = (b_min + b_max) / 2.0f; cube_size = b_max - b_min;
            if(cube_size.x == 0) cube_size.x = 1.0f; if(cube_size.y == 0) cube_size.y = 1.0f; if(cube_size.z == 0) cube_size.z = 1.0f;
        }

        int wall_index = 0;
        for (auto& w : g_Walls) {
            wall_index++;
            float w_dx = w.p2.x - w.p1.x; float w_dy = w.p2.y - w.p1.y;
            float wall_len = sqrt(w_dx * w_dx + w_dy * w_dy);
            
            float cx = (w.p1.x + w.p2.x) * 0.5f; float cz = (w.p1.y + w.p2.y) * 0.5f; 
            float angle = atan2(w.p1.x - w.p2.x, w.p1.y - w.p2.y);
            float shrink = (wall_index % 2 == 0) ? 0.005f : 0.0f; 
            float height = 0.30f - shrink; float thickness = 0.30f - shrink; 
            float base_y = -0.22f; float center_y = base_y + (height * 0.5f);
            float final_len = wall_len + 0.30f;

            model = Matrix_Translate(cx, center_y, cz) * Matrix_Rotate_Y(angle)
                  * Matrix_Scale(thickness / cube_size.x, height / cube_size.y, final_len / cube_size.z)
                  * Matrix_Translate(-cube_pivot.x, -cube_pivot.y, -cube_pivot.z);
                  
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            DrawVirtualObject("Cube"); 
        }
        glEnable(GL_CULL_FACE);

        // BURACO E BANDEIRA: Restaura o cilindro profundo que atravessa o buraco!
        glUniform1i(g_object_id_uniform, HOLE);
        float local_floor = GetFloorHeight(g_HolePosition.x, g_HolePosition.z, g_CurrentLevel);
        
        // Retornamos à versão original: Um cilindro profundo descendo no chão 
        model = Matrix_Translate(g_HolePosition.x, local_floor - 0.285f, g_HolePosition.z) * Matrix_Scale(g_HoleRadius, 0.3f, g_HoleRadius);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); 
        DrawVirtualObject("Cylinder"); 

        float altura_base_mastro = local_floor; 
        for(auto& shape : flagModel.shapes) {
            glm::mat4 model_flag;
            if (shape.name == "object_1") glUniform1i(g_object_id_uniform, FLAG_FABRIC); else glUniform1i(g_object_id_uniform, FLAG_POLE); 

            if (shape.name == "object_3" || shape.name == "object_6") {
                model_flag = Matrix_Translate(g_HolePosition.x, altura_base_mastro, g_HolePosition.z) * Matrix_Rotate_X(-1.5708f) 
                           * Matrix_Scale(flag_scale, flag_scale, flag_scale) * Matrix_Translate(-flag_center.x, -flag_center.y, -flag_min.z);
            } else {
                model_flag = Matrix_Translate(g_HolePosition.x, altura_base_mastro + g_FlagHeightOffset, g_HolePosition.z) * Matrix_Rotate_X(-1.5708f) 
                           * Matrix_Scale(flag_scale, flag_scale, flag_scale) * Matrix_Translate(-flag_center.x, -flag_center.y, -flag_min.z);
            }
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model_flag)); DrawVirtualObject(shape.name.c_str());
        }

        // BOLA 
        glUniform1i(g_object_id_uniform, BALL);
        glUniform3fv(g_ball_color_uniform, 1, glm::value_ptr(g_BallColors[g_CurrentBallColorIndex])); // <-- APLICANDO A COR NA PARTIDA
        float ball_scale = 1.0f; glm::vec3 ball_pivot(0.0f);
        if(g_VirtualScene.count("golf_ball")) {
            glm::vec3 b_min = g_VirtualScene["golf_ball"].bbox_min; glm::vec3 b_max = g_VirtualScene["golf_ball"].bbox_max;
            ball_pivot = (b_min + b_max) / 2.0f; float max_dim = std::max({b_max.x-b_min.x, b_max.y-b_min.y, b_max.z-b_min.z});
            if (max_dim > 0) ball_scale = (g_BallRadius * 1.5f) / max_dim;
        }

        model = Matrix_Translate(g_BallPosition.x, g_BallPosition.y, g_BallPosition.z) * g_BallRotationMatrix 
              * Matrix_Scale(ball_scale, ball_scale, ball_scale) * Matrix_Translate(-ball_pivot.x, -ball_pivot.y, -ball_pivot.z);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model)); DrawVirtualObject("golf_ball");

        // TACO 
        glUniform1i(g_object_id_uniform, CLUB);
        float clubSpeed = sqrt(g_BallVelocity.x * g_BallVelocity.x + g_BallVelocity.z * g_BallVelocity.z);
        if (!g_BallInHole && (clubSpeed == 0.0f || g_IsSwinging) && abs(g_BallVelocity.y) < 0.05f) {
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

            model = Matrix_Translate(clubPos.x, clubPos.y, clubPos.z) * Matrix_Rotate_Y(g_CameraTheta) * Matrix_Rotate_X(-1.5708f)
                  * Matrix_Scale(club_scale, club_scale, club_scale) * Matrix_Translate(-pivot.x, -pivot.y, -pivot.z); 
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            DrawVirtualObject("rdmobj00"); glDisable(GL_BLEND);
        }

        if (g_BallInHole) {
            glDisable(GL_DEPTH_TEST); 
            glm::mat4 hud_view = Matrix_Identity(); glm::mat4 hud_proj = Matrix_Identity();
            glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(hud_view));
            glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(hud_proj));
            glm::vec4 hud_light = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f);
            glUniform4fv(g_ball_position_uniform, 1, glm::value_ptr(hud_light));
            glUniform1i(g_object_id_uniform, WALL); 
            glm::mat4 model_hud = Matrix_Translate(0.0f, -0.05f, 0.0f) * Matrix_Rotate_X(1.5708f) * Matrix_Scale(1.35f, 1.0f, 0.55f); 
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model_hud)); DrawVirtualObject("the_plane");
            glEnable(GL_DEPTH_TEST); 
        }

        // HUD / TEXTOS
        int MAP_PAR = (g_CurrentLevel == 1) ? 4 : (g_CurrentLevel == 2 ? 3 : (g_CurrentLevel == 3 ? 5 : 4)); 

        char txtInfo[60]; 
        if (g_IsRandomMode) {
            snprintf(txtInfo, 60, "PISTA: RANDOM %02d/%02d  |  PAR: %d", g_RandomModeCurrentHole, g_RandomModeTotalHoles, MAP_PAR);
        } else {
            snprintf(txtInfo, 60, "PISTA: BURACO 0%d  |  PAR: %d", g_CurrentLevel, MAP_PAR);
        }
        PrintBold(window, txtInfo, -0.95f, 0.92f, 1.3f);
        char txtStrokes[40]; snprintf(txtStrokes, 40, "TACADAS TOTAIS: %d", g_Strokes);
        PrintBold(window, txtStrokes, -0.95f, 0.78f, 1.8f); 

        if (g_IsCharging) {
            int total_segments = 15; int active_segments = (int)((g_ShotIntensity / MAX_INTENSITY) * total_segments);
            std::string bar_str = "FORCA: [";
            for (int i = 0; i < total_segments; i++) bar_str += (i < active_segments) ? "I" : ".";
            bar_str += "] " + std::to_string((int)(g_ShotIntensity * 5)) + "%";
            PrintBold(window, bar_str, -0.95f, -0.85f, 1.5f);
        }

        if (g_BallInHole) {
            std::string golf_term = "";
            if (g_Strokes == 1) golf_term = "HOLE IN ONE!!!";
            else {
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
            std::string msg_vitoria = "CONCLUIDO: " + golf_term;
            PrintBold(window, msg_vitoria, -0.50f, 0.32f, 2.2f);

            PrintBold(window, "BURACO", -0.55f, 0.08f, 1.5f); PrintBold(window, "PAR", -0.25f, 0.08f, 1.5f);
            PrintBold(window, "TACADAS", 0.02f, 0.08f, 1.5f); PrintBold(window, "STATUS", 0.32f, 0.08f, 1.5f);

            char str_par[10], str_strokes[10], lvl_str[20];
            snprintf(str_par, 10, "%d", MAP_PAR); 
            snprintf(str_strokes, 10, "%d", g_Strokes); 
            
            if (g_IsRandomMode) {
                snprintf(lvl_str, 20, "RND %02d", g_RandomModeCurrentHole);
            } else {
                snprintf(lvl_str, 20, "#0%d", g_CurrentLevel);
            }
            
            PrintBold(window, lvl_str, -0.53f, -0.08f, 1.5f); PrintBold(window, str_par, -0.23f, -0.08f, 1.5f);
            PrintBold(window, str_strokes, 0.07f, -0.08f, 1.5f); PrintBold(window, golf_term, 0.32f, -0.08f, 1.5f);

            if(g_CurrentLevel == 3) {
                 PrintBold(window, "Pressione 'ENTER' para O MENU PRINCIPAL", -0.52f, -0.32f, 1.4f);
            } else {
                 PrintBold(window, "Pressione 'ENTER' para a proxima fase", -0.52f, -0.32f, 1.4f);
            }
        }

        TextRendering_ShowFramesPerSecond(window);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}

// ============================================================================
// BOILERPLATE / CÓDIGOS DE SETUP OpenGL
// ============================================================================
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
    g_ball_color_uniform = glGetUniformLocation(g_GpuProgramID, "ball_color"); // <-- ADICIONE AQUI
    g_hole_position_uniform = glGetUniformLocation(g_GpuProgramID, "hole_position"); // <-- ADICIONAR
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0); glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2); glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage3"), 3);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage4"), 4); glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage5"), 5);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage6"), 6); glUseProgram(0);
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