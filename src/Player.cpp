#include "Player.hpp"
#include "Renderer.hpp"
#include "TileGrid.hpp"
#include "Heading.hpp"
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <algorithm>

Player::Player() 
    : m_speed(5.0f)
    , m_rotationSpeed(90.0f)
    , m_size(1.0f, 1.0f)
    , m_tileGrid(nullptr)
    , m_isMoving(false)
    , m_hasPistol(false)
    , m_shotCooldown(0.5f)
    , m_timeSinceLastShot(0.5f) {
}

bool Player::initialize() {
    // Load animated walk cycle
    m_walkAnimation = std::make_unique<SpriteAnimation>();
    if (m_walkAnimation->loadFromFile("assets/textures/player-animation.json")) {
        // Calculate aspect ratio from frame dimensions for proper sprite size
        float aspectRatio = static_cast<float>(m_walkAnimation->getFrameWidth()) / 
                           static_cast<float>(m_walkAnimation->getFrameHeight());
        m_size = glm::vec2(1.0f * aspectRatio, 1.0f); // Maintain aspect ratio
        
        // Start with idle (first frame of walk)
        m_walkAnimation->play("walk");
        m_walkAnimation->pause(); // Start paused until player moves
    } else {
        std::cerr << "Failed to load player walk animation, falling back to static texture" << std::endl;
        m_walkAnimation.reset();
        
        // Fallback to static texture
        m_texture = std::make_unique<Texture>();
        m_texture->loadFromFile("assets/textures/player.png");
    }
    
    setPosition(glm::vec3(0.0f, 0.0f, 0.1f)); // Slightly above ground
    
    return true;
}

void Player::update(float deltaTime) {
    m_timeSinceLastShot = std::min(m_timeSinceLastShot + deltaTime, m_shotCooldown);

    // Update animation
    if (m_walkAnimation) {
        if (m_isMoving) {
            m_walkAnimation->resume();
        } else {
            m_walkAnimation->pause();
            m_walkAnimation->reset(); // Reset to first frame when idle
        }
        m_walkAnimation->update(deltaTime);
    }
    
    // Reset movement flag - will be set again if player moves this frame
    m_isMoving = false;
}

void Player::givePistol() {
    m_hasPistol = true;
    m_timeSinceLastShot = m_shotCooldown;
}

bool Player::canShoot() const {
    return m_hasPistol && m_timeSinceLastShot >= m_shotCooldown;
}

void Player::recordShot() {
    m_timeSinceLastShot = 0.0f;
}

void Player::render(Renderer* renderer) {
    if (!m_active || !renderer) return;
    
    if (m_walkAnimation && m_walkAnimation->getTexture()) {
        // Render animated sprite
        glm::vec4 uvOffsetScale = m_walkAnimation->getCurrentFrameUV();
        renderer->renderAnimatedSprite(*m_walkAnimation->getTexture(), 
                                        glm::vec2(m_position.x, m_position.y), 
                                        m_size, uvOffsetScale, m_rotation.z, glm::vec3(1.0f));
    } else if (m_texture) {
        // Fallback to static sprite
        renderer->renderSprite(*m_texture, glm::vec2(m_position.x, m_position.y), m_size, m_rotation.z, glm::vec3(1.0f));
    }
}

void Player::moveForward(float deltaTime) {
    // Move in the direction the player is facing
    glm::vec2 f2 = Heading::forwardFromHeadingDeg(m_rotation.z);
    glm::vec3 delta(f2.x * m_speed * deltaTime, f2.y * m_speed * deltaTime, 0.0f);
    applyMovement(delta);
    m_isMoving = true;
}

void Player::moveBackward(float deltaTime) {
    glm::vec2 f2 = Heading::forwardFromHeadingDeg(m_rotation.z);
    glm::vec3 delta(-f2.x * m_speed * deltaTime, -f2.y * m_speed * deltaTime, 0.0f);
    applyMovement(delta);
    m_isMoving = true;
}

void Player::turnLeft(float deltaTime) {
    // Heading convention: CCW positive.
    m_rotation.z += m_rotationSpeed * deltaTime;
    // Keep rotation in 0-360 range
    m_rotation.z = Heading::wrapDegrees360(m_rotation.z);
}

void Player::applyMovement(const glm::vec3& delta) {
    if (delta.x == 0.0f && delta.y == 0.0f) {
        return;
    }

    glm::vec3 newPosition = m_position;

    if (!m_tileGrid) {
        // No tile grid - just check collisions
        glm::vec3 target = newPosition + delta;
        if (!m_collisionManager.hasCallback() || 
            !m_collisionManager.wouldCollide(this, target, m_rotation.z)) {
            newPosition = target;
        }
        setPosition(newPosition);
        return;
    }

    // Resolve each axis separately so the player can slide along blocking walls.
    if (delta.x != 0.0f) {
        glm::vec3 target = newPosition + glm::vec3(delta.x, 0.0f, 0.0f);
        bool canMove = m_tileGrid->canOccupy(newPosition, target);
        if (canMove && m_collisionManager.hasCallback()) {
            canMove = !m_collisionManager.wouldCollide(this, target, m_rotation.z);
        }
        if (canMove) {
            newPosition.x = target.x;
        }
    }

    if (delta.y != 0.0f) {
        glm::vec3 startForY = newPosition;
        glm::vec3 target = startForY + glm::vec3(0.0f, delta.y, 0.0f);
        bool canMove = m_tileGrid->canOccupy(startForY, target);
        if (canMove && m_collisionManager.hasCallback()) {
            canMove = !m_collisionManager.wouldCollide(this, target, m_rotation.z);
        }
        if (canMove) {
            newPosition.y = target.y;
        }
    }

    setPosition(newPosition);
}

void Player::turnRight(float deltaTime) {
    // Heading convention: CW negative.
    m_rotation.z -= m_rotationSpeed * deltaTime;
    m_rotation.z = Heading::wrapDegrees360(m_rotation.z);
}