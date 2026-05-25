#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
in vec4 position_world;
in vec4 normal;
in vec4 position_model;
in vec2 texcoords;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Identificadores alinhados com o main.cpp do Mini Golf
#define WALL  0
#define FLOOR 1
#define BALL  2
#define CLUB  3
uniform int object_id;

// Parâmetros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// Variáveis para acesso das imagens de textura
uniform sampler2D TextureImage0; // Parede (Tijolo)
uniform sampler2D TextureImage1; // Chão (Pedra/Relva)
uniform sampler2D TextureImage2; // Textura da Bola
uniform sampler2D TextureImage3; // Textura do Taco

// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec4 color;

void main()
{
    // Obtemos a posição da câmera
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    vec4 p = position_world;
    vec4 n = normalize(normal);
    vec4 l = normalize(vec4(1.0, 1.0, 0.0, 0.0)); // Luz direcional
    vec4 v = normalize(camera_position - p);

    // Coordenadas de textura extraídas nativamente do arquivo .OBJ
    float U = texcoords.x;
    float V = texcoords.y;

    // Coeficiente de refletância difusa
    vec3 Kd0 = vec3(0.0, 0.0, 0.0);

    if ( object_id == WALL )
    {
        // Para a parede, se o plano não tiver UV perfeito, pode multiplicar U e V por um valor
        // escalar para fazer a textura repetir (tiling), ex: vec2(U*3.0, V*1.0)
        Kd0 = texture(TextureImage0, vec2(U, V)).rgb;
    }
    else if ( object_id == FLOOR )
    {
        Kd0 = texture(TextureImage1, vec2(U, V)).rgb;
    }
    else if ( object_id == BALL )
    {
        Kd0 = texture(TextureImage2, vec2(U, V)).rgb;
    }
    else if ( object_id == CLUB )
    {
        Kd0 = texture(TextureImage3, vec2(U, V)).rgb;
    }

    // Equação de Iluminação Lambertiana simples
    float lambert = max(0.0, dot(n, l));

    // Cor final afetada pela luz
    color.rgb = Kd0 * (lambert + 0.15); // +0.15 é a luz ambiente (para não ficar 100% escuro na sombra)
    color.a = 1.0;

    // Correção Gamma
    color.rgb = pow(color.rgb, vec3(1.0, 1.0, 1.0) / 2.2);
}
