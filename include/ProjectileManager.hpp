#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class Renderer;
class Texture;
class PedestrianManager;
class TrafficManager;
class Vehicle;
class Pedestrian;

class ProjectileManager {
public:
    enum class ProjectileOwner {
        Player,
        Police
    };

    using ExtraPedestrianTargetsCallback = std::function<std::vector<Pedestrian*>()>;
    using EnemyHitCallback = std::function<bool(const glm::vec2&, float)>;

    ProjectileManager();
    ~ProjectileManager() = default;

    void initialize();
    void update(float deltaTime, PedestrianManager* pedestrians,
                const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                TrafficManager* trafficManager);
    void render(Renderer* renderer) const;
    void setPedestrianHitCallback(std::function<void()> callback) { m_pedestrianHitCallback = std::move(callback); }
    void setExtraPedestrianTargetsCallback(ExtraPedestrianTargetsCallback callback) {
        m_extraPedestrianTargetsCallback = std::move(callback);
    }
    void setEnemyHitCallback(EnemyHitCallback callback) { m_enemyHitCallback = std::move(callback); }

    void spawnProjectile(const glm::vec3& origin, const glm::vec2& direction,
                         float speed, float maxRange,
                         ProjectileOwner owner = ProjectileOwner::Player);

private:
    struct Projectile {
        glm::vec3 position{0.0f};
        glm::vec2 velocity{0.0f};
        float life = 0.0f;
        ProjectileOwner owner = ProjectileOwner::Player;
    };

    std::shared_ptr<Texture> m_projectileTexture;
    std::vector<Projectile> m_projectiles;
    std::function<void()> m_pedestrianHitCallback;
    ExtraPedestrianTargetsCallback m_extraPedestrianTargetsCallback;
    EnemyHitCallback m_enemyHitCallback;

    void updateProjectile(Projectile& projectile, float deltaTime,
                          PedestrianManager* pedestrians,
                          const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                          TrafficManager* trafficManager);
    bool checkPedestrianHit(const Projectile& projectile, const glm::vec2& projPos, float projRadius,
                            PedestrianManager* pedestrians);
    bool checkVehicleHit(const glm::vec2& projPos, float projRadius,
                         const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                         TrafficManager* trafficManager);
    bool checkEnemyHit(const glm::vec2& projPos, float projRadius);
};
