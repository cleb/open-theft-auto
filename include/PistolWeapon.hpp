#pragma once

#include "Weapon.hpp"

class PistolWeapon : public Weapon {
public:
    PistolWeapon();
    ~PistolWeapon() override = default;

    void update(float deltaTime) override;
    bool canFire() const override;
    void recordFire() override;

private:
    float m_shotCooldown;
    float m_timeSinceLastShot;
};
