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
#define HILL 7
#define HUD_BAR 8

uniform int object_id;

uniform vec4 ball_position;
uniform vec4 hole_position; 

uniform vec4 bbox_min;
uniform vec4 bbox_max;

uniform sampler2D TextureImage0; 
uniform sampler2D TextureImage1; 
uniform sampler2D TextureImage2; 
uniform sampler2D TextureImage3; 
uniform sampler2D TextureImage4; 
uniform sampler2D TextureImage5; 
uniform sampler2D TextureImage6; 
uniform sampler2D TextureImage7; 

uniform vec3 ball_color;
out vec4 color;

void main()
{
    if (object_id == CLUB && position_world.y > 0.02) discard; 
    
    if (object_id == HUD_BAR) {
        color.rgb = ball_color; color.a = 1.0;
        return; 
    }

    if (object_id == FLOOR) {
        float dist = distance(position_world.xz, hole_position.xz); 
        if (dist <= 0.10) discard;
    }

    if (object_id == HOLE && position_world.y > hole_position.y) discard;

    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    vec4 p = position_world;
    vec4 n = normalize(normal);
    vec4 l = normalize(ball_position - p);      
    vec4 v = normalize(camera_position - p);      
    vec4 r = -l + 2.0 * dot(n, l) * n;            

    float U = texcoords.x; float V = texcoords.y;

    vec3 Kd = vec3(0.0); vec3 Ks = vec3(0.0); vec3 Ka = vec3(0.0);
    float q = 1.0; float alpha = 1.0;   

    if ( object_id == WALL ) {
        Kd = texture(TextureImage0, vec2(U * 4.0, V * 1.0)).rgb;
        Ks = vec3(0.1); Ka = Kd * 0.3; q = 10.0;
    } else if ( object_id == FLOOR ) {
        // SOLUÇÃO 4: Textura usando Coordenadas Globais (Acaba com os esticamentos e falhas do piso)
        Kd = texture(TextureImage1, position_world.xz * 1.5).rgb;
        Kd = Kd * vec3(0.3, 0.85, 0.3); 
        Ks = vec3(0.05); Ka = Kd * 0.3; q = 5.0;
    } else if ( object_id == HILL ) {
        Kd = texture(TextureImage7, vec2(U * 3.0, V * 3.0)).rgb;
        Ks = vec3(0.05); Ka = Kd * 0.3; q = 5.0;
    } else if ( object_id == HOLE ) { 
        Kd = texture(TextureImage4, vec2(U, V * 2.0)).rgb; 
        Kd = Kd * 0.45; Ks = vec3(0.0); Ka = Kd * 0.2; q = 1.0;
    } else if ( object_id == BALL ) {
        Kd = texture(TextureImage2, texcoords).rgb * ball_color;
        Ka = Kd;
    } else if ( object_id == CLUB ) {
        Kd = texture(TextureImage3, vec2(U, V)).rgb;
        Ks = vec3(0.5); Ka = Kd * 0.2; q = 32.0; alpha = 0.4;
    } else if ( object_id == FLAG_FABRIC ) {
        Kd = texture(TextureImage5, vec2(U, V)).rgb;
        Ks = vec3(0.05); Ka = Kd * 0.4; q = 5.0;
    } else if ( object_id == FLAG_POLE ) {
        Kd = texture(TextureImage6, vec2(U, V)).rgb;
        Ks = vec3(0.7); Ka = Kd * 0.2; q = 64.0;
    }

    vec3 ambient_color = vec3(0.15, 0.05, 0.30); 
    vec3 ambient_term = Kd * ambient_color; 
    vec3 light_color = vec3(0.9, 0.7, 1.0); 
    float lambert = max(0.0, dot(n, l));
    
    float dist_light = length(ball_position - p);
    float attenuation = 1.0 / (1.0 + 0.5 * dist_light + 0.2 * (dist_light * dist_light));

    vec3 diffuse_term = Kd * light_color * lambert * attenuation;
    vec3 specular_term = Ks * light_color * pow(max(0.0, dot(r, v)), q) * attenuation;
    
    if (lambert <= 0.0) specular_term = vec3(0.0); 

    color.rgb = ambient_term + diffuse_term + specular_term; color.a = alpha;
    if (object_id == BALL) color.rgb = texture(TextureImage2, texcoords).rgb * ball_color;
    color.rgb = pow(color.rgb, vec3(1.0, 1.0, 1.0) / 2.2);
}