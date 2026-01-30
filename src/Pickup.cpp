#include "Pickup.hpp"

#include "Renderer.hpp"
#include "TextureManager.hpp"

Pickup::Pickup(PickupType type)
    : m_type(type)
    , m_size(pickupDefaultSize(type))
    , m_rotationSpeed(120.0f) {
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
