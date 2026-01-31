#include "ProjectileManager.hpp"

#include "Renderer.hpp"
#include "Texture.hpp"
#include "PedestrianManager.hpp"
#include "TrafficManager.hpp"
#include "Vehicle.hpp"

#include <algorithm>
#include <iostream>

namespace {
constexpr float kProjectileRadius = 0.08f;
constexpr glm::vec2 kProjectileSize(0.15f, 0.15f);
}

ProjectileManager::ProjectileManager() = default;

void ProjectileManager::initialize() {
    if (!m_projectileTexture) {
        m_projectileTexture = std::make_shared<Texture>();
        m_projectileTexture->createSolidColor(255, 255, 255, 255);
    }
}

void ProjectileManager::spawnProjectile(const glm::vec3& origin, const glm::vec2& direction,
                                        float speed, float maxRange) {
    Projectile projectile;
    projectile.position = origin;
    projectile.velocity = direction * speed;
    projectile.life = maxRange / speed;
    m_projectiles.push_back(projectile);
}

void ProjectileManager::update(float deltaTime, PedestrianManager* pedestrians,
                               const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                               TrafficManager* trafficManager) {
    if (m_projectiles.empty()) {
        return;
    }

    for (auto& projectile : m_projectiles) {
        updateProjectile(projectile, deltaTime, pedestrians, playerVehicles, trafficManager);
    }

    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(), [](const Projectile& projectile) {
            return projectile.life <= 0.0f;
        }),
        m_projectiles.end());
}

void ProjectileManager::updateProjectile(Projectile& projectile, float deltaTime,
                                         PedestrianManager* pedestrians,
                                         const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                                         TrafficManager* trafficManager) {
    projectile.position.x += projectile.velocity.x * deltaTime;
    projectile.position.y += projectile.velocity.y * deltaTime;
    projectile.life -= deltaTime;

    if (projectile.life <= 0.0f) {
        return;
    }

    const glm::vec2 projPos(projectile.position.x, projectile.position.y);

    if (checkPedestrianHit(projPos, kProjectileRadius, pedestrians)) {
        projectile.life = 0.0f;
        return;
    }

    if (checkVehicleHit(projPos, kProjectileRadius, playerVehicles, trafficManager)) {
        projectile.life = 0.0f;
    }
}

bool ProjectileManager::checkPedestrianHit(const glm::vec2& projPos, float projRadius,
                                           PedestrianManager* pedestrians) const {
    if (!pedestrians) {
        return false;
    }

    for (const auto& pedestrian : pedestrians->getPedestrians()) {
        if (!pedestrian || !pedestrian->isActive() || pedestrian->isDead()) {
            continue;
        }
        const glm::vec3 pedPos3 = pedestrian->getPosition();
        const glm::vec2 pedPos(pedPos3.x, pedPos3.y);
        const glm::vec2 diff = pedPos - projPos;
        const glm::vec2 pedSize = pedestrian->getSize();
        const float pedRadius = std::max(pedSize.x, pedSize.y) * 0.25f;
        const float radius = projRadius + pedRadius;
        if ((diff.x * diff.x + diff.y * diff.y) <= radius * radius) {
            pedestrian->kill();
            return true;
        }
    }

    return false;
}

bool ProjectileManager::checkVehicleHit(const glm::vec2& projPos, float projRadius,
                                        const std::vector<std::unique_ptr<Vehicle>>* playerVehicles,
                                        TrafficManager* trafficManager) const {
    auto testVehicle = [&](Vehicle* vehicle) {
        if (!vehicle || !vehicle->isActive()) {
            return false;
        }
        const glm::vec3 vehPos3 = vehicle->getPosition();
        const glm::vec2 vehPos(vehPos3.x, vehPos3.y);
        const glm::vec2 diff = vehPos - projPos;
        const glm::vec2 vehSize = vehicle->getSpriteSize();
        const float vehRadius = std::max(vehSize.x, vehSize.y) * 0.5f;
        const float radius = projRadius + vehRadius;
        if ((diff.x * diff.x + diff.y * diff.y) <= radius * radius) {
            std::cout << "Pistol shot hit vehicle" << std::endl;
            vehicle->applyHit(1);
            return true;
        }
        return false;
    };

    if (playerVehicles) {
        for (const auto& vehicle : *playerVehicles) {
            if (testVehicle(vehicle.get())) {
                return true;
            }
        }
    }

    if (trafficManager) {
        for (const auto& vehicle : trafficManager->getTrafficVehicles()) {
            if (testVehicle(vehicle.get())) {
                return true;
            }
        }
    }

    return false;
}

void ProjectileManager::render(Renderer* renderer) const {
    if (!renderer || !m_projectileTexture) {
        return;
    }

    for (const auto& projectile : m_projectiles) {
        renderer->renderSprite(*m_projectileTexture, glm::vec2(projectile.position.x, projectile.position.y),
                               kProjectileSize, 0.0f, glm::vec3(1.0f));
    }
}
