#include "PistolWeapon.hpp"

#include <algorithm>
#include <utility>

PistolWeapon::PistolWeapon(int ammo)
    : m_shotCooldown(0.5f)
    , m_timeSinceLastShot(0.5f)
    , m_ammo(std::max(0, ammo)) {
}

void PistolWeapon::update(float deltaTime) {
    m_timeSinceLastShot = std::min(m_timeSinceLastShot + deltaTime, m_shotCooldown);
}

bool PistolWeapon::canFire() const {
    return m_ammo > 0 && m_timeSinceLastShot >= m_shotCooldown;
}

void PistolWeapon::recordFire() {
    if (m_ammo <= 0) {
        return;
    }
    m_timeSinceLastShot = 0.0f;
    --m_ammo;
}

const char* PistolWeapon::getDisplayName() const {
    return "Pistol";
}

int PistolWeapon::getAmmo() const {
    return m_ammo;
}
