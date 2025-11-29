#pragma once

#include "Vehicle.hpp"
#include "TileGrid.hpp"
#include "Camera.hpp"
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <random>

class Renderer;

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
                   const std::vector<std::unique_ptr<Vehicle>>* playerVehicles);
    void update(float deltaTime);
    void render(Renderer* renderer);
    void reset();

    // Configuration
    void setMaxTrafficVehicles(int max) { m_maxTrafficVehicles = max; }
    void setSpawnInterval(float interval) { m_spawnInterval = interval; }
    void setViewMargin(float margin) { m_viewMargin = margin; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Get traffic vehicles (for collision detection, etc.)
    const std::vector<std::unique_ptr<Vehicle>>& getTrafficVehicles() const { return m_trafficVehicles; }

private:
    TileGrid* m_tileGrid;
    Camera* m_camera;
    const std::vector<std::unique_ptr<Vehicle>>* m_playerVehicles;
    
    std::vector<std::unique_ptr<Vehicle>> m_trafficVehicles;
    std::vector<RoadSpawnPoint> m_roadSpawnPoints;
    
    // Configuration
    int m_maxTrafficVehicles;
    float m_spawnInterval;
    float m_spawnTimer;
    float m_viewMargin;  // Extra margin outside view for spawning/despawning
    bool m_enabled;
    
    // Random number generation
    std::mt19937 m_rng;
    
    // View bounds calculation
    struct ViewBounds {
        float minX, maxX;
        float minY, maxY;
    };
    
    // Helper methods
    void buildRoadSpawnPoints();
    ViewBounds calculateViewBounds() const;
    bool isInView(const glm::vec3& position, const ViewBounds& bounds) const;
    bool isInSpawnZone(const glm::vec3& position, const ViewBounds& bounds) const;
    void spawnVehicle();
    void despawnOutOfViewVehicles(const ViewBounds& bounds);
    void updateAIVehicles(float deltaTime);
    
    // Get rotation angle from traffic direction
    static float getRotationFromDirection(CarDirection dir, std::mt19937& rng);
    // Get forward vector from traffic direction
    static glm::vec2 getForwardFromDirection(CarDirection dir, float rotation);
    // Calculate target rotation based on tile direction and current heading
    float calculateTargetRotation(CarDirection tileDir, float currentRotation) const;
    // Check if position is too close to existing vehicles
    bool isTooCloseToOtherVehicles(const glm::vec3& position) const;
};
