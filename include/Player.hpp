#pragma once

#include "Character.hpp"
#include "ControllableObject.hpp"
#include "SpriteAnimation.hpp"
#include "Weapon.hpp"
#include "PickupTypes.hpp"
#include <array>
#include <functional>
#include <memory>
#include <optional>

class Player : public Character, public ControllableObject {
private:
    std::shared_ptr<Texture> m_texture;          // Static fallback texture
    std::unique_ptr<SpriteAnimation> m_walkAnimation;
    float m_speed;
    float m_rotationSpeed;
    
    // Animation state
    bool m_isMoving;
    std::array<std::unique_ptr<Weapon>, pickupTypeCount()> m_weaponSlots;
    std::size_t m_equippedSlot;  // Index into m_weaponSlots; pickupTypeCount() == no weapon
    int m_money;
    
    std::function<void(int fallTiles)> m_fatalFallCallback;

    void applyMovement(const glm::vec3& delta);

public:
    // Falls of more than this many tile levels kill the player.
    static constexpr int SurvivableFallTiles = 1;

    explicit Player(CharacterPhysics& physics);
    ~Player() = default;
    
    bool initialize();
    void update(float deltaTime) override;
    void render(class Renderer* renderer) override;
    
    void moveForward(float deltaTime);
    void moveBackward(float deltaTime);
    void turnLeft(float deltaTime);
    void turnRight(float deltaTime);

    bool hasWeapon() const;
    bool hasWeapon(PickupType type) const;
    Weapon* getWeapon(PickupType type);
    const Weapon* getWeapon(PickupType type) const;
    Weapon* getEquippedWeapon();
    const Weapon* getEquippedWeapon() const;
    std::optional<PickupType> getEquippedWeaponType() const;
    const char* getWeaponDisplayName() const;
    int getWeaponAmmo() const;
    int getMoney() const { return m_money; }
    void addMoney(int amount);
    bool addAmmo(PickupType type, int amount);
    void equipWeapon(PickupType type, std::unique_ptr<Weapon> weapon);
    bool equipWeaponType(PickupType type);
    void switchWeapon(int direction);
    void switchWeaponNext();
    void switchWeaponPrev();
    bool canShoot() const;
    void recordShot();
    
    void setFatalFallCallback(std::function<void(int fallTiles)> callback) {
        m_fatalFallCallback = std::move(callback);
    }

    float getSpeed() const { return m_speed; }
    void setSpeed(float speed) { m_speed = speed; }
};
