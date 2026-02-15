#pragma once

#include "Weapon.hpp"

class MachineGunWeapon : public Weapon {
public:
    explicit MachineGunWeapon(int ammo = 50);
    ~MachineGunWeapon() override = default;

    void update(float deltaTime) override;
    bool canFire() const override;
    void recordFire() override;
    const char* getDisplayName() const override;
    int getAmmo() const override;
    void addAmmo(int amount) override;
    bool isAutoFire() const override { return true; }

private:
    float m_shotCooldown;
    float m_timeSinceLastShot;
    int m_ammo;
};
