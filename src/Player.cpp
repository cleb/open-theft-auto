#include "Player.hpp"
#include "Renderer.hpp"
#include "TileGrid.hpp"
#include <glm/gtc/constants.hpp>
#include <iostream>

Player::Player() 
    : m_speed(5.0f)
    , m_rotationSpeed(90.0f)
    , m_size(1.0f, 1.0f)
    , m_tileGrid(nullptr) {
}

bool Player::initialize() {
    m_texture = std::make_unique<Texture>();
    m_texture->loadFromFile("assets/textures/player.png");
    
    setPosition(glm::vec3(0.0f, 0.0f, 0.1f)); // Slightly above ground
    m_size = glm::vec2(1.0f, 1.0f); // Sprite size
    
    return true;
}

void Player::update(float deltaTime) {
    // Player updates will be handled by input in the main game loop
    (void)deltaTime; // Suppress unused parameter warning
}

void Player::render(Renderer* renderer) {
    if (!m_active || !renderer || !m_texture) return;
    
    // Render as a flat sprite (billboard)
    renderer->renderSprite(*m_texture, glm::vec2(m_position.x, m_position.y), m_size, 360.0f - m_rotation.z, glm::vec3(1.0f));
}

void Player::moveForward(float deltaTime) {
    // Move in the direction the player is facing
    float angleRadians = glm::radians(m_rotation.z);
    glm::vec3 delta(
        sin(angleRadians) * m_speed * deltaTime,
        cos(angleRadians) * m_speed * deltaTime,
        0.0f
    );
    applyMovement(delta);
}

void Player::moveBackward(float deltaTime) {
    float angleRadians = glm::radians(m_rotation.z);
    glm::vec3 delta(
        -sin(angleRadians) * m_speed * deltaTime,
        -cos(angleRadians) * m_speed * deltaTime,
        0.0f
    );
    applyMovement(delta);
}

void Player::turnLeft(float deltaTime) {
    // Rotate counter-clockwise (decrease rotation angle)
    m_rotation.z -= m_rotationSpeed * deltaTime;
    // Keep rotation in 0-360 range
    if (m_rotation.z < 0.0f) {
        m_rotation.z += 360.0f;
    }
}

void Player::applyMovement(const glm::vec3& delta) {
    if (delta.x == 0.0f && delta.y == 0.0f) {
        return;
    }

    glm::vec3 newPosition = m_position;

    if (!m_tileGrid) {
        newPosition += delta;
        setPosition(newPosition);
        return;
    }

    // Resolve each axis separately so the player can slide along blocking walls.
    if (delta.x != 0.0f) {
        glm::vec3 target = newPosition + glm::vec3(delta.x, 0.0f, 0.0f);
        if (m_tileGrid->canOccupy(newPosition, target)) {
            newPosition.x = target.x;
        }
    }

    if (delta.y != 0.0f) {
        glm::vec3 startForY = newPosition;
        glm::vec3 target = startForY + glm::vec3(0.0f, delta.y, 0.0f);
        if (m_tileGrid->canOccupy(startForY, target)) {
            newPosition.y = target.y;
        }
    }

    setPosition(newPosition);
}

void Player::turnRight(float deltaTime) {
    // Rotate clockwise (increase rotation angle)
    m_rotation.z += m_rotationSpeed * deltaTime;
    // Keep rotation in 0-360 range
    if (m_rotation.z >= 360.0f) {
        m_rotation.z -= 360.0f;
    }
}