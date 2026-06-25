#version 330 core

in vec4 position_world;
in vec4 normal;
in vec4 position_model;
in vec2 texcoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

#define WALL  0
#define FLOOR 1
#define BALL  2
#define CLUB  3
#define HOLE  4
#define FLAG_FABRIC 5
#define FLAG_POLE   6

uniform int object_id;

// --- UNIFORMS DE POSIÇÃO DINÂMICA ---
uniform vec4 ball_position;
uniform vec4 hole_position; // <-- NOVO: Posição do buraco recebida do main.cpp

uniform vec4 bbox_min;
uniform vec4 bbox_max;

uniform sampler2D TextureImage0; // Parede
uniform sampler2D TextureImage1; // Chão
uniform sampler2D TextureImage2; // Bola
uniform sampler2D TextureImage3; // Taco
uniform sampler2D TextureImage4; // Buraco (Pedras/Terra)
uniform sampler2D TextureImage5; // Tecido Bandeira
uniform sampler2D TextureImage6; // Mastro (Metal)

uniform vec3 ball_color;

out vec4 color;

void main()
{
    // CORTA O CABO DO TACO
    if (object_id == CLUB && position_world.y > 0.02) 
    {
        discard; 
    }
    
    // CORTA CHAO (Dinâmico para todas as fases)
    if (object_id == FLOOR) {
        // Usa a coordenada real enviada pela CPU em vez de valor fixo
        float dist = distance(position_world.xz, hole_position.xz); 
        if (dist <= 0.10) { 
            discard;
        }
    }

    // CORTA O EXCESSO DA BOCA DO CILINDRO NO NÍVEL DO CHÃO
    if (object_id == HOLE && position_world.y > -0.10)
    {
        discard;
    }

    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    vec4 p = position_world;
    vec4 n = normalize(normal);
    
    // --- VETOR DA LUZ (Aponta do fragmento atual para a bola) ---
    vec4 l = normalize(ball_position - p);      
    
    vec4 v = normalize(camera_position - p);      
    vec4 r = -l + 2.0 * dot(n, l) * n;            

    float U = texcoords.x;
    float V = texcoords.y;

    vec3 Kd = vec3(0.0, 0.0, 0.0);
    vec3 Ks = vec3(0.0);
    vec3 Ka = vec3(0.0);
    float q = 1.0;       
    float alpha = 1.0;   

    // CONFIGURAÇÃO DOS MATERIAIS
    if ( object_id == WALL ) {
        Kd = texture(TextureImage0, vec2(U * 4.0, V * 1.0)).rgb;
        Ks = vec3(0.1); Ka = Kd * 0.3; q = 10.0;
    }
    else if ( object_id == FLOOR ) {
        // Puxa a textura quadriculada normal
        Kd = texture(TextureImage1, vec2(U * 8.0, V * 16.0)).rgb;
        // Aplica um tom de grama sintética verde (diminui o vermelho e o azul, destaca o verde)
        Kd = Kd * vec3(0.3, 0.85, 0.3); 
        Ks = vec3(0.05); 
        Ka = Kd * 0.3; 
        q = 5.0;
    }
    else if ( object_id == HOLE ) { 
        Kd = texture(TextureImage4, vec2(U, V * 2.0)).rgb; // Tiling vertical no tubo do buraco
        Kd = Kd * 0.45; // Escurece o fundo para dar sensação de poço profundo
        Ks = vec3(0.0); Ka = Kd * 0.2; q = 1.0;
    }
    else if ( object_id == BALL ) {
        Kd = texture(TextureImage2, texcoords).rgb * ball_color;
        Ka = Kd;
    }
    else if ( object_id == CLUB ) {
        Kd = texture(TextureImage3, vec2(U, V)).rgb;
        Ks = vec3(0.5); Ka = Kd * 0.2; q = 32.0; alpha = 0.4;
    }
    else if ( object_id == FLAG_FABRIC ) {
        Kd = texture(TextureImage5, vec2(U, V)).rgb;
        Ks = vec3(0.05); Ka = Kd * 0.4; q = 5.0;
    }
    else if ( object_id == FLAG_POLE ) {
        Kd = texture(TextureImage6, vec2(U, V)).rgb;
        Ks = vec3(0.7); Ka = Kd * 0.2; q = 64.0;
    }

    // ==========================================================
    // MODELO DE ILUMINAÇÃO (DARK MODE NEON + POINT LIGHT)
    // ==========================================================
    
    // 1. Iluminação Global Ambient (Fundo Roxo Neon)
    vec3 ambient_color = vec3(0.15, 0.05, 0.30); 
    vec3 ambient_term = Kd * ambient_color; 

    // 2. Iluminação Local Difusa e Especular (Bola emitindo luz)
    vec3 light_color = vec3(0.9, 0.7, 1.0); // Cor brilhante da luz que a bola emite
    float lambert = max(0.0, dot(n, l));
    
    // 3. Atenuação da Luz (diminui conforme a distância da bola)
    float dist_light = length(ball_position - p);
    float attenuation = 1.0 / (1.0 + 0.5 * dist_light + 0.2 * (dist_light * dist_light));

    vec3 diffuse_term = Kd * light_color * lambert * attenuation;
    vec3 specular_term = Ks * light_color * pow(max(0.0, dot(r, v)), q) * attenuation;
    
    if (lambert <= 0.0) { specular_term = vec3(0.0); }

    // Soma das contribuições de luz
    color.rgb = ambient_term + diffuse_term + specular_term;
    color.a = alpha;
    
    // 4. A própria bola brilha e ignora sombras (Neon Effect Corrigido)
    if (object_id == BALL) {
        // Pega a cor certa baseada na textura multiplicada pela cor do menu
        color.rgb = texture(TextureImage2, texcoords).rgb * ball_color;
    }

    // Correção Gamma (mantida conforme seu código original)
    color.rgb = pow(color.rgb, vec3(1.0, 1.0, 1.0) / 2.2);
}