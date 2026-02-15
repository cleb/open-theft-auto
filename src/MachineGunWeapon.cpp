#include "MachineGunWeapon.hpp"

#include <algorithm>

MachineGunWeapon::MachineGunWeapon(int ammo)
    : m_shotCooldown(0.1f)
    , m_timeSinceLastShot(0.1f)
    , m_ammo(std::max(0, ammo)) {
}

void MachineGunWeapon::update(float deltaTime) {
    m_timeSinceLastShot = std::min(m_timeSinceLastShot + deltaTime, m_shotCooldown);
}

bool MachineGunWeapon::canFire() const {
    return m_ammo > 0 && m_timeSinceLastShot >= m_shotCooldown;
}

void MachineGunWeapon::recordFire() {
    if (m_ammo <= 0) {
        return;
    }
    m_timeSinceLastShot = 0.0f;
    --m_ammo;
}

const char* MachineGunWeapon::getDisplayName() const {
    return "Machine Gun";
}

int MachineGunWeapon::getAmmo() const {
    return m_ammo;
}

void MachineGunWeapon::addAmmo(int amount) {
    if (amount <= 0) {
        return;
    }
    m_ammo += amount;
}
