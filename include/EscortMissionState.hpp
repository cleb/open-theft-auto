#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <vector>

class Renderer;
class Scene;
class SpriteAnimation;
class CombatPedestrian;
class TileGrid;
class Vehicle;
class Pedestrian;
class CharacterPhysics;

enum class EscortPhase {
    WaitingPickup,
    WalkingToVehicle,
    EscortBoarded,
    EnemiesActive,
    Complete,
    Failed
};

class EscortMissionState {
public:
    using ShootCallback = std::function<void(const glm::vec3& origin, const glm::vec2& dir)>;

    EscortMissionState();
    ~EscortMissionState();

    void setShootCallback(ShootCallback cb) { m_shootCallback = std::move(cb); }
    void setTileGrid(TileGrid* tileGrid) { m_tileGrid = tileGrid; }
    void setCharacterPhysics(CharacterPhysics* characterPhysics) { m_characterPhysics = characterPhysics; }

    void start(const glm::vec3& boothWorldPos,
               const glm::vec3& pickupWorldPos, const glm::vec3& escortWorldPos);
    void reset();

    void update(float deltaTime, const Scene& scene);
    void render(Renderer* renderer) const;

    bool isComplete() const { return m_phase == EscortPhase::Complete; }
    bool isFailed() const { return m_phase == EscortPhase::Failed; }
    EscortPhase getPhase() const { return m_phase; }

    const glm::vec3& getPickupPos() const { return m_pickupPos; }
    const glm::vec3& getReturnPos() const { return m_boothPos; }

    bool checkBulletHit(const glm::vec3& bulletPos, float bulletRadius);
    void checkVehicleCollision(const glm::vec3& vehiclePos, const glm::vec2& vehicleSize, float vehicleRotation,
                               float vehicleSpeed);

    bool hasActiveEnemies() const;

private:
    EscortPhase m_phase = EscortPhase::WaitingPickup;

    glm::vec3 m_boothPos{0.0f};
    glm::vec3 m_pickupPos{0.0f};
    glm::vec3 m_escortSpawnPos{0.0f};

    float m_enemySpawnTimer = 0.0f;
    int m_enemySpawnCount = 0;

    const Vehicle* m_boardedVehicle = nullptr;

    std::unique_ptr<SpriteAnimation> m_escortAnimation;
    std::unique_ptr<SpriteAnimation> m_enemyAnimation;
    std::unique_ptr<Pedestrian> m_escortCharacter;
    std::vector<std::unique_ptr<CombatPedestrian>> m_enemies;

    TileGrid* m_tileGrid = nullptr;
    CharacterPhysics* m_characterPhysics = nullptr;
    ShootCallback m_shootCallback;

    static constexpr float kPickupRadius = 3.5f;
    static constexpr float kReturnRadius = 4.0f;
    static constexpr float kEnemySpawnDelay = 5.0f;
    static constexpr float kEnemySpawnInterval = 0.75f;
    static constexpr int kEnemyCount = 3;
    static constexpr float kBoardRadius = 1.2f;
    static constexpr float kEscortWalkSpeed = 3.0f;

    bool spawnEnemy();
    void renderMarkers(Renderer* renderer) const;
};
