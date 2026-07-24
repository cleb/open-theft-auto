#include "Player.hpp"
#include "Renderer.hpp"
#include "Heading.hpp"
#include "TextureManager.hpp"
#include <glm/gtc/constants.hpp>
#include <iostream>

Player::Player(CharacterPhysics& physics)
    : Character(physics, glm::vec2(1.0f, 1.0f))
    , m_speed(5.0f)
    , m_rotationSpeed(90.0f)
    , m_isMoving(false)
    , m_equippedSlot(pickupTypeCount())
    , m_money(0) {
}

bool Player::initialize() {
    // Load animated walk cycle
    m_walkAnimation = std::make_unique<SpriteAnimation>();
    if (m_walkAnimation->loadFromFile("assets/textures/player-animation.json")) {
        // Calculate aspect ratio from frame dimensions for proper sprite size
        float aspectRatio = static_cast<float>(m_walkAnimation->getFrameWidth()) / 
                           static_cast<float>(m_walkAnimation->getFrameHeight());
        setCharacterSize(glm::vec2(1.0f * aspectRatio, 1.0f)); // Maintain aspect ratio
        
        // Start with idle (first frame of walk)
        m_walkAnimation->play("walk");
        m_walkAnimation->pause(); // Start paused until player moves
    } else {
        std::cerr << "Failed to load player walk animation, falling back to static texture" << std::endl;
        m_walkAnimation.reset();
        
        // Fallback to static texture
        m_texture = TextureManager::instance().getTextureFromPath("assets/textures/player.png");
    }
    
    setPosition(glm::vec3(0.0f, 0.0f, 0.1f)); // Slightly above ground
    
    return true;
}

void Player::update(float deltaTime) {
    if (auto* weapon = getEquippedWeapon()) {
        weapon->update(deltaTime);
    }

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

bool Player::canShoot() const {
    const auto* weapon = getEquippedWeapon();
    return weapon && weapon->canFire();
}

void Player::recordShot() {
    if (auto* weapon = getEquippedWeapon()) {
        weapon->recordFire();
    }
}

bool Player::hasWeapon() const {
    return m_equippedSlot < pickupTypeCount() && m_weaponSlots[m_equippedSlot] != nullptr;
}

bool Player::hasWeapon(PickupType type) const {
    return m_weaponSlots[pickupTypeToIndex(type)] != nullptr;
}

Weapon* Player::getWeapon(PickupType type) {
    return m_weaponSlots[pickupTypeToIndex(type)].get();
}

const Weapon* Player::getWeapon(PickupType type) const {
    return m_weaponSlots[pickupTypeToIndex(type)].get();
}

Weapon* Player::getEquippedWeapon() {
    if (m_equippedSlot >= pickupTypeCount()) {
        return nullptr;
    }
    return m_weaponSlots[m_equippedSlot].get();
}

const Weapon* Player::getEquippedWeapon() const {
    if (m_equippedSlot >= pickupTypeCount()) {
        return nullptr;
    }
    return m_weaponSlots[m_equippedSlot].get();
}

std::optional<PickupType> Player::getEquippedWeaponType() const {
    if (m_equippedSlot >= pickupTypeCount()) {
        return std::nullopt;
    }
    const auto& types = getAllPickupTypes();
    return types[m_equippedSlot];
}

const char* Player::getWeaponDisplayName() const {
    const auto* weapon = getEquippedWeapon();
    return weapon ? weapon->getDisplayName() : "Unarmed";
}

int Player::getWeaponAmmo() const {
    const auto* weapon = getEquippedWeapon();
    return weapon ? weapon->getAmmo() : 0;
}

void Player::addMoney(int amount) {
    if (amount <= 0) {
        return;
    }
    m_money += amount;
}

bool Player::addAmmo(PickupType type, int amount) {
    auto* weapon = getWeapon(type);
    if (!weapon) {
        return false;
    }
    weapon->addAmmo(amount);
    return true;
}

void Player::equipWeapon(PickupType type, std::unique_ptr<Weapon> weapon) {
    if (!weapon) {
        return;
    }
    std::size_t idx = pickupTypeToIndex(type);
    m_weaponSlots[idx] = std::move(weapon);
    m_equippedSlot = idx;
}

bool Player::equipWeaponType(PickupType type) {
    std::size_t idx = pickupTypeToIndex(type);
    if (!m_weaponSlots[idx]) {
        return false;
    }
    m_equippedSlot = idx;
    return true;
}

void Player::switchWeapon(int direction) {
    constexpr std::size_t count = pickupTypeCount();
    if (count == 0) return;

    // Start searching from the slot after (or before) the current one
    std::size_t start = (m_equippedSlot < count) ? m_equippedSlot : 0;
    for (std::size_t i = 1; i <= count; ++i) {
        std::size_t idx = (start + static_cast<std::size_t>(direction > 0 ? i : count - i)) % count;
        if (m_weaponSlots[idx] && idx != m_equippedSlot) {
            m_equippedSlot = idx;
            return;
        }
    }
}

void Player::switchWeaponNext() {
    switchWeapon(1);
}

void Player::switchWeaponPrev() {
    switchWeapon(-1);
}

void Player::render(Renderer* renderer) {
    if (!m_active || !renderer) return;
    
    if (m_walkAnimation && m_walkAnimation->getTexture()) {
        // Render animated sprite
        glm::vec4 uvOffsetScale = m_walkAnimation->getCurrentFrameUV();
        renderer->renderAnimatedSprite(*m_walkAnimation->getTexture(),
                                        m_position,
                                        getCharacterSize(), uvOffsetScale, m_rotation.z, glm::vec3(1.0f));
    } else if (m_texture) {
        // Fallback to static sprite
        renderer->renderSprite(*m_texture, m_position, getCharacterSize(), m_rotation.z, glm::vec3(1.0f));
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
    const CharacterMoveResult result = tryMove(delta, CharacterMoveMode::Slide);
    if (result.fallTiles > SurvivableFallTiles && m_fatalFallCallback) {
        m_fatalFallCallback(result.fallTiles);
    }
}

void Player::turnRight(float deltaTime) {
    // Heading convention: CW negative.
    m_rotation.z -= m_rotationSpeed * deltaTime;
    m_rotation.z = Heading::wrapDegrees360(m_rotation.z);
}
