#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

// Estrutura unificada de parede transferida do main.cpp
struct Wall { 
    glm::vec2 p1, p2; 
};

// Teste de intersecção Esfera-Linha (Parede)
bool CheckSphereWallCollision(glm::vec4 sphereCenter, float radius, glm::vec2 p1, glm::vec2 p2, glm::vec4& outNormal, float& penetration, glm::vec2& closestPoint);

// Teste de intersecção Esfera-Ponto (Buraco/Mastro)
bool CheckSpherePointCollision(glm::vec4 sphereCenter, float sphereRadius, glm::vec2 pointPos, float pointRadius, glm::vec4& outNormal, float& penetration);

// Teste de intersecção Raio-Linha Segmento (Câmera)
bool CheckRaySegmentIntersection(glm::vec2 rayOrigin, glm::vec2 rayDir, glm::vec2 p1, glm::vec2 p2, float& outT);