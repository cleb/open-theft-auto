#include "Player.hpp"
#include "Renderer.hpp"
#include <iostream>

Player::Player() 
    : m_speed(5.0f)
    , m_rotationSpeed(90.0f)
    , m_size(1.0f, 1.0f) {
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
    renderer->renderSprite(*m_texture, glm::vec2(m_position.x, m_position.y), m_size, m_rotation.z, glm::vec3(1.0f));
}

void Player::moveForward(float deltaTime) {
    // Move in positive Y direction (away from camera)
    m_position.y += m_speed * deltaTime;
}

void Player::moveBackward(float deltaTime) {
    // Move in negative Y direction (toward camera)
    m_position.y -= m_speed * deltaTime;
}

void Player::turnLeft(float deltaTime) {
    // Move in negative X direction (left)
    m_position.x -= m_speed * deltaTime;
}

void Player::turnRight(float deltaTime) {
    // Move in positive X direction (right)
    m_position.x += m_speed * deltaTime;
}