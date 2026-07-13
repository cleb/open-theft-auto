#pragma once

#include "Collider.hpp"

#include <functional>
#include <glm/glm.hpp>

enum class CharacterMoveMode {
    Slide,
    AllOrNothing
};

struct CharacterMoveResult {
    glm::vec3 position{0.0f};
    bool moved = false;
    bool blockedX = false;
    bool blockedY = false;

    bool blocked() const { return blockedX || blockedY; }
};

class CharacterPhysics {
public:
    using TerrainTransitionCheck = std::function<bool(const glm::vec3& from, const glm::vec3& to)>;
    using SurfaceHeightQuery = std::function<float(const glm::vec3& position,
                                                   const glm::vec2& footprintSize,
                                                   const glm::vec2& movement,
                                                   float referenceZ)>;

    void configure(TerrainTransitionCheck terrainCheck,
                   SurfaceHeightQuery surfaceHeightQuery,
                   ColliderCallback obstacleCallback);

    bool isConfigured() const;

    CharacterMoveResult move(const Collider* self,
                             const glm::vec3& start,
                             float rotation,
                             const glm::vec2& collisionSize,
                             const glm::vec2& surfaceSize,
                             const glm::vec3& delta,
                             CharacterMoveMode mode,
                             const Collider* ignoredObstacle = nullptr) const;

    bool canMove(const Collider* self,
                 const glm::vec3& from,
                 const glm::vec3& to,
                 float rotation,
                 const glm::vec2& collisionSize,
                 const Collider* ignoredObstacle = nullptr) const;

    bool isObstacleAt(const Collider* self,
                      const glm::vec3& position,
                      float rotation,
                      const glm::vec2& collisionSize,
                      const Collider* ignoredObstacle = nullptr) const;

private:
    TerrainTransitionCheck m_terrainCheck;
    SurfaceHeightQuery m_surfaceHeightQuery;
    ColliderCallback m_obstacleCallback;
};
