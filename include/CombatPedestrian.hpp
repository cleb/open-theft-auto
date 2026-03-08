#pragma once

#include "Pedestrian.hpp"
#include "Heading.hpp"
#include <functional>
#include <memory>
#include <glm/glm.hpp>

class TileGrid;
class SpriteAnimation;
class Renderer;
class Vehicle;

// Wraps a Pedestrian with chase/shoot/vehicle-collision behavior.
// Used by both PoliceChaseManager (officer) and EscortMissionState (enemies).
class CombatPedestrian {
public:
    using ShootCallback = std::function<void(const glm::vec3& origin, const glm::vec2& dir)>;

    CombatPedestrian();
    ~CombatPedestrian() = default;

    // Initialize the underlying Pedestrian with animation and grid
    void spawn(SpriteAnimation* animation, TileGrid* tileGrid,
               const glm::vec3& position, float headingDeg);

    // Per-frame update: animation, chase, shoot
    void update(float deltaTime, const glm::vec3& targetPos);

    // Render the underlying pedestrian
    void render(Renderer* renderer) const;

    // Vehicle collision: check against one vehicle's OBB, kill on contact
    bool checkVehicleCollision(const glm::vec3& vehiclePos, const glm::vec2& vehicleSize, float vehicleRotation);

    // Bullet hit check: returns true if bullet overlaps this pedestrian
    bool checkBulletHit(const glm::vec3& bulletPos, float bulletRadius);

    void setShootCallback(ShootCallback cb) { m_shootCallback = std::move(cb); }

    // Configuration
    void setSpeed(float speed) { m_speed = speed; }
    void setFireDistance(float dist) { m_fireDistance = dist; }
    void setChaseDistance(float dist) { m_chaseDistance = dist; }
    void setShootCooldown(float cooldown) { m_shootCooldownTime = cooldown; }

    Pedestrian* getPedestrian() { return m_pedestrian.get(); }
    const Pedestrian* getPedestrian() const { return m_pedestrian.get(); }

    bool isAlive() const;
    bool isDead() const;
    const glm::vec3& getPosition() const;

private:
    std::unique_ptr<Pedestrian> m_pedestrian;
    ShootCallback m_shootCallback;
    TileGrid* m_tileGrid = nullptr;

    float m_shootCooldown = 0.0f;

    // Configurable parameters
    float m_speed = 3.8f;
    float m_fireDistance = 14.0f;
    float m_chaseDistance = 5.0f;
    float m_shootCooldownTime = 0.55f;
};
