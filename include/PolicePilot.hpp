#pragma once

#include "AutoPilot.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <vector>

class Vehicle;
class TileGrid;

// Callback to get the player's current position
using PlayerPositionCallback = std::function<glm::vec3()>;

// Police AI pilot that extends AutoPilot with chase behavior
// Differences from regular AutoPilot:
// 1. Chooses road direction that leads towards player
// 2. Drives around obstacles instead of waiting for them
class PolicePilot : public AutoPilot {
public:
    PolicePilot();
    ~PolicePilot() override = default;
    
    void update(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime) override;
    void onAssign(Vehicle* vehicle) override;
    void onRelease(Vehicle* vehicle) override;
    
    // Set the callback to get player position
    void setPlayerPositionCallback(PlayerPositionCallback callback) { m_playerPositionCallback = std::move(callback); }
    
private:
    PlayerPositionCallback m_playerPositionCallback;

    enum class RouteMode {
        StrictLane,
        Flexible
    };

    struct SearchResult {
        std::vector<glm::ivec3> path;
        float totalCost = 0.0f;
        bool reachedGoal = false;
    };

    std::vector<glm::ivec3> m_cachedPath;
    glm::ivec3 m_cachedGoal{-1, -1, -1};
    float m_repathCooldown = 0.0f;
    float m_detourTimer = 0.0f;

    glm::ivec3 worldToDriveGrid(const TileGrid* tileGrid, const glm::vec3& worldPos) const;
    glm::vec3 gridToDriveWorld(const TileGrid* tileGrid, const glm::ivec3& gridPos, float z) const;
    bool isRoadTile(const TileGrid* tileGrid, const glm::ivec3& gridPos) const;
    bool isMoveAllowedByDirection(CarDirection dir, const glm::ivec2& step) const;
    SearchResult findPath(const TileGrid* tileGrid, const glm::ivec3& start, const glm::ivec3& goal, RouteMode mode) const;
    float computeMoveCost(const TileGrid* tileGrid, const glm::ivec3& from, const glm::ivec3& to,
                          const glm::ivec3& goal, RouteMode mode) const;
    float heuristicCost(const glm::ivec3& from, const glm::ivec3& goal) const;
    bool wouldCollideAt(const Vehicle* vehicle, const glm::vec3& newPos, float heading) const;
    bool canMoveTo(const TileGrid* tileGrid, const glm::vec3& fromPos, const glm::vec3& toPos) const;
    float chooseDetourHeading(const Vehicle* vehicle, const TileGrid* tileGrid, const glm::vec3& pos,
                              float desiredHeading, float travelDistance, bool& foundDetour) const;
    float angleDifference(float from, float to) const;
};
