#pragma once

#include "AutoPilot.hpp"
#include <glm/glm.hpp>
#include <functional>

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
    
    // Lane change state
    mutable bool m_pendingLaneChange = false;
    mutable glm::vec3 m_laneChangeTargetPos{0.0f};
    
    // Override heading if current direction doesn't lead to player
    bool shouldOverrideHeading(Vehicle* vehicle, TileGrid* tileGrid, float& newHeading) const;
    
    // Get available headings for a road direction
    std::vector<float> getHeadingsForRoadDirection(CarDirection roadDir) const;
    
    // Get the best heading from available road directions to reach target
    float getBestHeadingForTarget(float currentHeading, const glm::vec3& pos, const glm::vec3& targetPos, CarDirection roadDir) const;
    
    // Returns true if heading roughly points towards target position
    bool isHeadingTowardsTarget(float heading, const glm::vec3& fromPos, const glm::vec3& targetPos) const;
    
    // Try to steer around an obstacle instead of stopping
    bool trySteerAroundObstacle(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime);
    
    // Helper
    float angleDifference(float from, float to) const;
};
