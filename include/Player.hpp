#pragma once

#include "ControllableObject.hpp"
#include "Collider.hpp"
#include "SpriteAnimation.hpp"
#include "Weapon.hpp"
#include <memory>

class TileGrid;

class Player : public ControllableObject, public Collider {
private:
    std::unique_ptr<Texture> m_texture;          // Static fallback texture
    std::unique_ptr<SpriteAnimation> m_walkAnimation;
    float m_speed;
    float m_rotationSpeed;
    glm::vec2 m_size;
    TileGrid* m_tileGrid;
    CollisionManager m_collisionManager;
    
    // Animation state
    bool m_isMoving;
    std::unique_ptr<Weapon> m_weapon;
    
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

    bool hasWeapon() const { return m_weapon != nullptr; }
    const Weapon* getWeapon() const { return m_weapon.get(); }
    const char* getWeaponDisplayName() const { return m_weapon ? m_weapon->getDisplayName() : "Unarmed"; }
    int getWeaponAmmo() const { return m_weapon ? m_weapon->getAmmo() : 0; }
    void equipWeapon(std::unique_ptr<Weapon> weapon);
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
