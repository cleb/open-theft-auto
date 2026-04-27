#pragma once

#include "ControllableObject.hpp"
#include "Collider.hpp"
#include "SpriteAnimation.hpp"
#include "Weapon.hpp"
#include "PickupTypes.hpp"
#include <array>
#include <memory>
#include <optional>

class TileGrid;

class Player : public ControllableObject, public Collider {
private:
    std::shared_ptr<Texture> m_texture;          // Static fallback texture
    std::unique_ptr<SpriteAnimation> m_walkAnimation;
    float m_speed;
    float m_rotationSpeed;
    glm::vec2 m_size;
    TileGrid* m_tileGrid;
    CollisionManager m_collisionManager;
    
    // Animation state
    bool m_isMoving;
    std::array<std::unique_ptr<Weapon>, pickupTypeCount()> m_weaponSlots;
    std::size_t m_equippedSlot;  // Index into m_weaponSlots; pickupTypeCount() == no weapon
    
    void applyMovement(const glm::vec3& delta);

public:
    Player();
    ~Player() = default;
    
    bool initialize();
    void update(float deltaTime) override;
    void render(class Renderer* renderer) override;
    
    void moveForward(float deltaTime);
    void moveBackward(float deltaTime);
    void turnLeft(float deltaTime);
    void turnRight(float deltaTime);
    void setTileGrid(TileGrid* tileGrid) { m_tileGrid = tileGrid; }

    bool hasWeapon() const;
    bool hasWeapon(PickupType type) const;
    Weapon* getWeapon(PickupType type);
    const Weapon* getWeapon(PickupType type) const;
    Weapon* getEquippedWeapon();
    const Weapon* getEquippedWeapon() const;
    std::optional<PickupType> getEquippedWeaponType() const;
    const char* getWeaponDisplayName() const;
    int getWeaponAmmo() const;
    bool addAmmo(PickupType type, int amount);
    void equipWeapon(PickupType type, std::unique_ptr<Weapon> weapon);
    bool equipWeaponType(PickupType type);
    void switchWeapon(int direction);
    void switchWeaponNext();
    void switchWeaponPrev();
    bool canShoot() const;
    void recordShot();
    
    float getSpeed() const { return m_speed; }
    void setSpeed(float speed) { m_speed = speed; }
    
    // Collider interface implementation
    glm::vec3 getColliderPosition() const override { return m_position; }
    float getColliderRotation() const override { return m_rotation.z; }
    glm::vec2 getColliderSize() const override { return m_size; }
    bool isColliderActive() const override { return m_active; }
    
    // Collision management
    void setCollisionCallback(ColliderCallback callback) { m_collisionManager.setColliderCallback(callback); }
    CollisionManager& getCollisionManager() { return m_collisionManager; }
    const CollisionManager& getCollisionManager() const { return m_collisionManager; }
};
