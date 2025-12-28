#include "Collider.hpp"
#include "Heading.hpp"
#include <algorithm>
#include <cmath>

// SAT helper functions
namespace {

glm::vec2 projectOntoAxis(const std::array<glm::vec2, 4>& corners, const glm::vec2& axis) {
    float minProj = glm::dot(corners[0], axis);
    float maxProj = minProj;
    
    for (int i = 1; i < 4; ++i) {
        float proj = glm::dot(corners[i], axis);
        minProj = std::min(minProj, proj);
        maxProj = std::max(maxProj, proj);
    }
    
    return glm::vec2(minProj, maxProj);
}

bool projectionsOverlap(const glm::vec2& proj1, const glm::vec2& proj2) {
    return proj1.y >= proj2.x && proj2.y >= proj1.x;
}

} // anonymous namespace

std::array<glm::vec2, 4> Collider::getCorners() const {
    return getCornersAt(getColliderPosition(), getColliderRotation());
}

std::array<glm::vec2, 4> Collider::getCornersAt(const glm::vec3& position, float rotation) const {
    glm::vec2 size = getColliderSize();
    const float halfWidth = size.x * 0.5f;
    const float halfLength = size.y * 0.5f;
    
    glm::vec2 forward = Heading::forwardFromHeadingDeg(rotation);
    // Right is perpendicular to forward (90° clockwise)
    glm::vec2 right(forward.y, -forward.x);
    
    glm::vec2 pos2d(position.x, position.y);
    
    return {
        pos2d + forward * halfLength + right * halfWidth,   // Front-right
        pos2d + forward * halfLength - right * halfWidth,   // Front-left
        pos2d - forward * halfLength - right * halfWidth,   // Back-left
        pos2d - forward * halfLength + right * halfWidth    // Back-right
    };
}

bool Collider::checkOBBCollision(const std::array<glm::vec2, 4>& corners1,
                                  const std::array<glm::vec2, 4>& corners2) {
    // Get axes from both rectangles (4 axes total, 2 per rectangle)
    std::array<glm::vec2, 4> axes;
    
    // Axes from first rectangle (perpendicular to edges)
    glm::vec2 edge1 = corners1[1] - corners1[0];
    glm::vec2 edge2 = corners1[3] - corners1[0];
    float len1 = glm::length(edge1);
    float len2 = glm::length(edge2);
    
    // Handle degenerate cases
    if (len1 < 0.0001f || len2 < 0.0001f) {
        return false;
    }
    
    axes[0] = edge1 / len1;
    axes[1] = edge2 / len2;
    
    // Axes from second rectangle
    glm::vec2 edge3 = corners2[1] - corners2[0];
    glm::vec2 edge4 = corners2[3] - corners2[0];
    float len3 = glm::length(edge3);
    float len4 = glm::length(edge4);
    
    if (len3 < 0.0001f || len4 < 0.0001f) {
        return false;
    }
    
    axes[2] = edge3 / len3;
    axes[3] = edge4 / len4;
    
    // Check for overlap on all axes (Separating Axis Theorem)
    for (const auto& axis : axes) {
        glm::vec2 proj1 = projectOntoAxis(corners1, axis);
        glm::vec2 proj2 = projectOntoAxis(corners2, axis);
        
        if (!projectionsOverlap(proj1, proj2)) {
            return false; // Separating axis found, no collision
        }
    }
    
    return true; // No separating axis found, collision detected
}

bool Collider::checkCollisionWith(const Collider* other) const {
    if (!other || other == this || !other->isColliderActive() || !isColliderActive()) {
        return false;
    }
    
    // Quick distance check first (bounding circle)
    glm::vec3 otherPos = other->getColliderPosition();
    glm::vec3 myPos = getColliderPosition();
    glm::vec2 diff(otherPos.x - myPos.x, otherPos.y - myPos.y);
    
    glm::vec2 mySize = getColliderSize();
    glm::vec2 otherSize = other->getColliderSize();
    float maxDim1 = std::max(mySize.x, mySize.y);
    float maxDim2 = std::max(otherSize.x, otherSize.y);
    float maxDist = (maxDim1 + maxDim2) * 0.5f;
    float maxDistSq = maxDist * maxDist;
    
    if (glm::dot(diff, diff) > maxDistSq) {
        return false;
    }
    
    // Full OBB collision check
    return checkOBBCollision(getCorners(), other->getCorners());
}

bool Collider::checkCollisionAtPosition(const Collider* other, const glm::vec3& position, float rotation) const {
    if (!other || other == this || !other->isColliderActive()) {
        return false;
    }
    
    // Quick distance check first
    glm::vec3 otherPos = other->getColliderPosition();
    glm::vec2 diff(otherPos.x - position.x, otherPos.y - position.y);
    
    glm::vec2 mySize = getColliderSize();
    glm::vec2 otherSize = other->getColliderSize();
    float maxDim1 = std::max(mySize.x, mySize.y);
    float maxDim2 = std::max(otherSize.x, otherSize.y);
    float maxDist = (maxDim1 + maxDim2) * 0.5f;
    float maxDistSq = maxDist * maxDist;
    
    if (glm::dot(diff, diff) > maxDistSq) {
        return false;
    }
    
    // Full OBB collision check
    return checkOBBCollision(getCornersAt(position, rotation), other->getCorners());
}

// CollisionManager implementation

bool CollisionManager::wouldCollide(const Collider* self, const glm::vec3& position, float rotation) const {
    if (!m_callback || !self) {
        return false;
    }
    
    auto colliders = m_callback();
    for (const Collider* other : colliders) {
        if (self->checkCollisionAtPosition(other, position, rotation)) {
            return true;
        }
    }
    
    return false;
}

std::vector<const Collider*> CollisionManager::getCollisions(const Collider* self, const glm::vec3& position, float rotation) const {
    std::vector<const Collider*> results;
    
    if (!m_callback || !self) {
        return results;
    }
    
    auto colliders = m_callback();
    for (const Collider* other : colliders) {
        if (self->checkCollisionAtPosition(other, position, rotation)) {
            results.push_back(other);
        }
    }
    
    return results;
}
