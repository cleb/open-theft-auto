#pragma once

#include "ControllableObject.hpp"
#include "Collider.hpp"
#include <memory>
#include <array>
#include <functional>
#include <string>
#include <glm/glm.hpp>

class TileGrid;
class Pilot;
class Vehicle;

// Callback invoked when a vehicle is carjacked
// Parameters: vehicle position, vehicle rotation (degrees), vehicle size
using CarjackCallback = std::function<void(const glm::vec3&, float, const glm::vec2&)>;
using VehicleExplodeCallback = std::function<void(Vehicle*)>;

// Damage state for vehicle quadrants
struct VehicleDamage {
    bool frontLeft = false;   // Upper-left quadrant (front-left of car)
    bool frontRight = false;  // Upper-right quadrant (front-right of car)
    bool rearLeft = false;    // Lower-left quadrant (rear-left of car)
    bool rearRight = false;   // Lower-right quadrant (rear-right of car)
    
    bool hasAnyDamage() const {
        return frontLeft || frontRight || rearLeft || rearRight;
    }
    
    void reset() {
        frontLeft = frontRight = rearLeft = rearRight = false;
    }
};

// Direction of collision impact
enum class CollisionDirection {
    None,
    Front,
    Rear,
    Left,
    Right
};

class Vehicle : public ControllableObject, public Collider {
private:
    std::shared_ptr<Texture> m_texture;
    std::shared_ptr<Texture> m_deltaTexture;     // Damage overlay texture
    std::unique_ptr<Pilot> m_pilot;
    CarjackCallback m_carjackCallback;           // Called when vehicle is carjacked
    float m_speed;
    float m_maxSpeed;
    float m_maxSpeedRoad;
    float m_acceleration;
    float m_turnSpeed;
    glm::vec2 m_size;
    TileGrid* m_tileGrid;
    CollisionManager m_collisionManager;
    VehicleDamage m_damage;
    bool m_inCollision = false;  // Tracks if currently in collision state
    std::string m_vehicleTypeId = "sedan";  // Type ID of this vehicle (from VehicleConfig)
    int m_maxHealth = 10;
    int m_health = 10;
    bool m_burning = false;
    float m_burnTimer = 0.0f;
    float m_effectTime = 0.0f;
    bool m_exploding = false;
    bool m_hasExploded = false;
    float m_explosionTimer = 0.0f;
    float m_collisionDamageCooldown = 0.0f;
    VehicleExplodeCallback m_explodeCallback;
    std::shared_ptr<Texture> m_explodedTexture;

public:
    Vehicle();
    ~Vehicle();
    
    bool initialize(const std::string& texturePath);
    bool initialize(std::shared_ptr<Texture> texture);
    void update(float deltaTime) override;
    void render(class Renderer* renderer) override;
    
    void moveForward(float deltaTime) override;
    void moveBackward(float deltaTime) override;
    void turnLeft(float deltaTime) override;
    void turnRight(float deltaTime) override;
    void setSpriteSize(const glm::vec2& size) { m_size = size; }
    const glm::vec2& getSpriteSize() const { return m_size; }
    void setTileGrid(class TileGrid* tileGrid) { m_tileGrid = tileGrid; }
    TileGrid* getTileGrid() const { return m_tileGrid; }
    
    // Vehicle type configuration
    void setVehicleType(const std::string& typeId);
    const std::string& getVehicleTypeId() const { return m_vehicleTypeId; }
    
    // Pilot management
    void setPilot(std::unique_ptr<Pilot> pilot);
    Pilot* getPilot() const { return m_pilot.get(); }
    void clearPilot();
    bool hasPilot() const { return m_pilot != nullptr; }
    
    // Carjack callback - called when player takes the vehicle
    void setCarjackCallback(CarjackCallback callback) { m_carjackCallback = std::move(callback); }
    void triggerCarjack();
    bool hasCarjackCallback() const { return m_carjackCallback != nullptr; }

    void setExplodeCallback(VehicleExplodeCallback callback) { m_explodeCallback = std::move(callback); }
    
    // Speed access for pilots
    float getSpeed() const { return m_speed; }
    void setSpeed(float speed) { m_speed = speed; }
    float getMaxSpeed() const { return m_maxSpeed; }
    float getAcceleration() const { return m_acceleration; }
    float getTurnSpeed() const { return m_turnSpeed; }
    float getCurrentMaxSpeed() const;  // Get max speed based on current surface
    
    // Collider interface implementation
    glm::vec3 getColliderPosition() const override { return m_position; }
    float getColliderRotation() const override { return m_rotation.z; }
    glm::vec2 getColliderSize() const override { return m_size; }
    bool isColliderActive() const override { return m_active; }
    
    // Collision management
    void setCollisionCallback(ColliderCallback callback) { m_collisionManager.setColliderCallback(callback); }
    CollisionManager& getCollisionManager() { return m_collisionManager; }
    const CollisionManager& getCollisionManager() const { return m_collisionManager; }
    
    // Damage system
    void setDeltaTexture(std::shared_ptr<Texture> deltaTexture);
    void applyDamage(CollisionDirection direction);
    void applyDamageFromCollision(const glm::vec3& collisionPoint);
    const VehicleDamage& getDamage() const { return m_damage; }
    void resetDamage();

    void applyHit(int amount = 1);
    int getHealth() const { return m_health; }
    int getMaxHealth() const { return m_maxHealth; }
    bool isBurning() const { return m_burning; }
    bool isExploding() const { return m_exploding; }
    bool isWrecked() const { return m_hasExploded; }
    
    // Collision state management (prevents repeated damage application)
    void setInCollision(bool inCollision) { m_inCollision = inCollision; }
    bool isInCollision() const { return m_inCollision; }

private:
    float getCurrentDrivability() const;
    bool isOnRoad() const;
    std::array<glm::vec3, 8> getCollisionOffsets() const;
    bool canMoveTo(const glm::vec3& from, const glm::vec3& to) const;
    bool wouldCollideWithOther(const glm::vec3& newPosition) const;
    CollisionDirection determineCollisionDirection(const glm::vec3& collisionPoint) const;
    float getHealthFraction() const;
    float getFireIntensity() const;
    void startBurning();
    void triggerExplosion();
    void setExplodedTextureIfNeeded();
};