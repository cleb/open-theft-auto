#pragma once

#include "Vehicle.hpp"
#include "TileGrid.hpp"
#include "Camera.hpp"
#include "Collider.hpp"
#include "ViewBounds.hpp"
#include "LevelData.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include <random>

class Renderer;
class PedestrianManager;

// Structure to hold road tile information for spawning
struct RoadSpawnPoint {
    glm::ivec3 gridPos;
    glm::vec3 worldPos;
    CarDirection direction;
    float rotationDegrees;  // Rotation for vehicle based on traffic direction
};

class TrafficManager {
public:
    TrafficManager();
    ~TrafficManager() = default;

    void initialize(TileGrid* tileGrid, Camera* camera, 
                   std::vector<std::unique_ptr<Vehicle>>* vehicles);
    void update(float deltaTime);
    void render(Renderer* renderer);
    void reset();
    
    // Set projection info for accurate view bounds calculation
    void setProjectionInfo(float fovRadians, float aspectRatio);

    // Configuration
    void setMaxTrafficVehicles(int max) { m_maxTrafficVehicles = max; }
    void setSpawnInterval(float minInterval, float maxInterval) { 
        m_spawnIntervalMin = minInterval; 
        m_spawnIntervalMax = maxInterval;
    }
    void setViewMargin(float margin) { m_viewMargin = margin; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    // Callback used to add a newly-spawned vehicle to the shared list.
    // The callback is expected to set collision/explode callbacks and tileGrid.
    using AddVehicleCallback = std::function<void(std::unique_ptr<Vehicle>)>;
    void setAddVehicleCallback(AddVehicleCallback callback) { m_addVehicleCallback = std::move(callback); }
    
    // Set pedestrian manager for carjack callbacks
    void setPedestrianManager(PedestrianManager* pedestrianManager) { m_pedestrianManager = pedestrianManager; }

    // Get road spawn points (for police spawning, etc.)
    const std::vector<RoadSpawnPoint>& getRoadSpawnPoints() const { return m_roadSpawnPoints; }
    
    // Debug rendering
    void setDebugRenderSpawnPoints(bool enabled) { m_debugRenderSpawnPoints = enabled; }
    bool isDebugRenderSpawnPointsEnabled() const { return m_debugRenderSpawnPoints; }
    void renderDebugSpawnPoints(Renderer* renderer);

private:
    TileGrid* m_tileGrid;
    Camera* m_camera;
    std::vector<std::unique_ptr<Vehicle>>* m_vehicles;   // Shared vehicle list (owned by Scene)
    PedestrianManager* m_pedestrianManager;
    
    std::vector<RoadSpawnPoint> m_roadSpawnPoints;
    AddVehicleCallback m_addVehicleCallback;
    
    // Configuration
    int m_maxTrafficVehicles;
    float m_spawnIntervalMin;
    float m_spawnIntervalMax;
    float m_spawnTimer;
    float m_nextSpawnInterval;
    float m_viewMargin;  // Extra margin outside view for spawning/despawning
    bool m_enabled;
    bool m_debugRenderSpawnPoints;  // Debug: render spawn points on map
    
    // Projection info for accurate view bounds
    float m_fovRadians;
    float m_aspectRatio;
    
    // Random number generation
    std::mt19937 m_rng;
    
    // Helper methods
    void buildRoadSpawnPoints();
    void spawnVehicle();
    void despawnOutOfViewVehicles(const ViewBounds& bounds);
    void updateTrafficVehicles(float deltaTime);
    
    // Get rotation angle from traffic direction
    static float getRotationFromDirection(CarDirection dir, std::mt19937& rng);
    // Get forward vector from traffic direction
    static glm::vec2 getForwardFromDirection(CarDirection dir, float rotation);
    // Check if position is too close to existing vehicles
    bool isTooCloseToOtherVehicles(const glm::vec3& position) const;
};
