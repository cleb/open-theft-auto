#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Scene;
class Vehicle;
class Renderer;
class EscortMissionState;
class TileGrid;
class CharacterPhysics;

enum class MissionState {
    Idle,
    Prompted,
    Active,
    Completed,
    Failed
};

struct Job {
    std::string id;
    std::string title;
    std::string description;
    std::string completionMessage;
    int rewardMoney = 0;
    std::function<bool(const Scene&)> activationCondition;
    std::function<bool(const Scene&, const glm::vec3& boothWorldPos)> successCondition;
    std::function<void(const glm::vec3& boothWorldPos)> onStart;
    std::function<void(float deltaTime, const Scene&)> onUpdate;
    std::function<void(const Scene&)> onComplete;
    std::function<void(Renderer*)> onRender;
    std::function<void()> onReset;
};

class MissionSystem {
public:
    using ShootCallback = std::function<void(const glm::vec3& origin, const glm::vec2& dir)>;
    // Lookup: given a marker name, return its world position (or zero if not found)
    using MarkerLookup = std::function<glm::vec3(const std::string& name)>;

    MissionSystem();

    void registerBuiltinJobs();

    void setEnemyShootCallback(ShootCallback cb);
    void setMarkerLookup(MarkerLookup lookup) { m_markerLookup = std::move(lookup); }
    void setTileGrid(TileGrid* tileGrid);
    void setCharacterPhysics(CharacterPhysics* characterPhysics);

    const Job* findJob(const std::string& jobId) const;

    MissionState getState() const { return m_state; }
    const Job* getActiveJob() const { return m_activeJob; }
    const std::string& getActiveBoothId() const { return m_activeBoothId; }
    glm::vec3 getActiveBoothWorldPos() const { return m_activeBoothWorldPos; }

    void startMission(const Job* job, const std::string& boothId, const glm::vec3& boothWorldPos);
    bool update(float deltaTime, const Scene& scene);
    void render(Renderer* renderer) const;
    void reset();

    bool isBoothActive(const std::string& jobId, const Scene& scene) const;

    // Returns true if a player projectile hit a mission enemy
    bool checkProjectileHitEnemy(const glm::vec2& projPos, float projRadius);
    // Check if a vehicle overlaps a mission enemy
    void checkVehicleHitEnemies(const glm::vec3& vehiclePos, const glm::vec2& vehicleSize, float vehicleRotation,
                                float vehicleSpeed);

private:
    std::vector<Job> m_jobs;
    MissionState m_state = MissionState::Idle;
    const Job* m_activeJob = nullptr;
    std::string m_activeBoothId;
    glm::vec3 m_activeBoothWorldPos{0.0f};

    ShootCallback m_enemyShootCallback;
    MarkerLookup m_markerLookup;

    std::shared_ptr<EscortMissionState> m_escortState;
};
