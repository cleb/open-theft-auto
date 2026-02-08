#pragma once

#include "Pedestrian.hpp"
#include "SpriteAnimation.hpp"
#include "TileGrid.hpp"
#include "Camera.hpp"
#include "ViewBounds.hpp"
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <random>
#include <functional>

class Renderer;
class Vehicle;

// Callback type for getting all vehicles in the scene
using VehicleCallback = std::function<std::vector<Vehicle*>()>;

// Callback type for when a pedestrian is killed by a vehicle
// Parameters: killer vehicle (can be used to check if it's the player's vehicle)
using PedestrianKillCallback = std::function<void(Vehicle*)>;

// Structure to hold sidewalk tile information for spawning
struct SidewalkSpawnPoint {
    glm::ivec3 gridPos;
    glm::vec3 worldPos;
    SidewalkDirection direction;
};

class PedestrianManager {
public:
    PedestrianManager();
    ~PedestrianManager() = default;

    void initialize(TileGrid* tileGrid, Camera* camera);
    void update(float deltaTime);
    void render(Renderer* renderer);
    void reset();
    
    // Set projection info for accurate view bounds calculation
    void setProjectionInfo(float fovRadians, float aspectRatio);

    // Configuration
    void setMaxPedestrians(int max) { m_maxPedestrians = max; }
    void setSpawnInterval(float minInterval, float maxInterval) { 
        m_spawnIntervalMin = minInterval; 
        m_spawnIntervalMax = maxInterval;
    }
    void setViewMargin(float margin) { m_viewMargin = margin; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    // Vehicle collision callback
    void setVehicleCallback(VehicleCallback callback) { m_vehicleCallback = callback; }
    
    // Pedestrian kill callback (called when a pedestrian is killed by a vehicle)
    void setPedestrianKillCallback(PedestrianKillCallback callback) { m_pedestrianKillCallback = callback; }

    // Get pedestrians (for collision detection, etc.)
    const std::vector<std::unique_ptr<Pedestrian>>& getPedestrians() const { return m_pedestrians; }
    
    // Spawn a carjacked pedestrian at a specific position (called when player takes an autopiloted vehicle)
    void spawnCarjackedPedestrian(const glm::vec3& position, float rotation);
    
    // Get the shared animation (for external pedestrian creation)
    SpriteAnimation* getSharedAnimation() const { return m_sharedAnimation.get(); }

    void notifyGunshot(const glm::vec3& sourcePosition);

private:
    TileGrid* m_tileGrid;
    Camera* m_camera;
    
    // Shared animation for all pedestrians (loaded once)
    std::unique_ptr<SpriteAnimation> m_sharedAnimation;
    
    std::vector<std::unique_ptr<Pedestrian>> m_pedestrians;
    std::vector<SidewalkSpawnPoint> m_sidewalkSpawnPoints;
    
    // Configuration
    int m_maxPedestrians;
    float m_spawnIntervalMin;
    float m_spawnIntervalMax;
    float m_spawnTimer;
    float m_nextSpawnInterval;
    float m_viewMargin;
    bool m_enabled;
    
    // Projection info for accurate view bounds
    float m_fovRadians;
    float m_aspectRatio;
    
    // Random number generation
    std::mt19937 m_rng;
    
    // Vehicle collision callback
    VehicleCallback m_vehicleCallback;
    
    // Pedestrian kill callback
    PedestrianKillCallback m_pedestrianKillCallback;
    
    // Helper methods
    void buildSidewalkSpawnPoints();
    void spawnPedestrian();
    void despawnOutOfViewPedestrians(const ViewBounds& bounds);
    void updatePedestrians(float deltaTime);
    void checkVehicleCollisions();
    
    // Get rotation angle from sidewalk direction (random choice for bidirectional)
    float getRotationFromDirection(SidewalkDirection dir);
    // Check if position is too close to existing pedestrians
    bool isTooCloseToOthers(const glm::vec3& position) const;
    // Check if a position is blocked by any vehicle (for pedestrian avoidance)
    bool isPositionBlockedByVehicle(const glm::vec3& position, float pedRadius) const;
    // Set the vehicle block check callback on a pedestrian
    void setupVehicleBlockCheck(Pedestrian* pedestrian);
};
