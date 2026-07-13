#pragma once

#include "Vehicle.hpp"
#include "TileGrid.hpp"
#include "Camera.hpp"
#include "ViewBounds.hpp"
#include "TrafficManager.hpp"
#include "CombatPedestrian.hpp"
#include "SpriteAnimation.hpp"
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <random>
#include <deque>
#include <functional>
#include <string>

class Renderer;
class Player;
class CharacterPhysics;

// Callback to get the current player position
using PlayerPositionCallback = std::function<glm::vec3()>;
// Callback for police officer weapon fire
using OfficerShootCallback = std::function<void(const glm::vec3&, const glm::vec2&)>;

class PoliceChaseManager {
public:
    PoliceChaseManager();
    ~PoliceChaseManager() = default;

    void initialize(TileGrid* tileGrid, Camera* camera, Player* player, TrafficManager* trafficManager,
                    std::vector<std::unique_ptr<Vehicle>>* vehicles, CharacterPhysics* characterPhysics);
    void update(float deltaTime);
    void render(Renderer* renderer);
    void endChase();
    void resetForWorldRestart();
    
    // Set projection info for accurate view bounds calculation
    void setProjectionInfo(float fovRadians, float aspectRatio);
    
    // Called when the player fires a weapon (regardless of whether it hits)
    void onPlayerFiredWeapon();
    
    // Called when the player runs down a pedestrian with a vehicle
    void onPedestrianRunDown();
    
    // Called when a pedestrian is killed by player gunfire
    void onPedestrianKilledByGunfire();
    
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
    int getWantedPoliceUnitCount() const { return m_wantedLevel; }
    int getRecentKillCount() const;

private:
    TileGrid* m_tileGrid;
    Camera* m_camera;
    Player* m_player;
    TrafficManager* m_trafficManager;
    CharacterPhysics* m_characterPhysics;
    
    std::vector<std::unique_ptr<Vehicle>>* m_vehicles = nullptr;  // Shared vehicle list (owned by Scene)
    PlayerPositionCallback m_playerPositionCallback;
    OfficerShootCallback m_officerShootCallback;
    AddVehicleCallback m_addVehicleCallback;
    ColliderCallback m_officerColliderCallback;  // Used for officer-vs-vehicle collision

    struct OfficerUnit {
        std::unique_ptr<CombatPedestrian> officer;
        Vehicle* vehicle = nullptr;
        bool deathHandled = false;
        bool inVehicle = false;
    };

    // On-foot police officer state
    std::unique_ptr<SpriteAnimation> m_policeOfficerAnimation;
    std::vector<OfficerUnit> m_onFootOfficers;
    const Vehicle* m_officerDetourVehicle = nullptr;
    int m_officerDetourSide = 0;
    
    // Kill tracking: timestamps of recent pedestrian kills
    std::deque<float> m_killTimestamps;
    float m_currentTime = 0.0f;
    
    // Configuration
    int m_killThreshold = 3;       // Number of kills to trigger chase (default: 3)
    float m_killWindowSeconds = 60.0f; // Time window for counting kills (default: 60s)
    float m_viewMargin;            // Margin for spawn zone
    bool m_enabled;
    bool m_chaseActive;
    int m_wantedLevel = 0;
    int m_civilianKillOffensesAtWantedLevel = 0;
    int m_directOffensesAtWantedLevel = 0;
    
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
    bool isAnyPoliceVehicleOnScreen() const;
    void triggerChase();
    void increaseWantedLevel(const std::string& reason);
    void recordCivilianKillOffense(const std::string& reason);
    void recordDirectWantedOffense(const std::string& reason);
    int getCivilianKillThresholdForCurrentLevel() const;
    int getDirectOffenseThresholdForCurrentLevel() const;
    void clearWantedState();
    void activatePoliceVehicles();
    void checkChaseCondition();
    void cleanupOldKills();
    void spawnPoliceVehicle();
    void assignPolicePilot(Vehicle* vehicle);
    void assignPatrolPilot(Vehicle* vehicle);
    void updatePoliceVehicles(float deltaTime);
    void updateOfficer(float deltaTime);
    void ensureWantedPoliceUnits();
    int getActivePoliceUnitCount() const;
    bool isVehicleAssignedToOfficer(const Vehicle* vehicle) const;
    OfficerUnit* findOfficerUnitForVehicle(Vehicle* vehicle);
    void handleOfficerKilled(OfficerUnit& unit);
    void updateOfficerUnit(OfficerUnit& unit, float deltaTime, const glm::vec3& playerPos);
    void maybeDeployOfficer(Vehicle* vehicle, const glm::vec3& playerPos);
    void tryOfficerReenterVehicle(OfficerUnit& unit, float deltaTime, const glm::vec3& playerPos);
    void clearOfficer(OfficerUnit& unit);
    glm::vec3 getOfficerVehicleEntryPoint(const OfficerUnit& unit, const glm::vec3& officerPos) const;
    glm::vec3 adjustOfficerMovementTargetAroundVehicles(const glm::vec3& from, const glm::vec3& target,
                                                        float officerRadius);
    glm::vec3 findValidSpawnPoint();
    bool isTooCloseToOtherVehicles(const glm::vec3& position) const;
    bool isOfficerPositionBlockedByVehicle(const glm::vec3& position, float officerRadius) const;
    bool isOfficerLineOfSightBlockedByVehicle(const glm::vec3& from, const glm::vec3& target,
                                              float clearanceRadius) const;
    bool segmentIntersectsVehicle(const glm::vec3& from, const glm::vec3& target, const Vehicle* vehicle,
                                  float clearanceRadius, bool ignoreIfTargetInside) const;
};
