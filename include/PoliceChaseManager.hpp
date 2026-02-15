#pragma once

#include "Vehicle.hpp"
#include "TileGrid.hpp"
#include "Camera.hpp"
#include "ViewBounds.hpp"
#include "TrafficManager.hpp"
#include "Pedestrian.hpp"
#include "SpriteAnimation.hpp"
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <random>
#include <deque>
#include <functional>

class Renderer;
class Player;

// Callback to get the current player position
using PlayerPositionCallback = std::function<glm::vec3()>;
// Callback for police officer weapon fire
using OfficerShootCallback = std::function<void(const glm::vec3&, const glm::vec2&)>;

class PoliceChaseManager {
public:
    PoliceChaseManager();
    ~PoliceChaseManager() = default;

    void initialize(TileGrid* tileGrid, Camera* camera, Player* player, TrafficManager* trafficManager,
                    std::vector<std::unique_ptr<Vehicle>>* vehicles);
    void update(float deltaTime);
    void render(Renderer* renderer);
    void reset();
    
    // Set projection info for accurate view bounds calculation
    void setProjectionInfo(float fovRadians, float aspectRatio);
    
    // Called when a pedestrian is killed by the player
    void onPedestrianKilled();
    
    // Called when the player causes a vehicle explosion via gunfire
    void onPlayerCausedVehicleExplosion();
    
    // Set callbacks
    void setPlayerPositionCallback(PlayerPositionCallback callback) { m_playerPositionCallback = std::move(callback); }
    void setOfficerShootCallback(OfficerShootCallback callback) { m_officerShootCallback = std::move(callback); }
    void setOfficerColliderCallback(ColliderCallback callback) { m_officerColliderCallback = std::move(callback); }

    // Callback used to add a newly-spawned vehicle to the shared list.
    using AddVehicleCallback = std::function<void(std::unique_ptr<Vehicle>)>;
    void setAddVehicleCallback(AddVehicleCallback callback) { m_addVehicleCallback = std::move(callback); }
    
    std::vector<Pedestrian*> getShootableOfficers() const;
    
    // Configuration
    void setKillThreshold(int threshold) { m_killThreshold = threshold; }
    void setKillWindowSeconds(float seconds) { m_killWindowSeconds = seconds; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    // Chase state
    bool isChaseActive() const { return m_chaseActive; }
    int getRecentKillCount() const;

private:
    TileGrid* m_tileGrid;
    Camera* m_camera;
    Player* m_player;
    TrafficManager* m_trafficManager;
    
    std::vector<std::unique_ptr<Vehicle>>* m_vehicles = nullptr;  // Shared vehicle list (owned by Scene)
    PlayerPositionCallback m_playerPositionCallback;
    OfficerShootCallback m_officerShootCallback;
    AddVehicleCallback m_addVehicleCallback;
    ColliderCallback m_officerColliderCallback;  // Used for officer-vs-vehicle collision

    // On-foot police officer state (one officer for now)
    std::unique_ptr<SpriteAnimation> m_policeOfficerAnimation;
    std::unique_ptr<Pedestrian> m_onFootOfficer;
    Vehicle* m_officerVehicle = nullptr;
    float m_officerShootCooldown = 0.0f;
    
    // Kill tracking: timestamps of recent pedestrian kills
    std::deque<float> m_killTimestamps;
    float m_currentTime;
    
    // Configuration
    int m_killThreshold;           // Number of kills to trigger chase (default: 3)
    float m_killWindowSeconds;     // Time window for counting kills (default: 60s)
    float m_viewMargin;            // Margin for spawn zone
    bool m_enabled;
    bool m_chaseActive;
    
    // Projection info for view bounds
    float m_fovRadians;
    float m_aspectRatio;
    
    // Random number generation
    std::mt19937 m_rng;
    
    // Temp storage for spawn rotation (set by findValidSpawnPoint)
    float m_lastSpawnRotation = 0.0f;
    float m_officerDeployDistance = 8.0f;
    float m_officerReturnDistance = 18.0f;
    float m_officerFireDistance = 14.0f;
    float m_officerSpeed = 3.8f;
    
    // Helper methods
    void checkChaseCondition();
    void spawnPoliceVehicle();
    void assignPolicePilot(Vehicle* vehicle);
    void updatePoliceVehicles(float deltaTime);
    void updateOfficer(float deltaTime);
    void maybeDeployOfficer(Vehicle* vehicle, const glm::vec3& playerPos);
    void updateOfficerCombat(float deltaTime, const glm::vec3& playerPos);
    void tryOfficerReenterVehicle(float deltaTime, const glm::vec3& playerPos);
    void checkOfficerVehicleCollision();
    void clearOfficer(bool keepCorpse);
    void cleanupOldKills();
    glm::vec3 findValidSpawnPoint();
    bool isTooCloseToOtherVehicles(const glm::vec3& position) const;
};
