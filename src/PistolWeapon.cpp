#include "PistolWeapon.hpp"

#include <algorithm>

PistolWeapon::PistolWeapon()
    : m_shotCooldown(0.5f)
    , m_timeSinceLastShot(0.5f) {
}

void PistolWeapon::update(float deltaTime) {
    m_timeSinceLastShot = std::min(m_timeSinceLastShot + deltaTime, m_shotCooldown);
}

bool PistolWeapon::canFire() const {
    return m_timeSinceLastShot >= m_shotCooldown;
}

void PistolWeapon::recordFire() {
    m_timeSinceLastShot = 0.0f;
}
