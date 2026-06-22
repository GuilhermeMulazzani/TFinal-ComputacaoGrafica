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

uniform vec4 bbox_min;
uniform vec4 bbox_max;

uniform sampler2D TextureImage0; // Parede
uniform sampler2D TextureImage1; // Chão
uniform sampler2D TextureImage2; // Bola
uniform sampler2D TextureImage3; // Taco
uniform sampler2D TextureImage4; // Buraco (Pedras/Terra)
uniform sampler2D TextureImage5; // Tecido Bandeira
uniform sampler2D TextureImage6; // Mastro (Metal)

out vec4 color;

void main()
{
    // CORTA O CABO DO TACO
    if (object_id == CLUB && position_world.y > 0.02) 
    {
        discard; 
    }
    
    // corta chao
    if (object_id == FLOOR) {
        float dist = distance(position_world.xz, vec2(-4.0, -17.5)); // Coordenadas de g_HolePosition
        if (dist <= 0.10) { // 0.35 é o exato g_HoleRadius
            discard;
        }
    }

    // CORTA O EXCESSO DA BOCA DO CILINDRO NO NÍVEL DO CHÃO
    if (object_id == HOLE && position_world.y > -0.20)
    {
        discard;
    }

    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    vec4 p = position_world;
    vec4 n = normalize(normal);
    vec4 l = normalize(vec4(1.0, 1.0, 0.5, 0.0)); 
    vec4 v = normalize(camera_position - p);      
    vec4 r = -l + 2.0 * dot(n, l) * n;            

    float U = texcoords.x;
    float V = texcoords.y;

    vec3 Kd = vec3(0.0);
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
        Kd = texture(TextureImage2, vec2(U, V)).rgb;
        Ks = vec3(0.8); Ka = Kd * 0.2; q = 64.0;
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

    // MODELO DE ILUMINAÇÃO DE PHONG
    vec3 I  = vec3(1.0, 1.0, 1.0); 
    vec3 Ia = vec3(1.0, 1.0, 1.0); 

    vec3 ambient_term = Ka * Ia;
    float lambert = max(0.0, dot(n, l));
    vec3 diffuse_term = Kd * I * lambert;
    vec3 specular_term = Ks * I * pow(max(0.0, dot(r, v)), q);
    
    if (lambert <= 0.0) { specular_term = vec3(0.0); }

    color.rgb = ambient_term + diffuse_term + specular_term;
    color.a = alpha;
    color.rgb = pow(color.rgb, vec3(1.0, 1.0, 1.0) / 2.2);
}