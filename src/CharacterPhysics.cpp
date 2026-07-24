#include "CharacterPhysics.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

void CharacterPhysics::configure(TerrainTransitionCheck terrainCheck,
                                 SurfaceHeightQuery surfaceHeightQuery,
                                 ColliderCallback obstacleCallback,
                                 FallHeightQuery fallHeightQuery) {
    m_terrainCheck = std::move(terrainCheck);
    m_surfaceHeightQuery = std::move(surfaceHeightQuery);
    m_obstacleCallback = std::move(obstacleCallback);
    m_fallHeightQuery = std::move(fallHeightQuery);
}

bool CharacterPhysics::isConfigured() const {
    return m_terrainCheck && m_surfaceHeightQuery && m_obstacleCallback;
}

bool CharacterPhysics::isObstacleAt(const Collider* self,
                                    const glm::vec3& position,
                                    float rotation,
                                    const glm::vec2& collisionSize,
                                    const Collider* ignoredObstacle) const {
    if (!m_obstacleCallback) {
        return true;
    }

    const auto candidateCorners = Collider::getCornersAt(position, rotation, collisionSize);
    const float candidateRadius = glm::length(collisionSize) * 0.5f;

    for (const Collider* obstacle : m_obstacleCallback()) {
        if (!obstacle || obstacle == self || obstacle == ignoredObstacle || !obstacle->isColliderActive()) {
            continue;
        }

        const glm::vec3 obstaclePosition = obstacle->getColliderPosition();
        const glm::vec2 difference(obstaclePosition.x - position.x, obstaclePosition.y - position.y);
        const glm::vec2 obstacleSize = obstacle->getColliderSize();
        const float maxDistance = candidateRadius + glm::length(obstacleSize) * 0.5f;
        if (glm::dot(difference, difference) > maxDistance * maxDistance) {
            continue;
        }

        if (Collider::checkOBBCollision(candidateCorners, obstacle->getCorners())) {
            return true;
        }
    }

    return false;
}

bool CharacterPhysics::canMove(const Collider* self,
                               const glm::vec3& from,
                               const glm::vec3& to,
                               float rotation,
                               const glm::vec2& collisionSize,
                               const Collider* ignoredObstacle) const {
    if (!isConfigured() || !m_terrainCheck(from, to)) {
        return false;
    }
    return !isObstacleAt(self, to, rotation, collisionSize, ignoredObstacle);
}

CharacterMoveResult CharacterPhysics::move(const Collider* self,
                                           const glm::vec3& start,
                                           float rotation,
                                           const glm::vec2& collisionSize,
                                           const glm::vec2& surfaceSize,
                                           const glm::vec3& delta,
                                           CharacterMoveMode mode,
                                           const Collider* ignoredObstacle) const {
    CharacterMoveResult result;
    result.position = start;

    const glm::vec2 planarDelta(delta.x, delta.y);
    const float distance = glm::length(planarDelta);
    if (distance <= 0.0001f) {
        return result;
    }

    if (!isConfigured()) {
        result.blockedX = std::abs(delta.x) > 0.0001f;
        result.blockedY = std::abs(delta.y) > 0.0001f;
        return result;
    }

    const float minimumDimension = std::max(0.1f, std::min(collisionSize.x, collisionSize.y));
    const float maximumStepDistance = minimumDimension * 0.5f;
    const int stepCount = std::max(1, static_cast<int>(std::ceil(distance / maximumStepDistance)));
    const glm::vec3 stepDelta = delta / static_cast<float>(stepCount);

    for (int step = 0; step < stepCount; ++step) {
        const glm::vec3 stepStart = result.position;

        if (mode == CharacterMoveMode::AllOrNothing) {
            const glm::vec3 target = stepStart + stepDelta;
            if (!canMove(self, stepStart, target, rotation, collisionSize, ignoredObstacle)) {
                result.blockedX = result.blockedX || std::abs(stepDelta.x) > 0.0001f;
                result.blockedY = result.blockedY || std::abs(stepDelta.y) > 0.0001f;
                break;
            }
            result.position = target;
        } else {
            if (std::abs(stepDelta.x) > 0.0001f) {
                const glm::vec3 target = result.position + glm::vec3(stepDelta.x, 0.0f, 0.0f);
                if (canMove(self, result.position, target, rotation, collisionSize, ignoredObstacle)) {
                    result.position.x = target.x;
                } else {
                    result.blockedX = true;
                }
            }

            if (std::abs(stepDelta.y) > 0.0001f) {
                const glm::vec3 target = result.position + glm::vec3(0.0f, stepDelta.y, 0.0f);
                if (canMove(self, result.position, target, rotation, collisionSize, ignoredObstacle)) {
                    result.position.y = target.y;
                } else {
                    result.blockedY = true;
                }
            }

            if (result.position.x == stepStart.x && result.position.y == stepStart.y &&
                result.blockedX && result.blockedY) {
                break;
            }
        }

        const glm::vec2 movement(result.position.x - stepStart.x, result.position.y - stepStart.y);
        if (glm::dot(movement, movement) > 0.0000001f) {
            result.position.z =
                m_surfaceHeightQuery(result.position, surfaceSize, movement, stepStart.z);
            if (m_fallHeightQuery) {
                result.fallTiles = std::max(
                    result.fallTiles,
                    m_fallHeightQuery(result.position, stepStart.z, result.position.z));
            }
            result.moved = true;
        }
    }

    return result;
}
