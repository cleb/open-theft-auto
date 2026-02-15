#include "Pickup.hpp"

#include "Renderer.hpp"
#include "TextureManager.hpp"

#include <algorithm>

Pickup::Pickup(PickupType type)
    : m_type(type)
    , m_size(pickupDefaultSize(type))
    , m_rotationSpeed(120.0f)
    , m_ammoAmount(10)
    , m_respawnDelay(30.0f)
    , m_respawnTimer(0.0f) {
}

bool Pickup::initialize() {
    const std::string texturePath = pickupTexturePath(m_type);
    m_texture = TextureManager::instance().getTextureFromPath(texturePath);
    if (!m_texture) {
        return false;
    }
    return true;
}

void Pickup::update(float deltaTime) {
    if (!m_active) {
        if (m_respawnTimer > 0.0f) {
            m_respawnTimer = std::max(0.0f, m_respawnTimer - deltaTime);
            if (m_respawnTimer <= 0.0f) {
                m_active = true;
            }
        }
        return;
    }

    m_rotation.z += m_rotationSpeed * deltaTime;
    if (m_rotation.z >= 360.0f) {
        m_rotation.z -= 360.0f;
    }
}

void Pickup::render(Renderer* renderer) {
    if (!m_active || !renderer || !m_texture) {
        return;
    }

    renderer->renderSprite(*m_texture, glm::vec2(m_position.x, m_position.y), m_size, m_rotation.z, glm::vec3(1.0f));
}

void Pickup::setAmmoAmount(int ammo) {
    m_ammoAmount = std::max(0, ammo);
}

void Pickup::setRespawnDelay(float seconds) {
    m_respawnDelay = std::max(0.0f, seconds);
}

void Pickup::startRespawn() {
    m_active = false;
    m_respawnTimer = m_respawnDelay;
}
