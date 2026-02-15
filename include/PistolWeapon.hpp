#pragma once

#include "Weapon.hpp"

class PistolWeapon : public Weapon {
public:
    explicit PistolWeapon(int ammo = 10);
    ~PistolWeapon() override = default;

    void update(float deltaTime) override;
    bool canFire() const override;
    void recordFire() override;
    const char* getDisplayName() const override;
    int getAmmo() const override;
    void addAmmo(int amount) override;

private:
    float m_shotCooldown;
    float m_timeSinceLastShot;
    int m_ammo;
};
