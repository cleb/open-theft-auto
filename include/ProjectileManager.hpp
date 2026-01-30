#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Renderer;
class Texture;
class PedestrianManager;
class TrafficManager;
class Vehicle;

class ProjectileManager {
public:
    ProjectileManager();
    ~ProjectileManager() = default;

    void initialize();
    void update(float deltaTime, PedestrianManager* pedestrians,
                const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                TrafficManager* trafficManager);
    void render(Renderer* renderer) const;

    void spawnProjectile(const glm::vec3& origin, const glm::vec2& direction,
                         float speed, float maxRange);

private:
    struct Projectile {
        glm::vec3 position{0.0f};
        glm::vec2 velocity{0.0f};
        float life = 0.0f;
    };

    std::shared_ptr<Texture> m_projectileTexture;
    std::vector<Projectile> m_projectiles;

    void updateProjectile(Projectile& projectile, float deltaTime,
                          PedestrianManager* pedestrians,
                          const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                          TrafficManager* trafficManager);
    bool checkPedestrianHit(const glm::vec2& projPos, float projRadius, PedestrianManager* pedestrians) const;
    bool checkVehicleHit(const glm::vec2& projPos, float projRadius,
                         const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                         TrafficManager* trafficManager) const;
};
