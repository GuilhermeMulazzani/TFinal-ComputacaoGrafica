#include "collisions.h"
#include <cmath>
#include <algorithm>

bool CheckSphereWallCollision(glm::vec4 sphereCenter, float radius, glm::vec2 p1, glm::vec2 p2, glm::vec4& outNormal, float& penetration, glm::vec2& closestPoint) {
    glm::vec2 ab = p2 - p1;
    glm::vec2 ac = glm::vec2(sphereCenter.x, sphereCenter.z) - p1;
    
    // Projeta AC sobre AB e encontra o fator t [0, 1]
    float t = (ac.x * ab.x + ac.y * ab.y) / (ab.x * ab.x + ab.y * ab.y);
    t = std::max(0.0f, std::min(t, 1.0f));
    
    closestPoint = p1 + t * ab;
    glm::vec2 diff = glm::vec2(sphereCenter.x, sphereCenter.z) - closestPoint;
    float dist = sqrt(diff.x * diff.x + diff.y * diff.y);
    
    if (dist < radius && dist > 0.00001f) {
        outNormal = glm::vec4(diff.x / dist, 0.0f, diff.y / dist, 0.0f);
        penetration = radius - dist;
        return true;
    }
    return false;
}

bool CheckSpherePointCollision(glm::vec4 sphereCenter, float sphereRadius, glm::vec2 pointPos, float pointRadius, glm::vec4& outNormal, float& penetration) {
    glm::vec2 diff = glm::vec2(sphereCenter.x, sphereCenter.z) - pointPos;
    float dist = sqrt(diff.x * diff.x + diff.y * diff.y);
    
    if (dist < (sphereRadius + pointRadius) && dist > 0.00001f) {
        outNormal = glm::vec4(diff.x / dist, 0.0f, diff.y / dist, 0.0f);
        penetration = (sphereRadius + pointRadius) - dist;
        return true;
    }
    return false;
}

bool CheckRaySegmentIntersection(glm::vec2 rayOrigin, glm::vec2 rayDir, glm::vec2 p1, glm::vec2 p2, float& outT) {
    glm::vec2 v1 = rayOrigin - p1;
    glm::vec2 v2 = p2 - p1;
    glm::vec2 v3 = glm::vec2(-rayDir.y, rayDir.x);
    
    float dot_v2_v3 = v2.x * v3.x + v2.y * v3.y;
    if (std::abs(dot_v2_v3) < 0.00001f) return false;
    
    float t1 = (v2.x * v1.y - v2.y * v1.x) / dot_v2_v3; // T do raio
    float t2 = (v1.x * v3.x + v1.y * v3.y) / dot_v2_v3; // T do segmento de reta
    
    if (t1 >= 0.0f && t2 >= 0.0f && t2 <= 1.0f) {
        outT = t1;
        return true;
    }
    return false;
}