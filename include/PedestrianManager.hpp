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

    // Get pedestrians (for collision detection, etc.)
    const std::vector<std::unique_ptr<Pedestrian>>& getPedestrians() const { return m_pedestrians; }

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
};
