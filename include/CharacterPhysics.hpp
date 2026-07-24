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
    // Largest drop (in whole tile levels) taken during this move.
    int fallTiles = 0;

    bool blocked() const { return blockedX || blockedY; }
};

class CharacterPhysics {
public:
    using TerrainTransitionCheck = std::function<bool(const glm::vec3& from, const glm::vec3& to)>;
    using SurfaceHeightQuery = std::function<float(const glm::vec3& position,
                                                   const glm::vec2& footprintSize,
                                                   const glm::vec2& movement,
                                                   float referenceZ)>;
    // Reports how many whole tile levels the entity dropped when it moved to
    // `position` from `referenceZ` and settled at `landedZ`.
    using FallHeightQuery = std::function<int(const glm::vec3& position, float referenceZ, float landedZ)>;

    void configure(TerrainTransitionCheck terrainCheck,
                   SurfaceHeightQuery surfaceHeightQuery,
                   ColliderCallback obstacleCallback,
                   FallHeightQuery fallHeightQuery = {});

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
    FallHeightQuery m_fallHeightQuery;
    ColliderCallback m_obstacleCallback;
};
