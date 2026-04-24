#include "Vehicle.hpp"
#include "Renderer.hpp"
#include "TileGrid.hpp"
#include "Heading.hpp"
#include "Pilot.hpp"
#include "TextureManager.hpp"
#include "VehicleConfig.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

// Default values if config not loaded
namespace {
    constexpr float DEFAULT_MAX_SPEED = 24.0f;
    constexpr float DEFAULT_MAX_SPEED_ROAD = 36.0f;
    constexpr float DEFAULT_ACCELERATION = 12.0f;
}

Vehicle::Vehicle() 
    : m_pilot(nullptr)
    , m_speed(0.0f)
    , m_maxSpeed(DEFAULT_MAX_SPEED)
    , m_maxSpeedRoad(DEFAULT_MAX_SPEED_ROAD)
    , m_acceleration(DEFAULT_ACCELERATION)
    , m_turnSpeed(200.0f)
    , m_size(1.5f, 3.0f)
    , m_tileGrid(nullptr)
    , m_vehicleTypeId("sedan") {
}

Vehicle::~Vehicle() = default;

bool Vehicle::initialize(const std::string& texturePath) {
    if (!texturePath.empty()) {
        m_texture = std::make_shared<Texture>();
        if (!m_texture->loadFromFile(texturePath)) {
            std::cerr << "Failed to load vehicle texture: " << texturePath << std::endl;
            m_texture.reset();
        }
    }
    
    // Load delta texture for damage effects based on vehicle type
    const auto* def = VehicleConfig::getInstance().getDefinition(m_vehicleTypeId);
    if (def && !def->deltaTexturePath.empty()) {
        m_deltaTexture = TextureManager::instance().getTextureFromPath(def->deltaTexturePath);
    }
    
    setPosition(glm::vec3(0.0f, 0.0f, 0.1f)); // Slightly above ground
    return true;
}

bool Vehicle::initialize(std::shared_ptr<Texture> texture) {
    m_texture = texture;
    
    // Load delta texture for damage effects based on vehicle type
    const auto* def = VehicleConfig::getInstance().getDefinition(m_vehicleTypeId);
    if (def && !def->deltaTexturePath.empty()) {
        m_deltaTexture = TextureManager::instance().getTextureFromPath(def->deltaTexturePath);
    }
    
    setPosition(glm::vec3(0.0f, 0.0f, 0.1f)); // Slightly above ground
    return true;
}

void Vehicle::update(float deltaTime) {
    if (!m_active) {
        return;
    }

    m_effectTime += deltaTime;
    if (m_collisionDamageCooldown > 0.0f) {
        m_collisionDamageCooldown = std::max(0.0f, m_collisionDamageCooldown - deltaTime);
    }

    if (m_exploding) {
        m_explosionTimer += deltaTime;
        if (m_explosionTimer >= 1.2f) {
            m_exploding = false;
            m_hasExploded = true;
            setExplodedTextureIfNeeded();
        }
    }

    if (m_burning && !m_exploding && !m_hasExploded) {
        m_burnTimer += deltaTime;
        while (m_burnTimer >= 10.0f && m_health > 0) {
            m_burnTimer -= 10.0f;
            applyHit(1);
        }
    }

    if (m_exploding || m_hasExploded) {
        m_speed = 0.0f;
        return;
    }

    // Let the pilot control the vehicle if one is assigned
    if (m_pilot) {
        m_pilot->update(this, m_tileGrid, deltaTime);
    }
    
    // Check if we're over the max speed for current surface and apply deceleration
    float maxSpeed = getCurrentMaxSpeed();
    if (std::abs(m_speed) > maxSpeed) {
        // Apply surface-based deceleration (gentler than braking)
        float surfaceDecel = m_acceleration * 0.5f * deltaTime;
        if (m_speed > maxSpeed) {
            m_speed -= surfaceDecel;
            if (m_speed < maxSpeed) m_speed = maxSpeed;
        } else if (m_speed < -maxSpeed * 0.5f) {  // Reverse max is half
            m_speed += surfaceDecel;
            if (m_speed > -maxSpeed * 0.5f) m_speed = -maxSpeed * 0.5f;
        }
    }
    
    // Apply friction so the car gradually coasts to a stop
    // Use frame-rate independent damping: damping^(dt * 60) so behavior is
    // consistent regardless of frame rate (calibrated for 60 fps baseline).
    bool hasUserPilot = m_pilot != nullptr; // TODO: could check pilot type
    const float baseDamping = hasUserPilot ? 0.985f : 0.95f;
    const float damping = std::pow(baseDamping, deltaTime * 60.0f);
    m_speed *= damping;
    if (std::abs(m_speed) < 0.01f) {
        m_speed = 0.0f;
    }
    
    // Move forward based on current speed
    if (std::abs(m_speed) > 0.01f) {
        // Heading convention: 0° = +X (East), 90° = +Y (North), CCW positive.
        glm::vec2 f2 = Heading::forwardFromHeadingDeg(m_rotation.z);
        glm::vec3 forward(f2.x, f2.y, 0.0f);
        glm::vec3 delta = forward * m_speed * deltaTime;

        if (m_tileGrid) {
            glm::vec3 newPosition = m_position;
            bool collisionOccurred = false;
            bool blockedThisFrame = false;
            glm::vec3 collisionDirection(0.0f);

            if (delta.x != 0.0f) {
                glm::vec3 candidate = newPosition + glm::vec3(delta.x, 0.0f, 0.0f);
                bool tileBlocked = !canMoveTo(newPosition, candidate);
                bool vehicleBlocked = wouldCollideWithOther(candidate);
                if (!tileBlocked && !vehicleBlocked) {
                    newPosition.x = candidate.x;
                } else {
                    blockedThisFrame = true;
                    if (!m_inCollision) {
                        collisionOccurred = true;
                        collisionDirection += glm::vec3(delta.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
                    }
                    m_speed = 0.0f;
                    delta.x = 0.0f;
                }
            }

            if (delta.y != 0.0f) {
                glm::vec3 startForY = newPosition;
                glm::vec3 candidate = startForY + glm::vec3(0.0f, delta.y, 0.0f);
                bool tileBlocked = !canMoveTo(startForY, candidate);
                bool vehicleBlocked = wouldCollideWithOther(candidate);
                if (!tileBlocked && !vehicleBlocked) {
                    newPosition.y = candidate.y;
                } else {
                    blockedThisFrame = true;
                    if (!m_inCollision) {
                        collisionOccurred = true;
                        collisionDirection += glm::vec3(0.0f, delta.y > 0 ? 1.0f : -1.0f, 0.0f);
                    }
                    m_speed = 0.0f;
                }
            }

            // Apply damage if collision occurred (only once per collision)
            if (collisionOccurred && glm::length(collisionDirection) > 0.0f && m_collisionDamageCooldown <= 0.0f) {
                glm::vec3 collisionPoint = m_position + glm::normalize(collisionDirection) * glm::length(glm::vec2(m_size.x, m_size.y)) * 0.5f;
                applyDamageFromCollision(collisionPoint);
                m_collisionDamageCooldown = 0.4f;
            }

            m_inCollision = blockedThisFrame;

            // Follow ground surface (handle slopes). Use the vehicle's front
            // edge so the body lifts as soon as it starts climbing a ramp.
            const glm::vec2 movement(newPosition.x - m_position.x, newPosition.y - m_position.y);
            newPosition.z = m_tileGrid->getSurfaceHeightForFootprint(newPosition, m_size, movement, m_position.z);

            setPosition(newPosition);
        } else {
            // No tile grid - still check vehicle collisions
            glm::vec3 newPosition = m_position + delta;
            if (!wouldCollideWithOther(newPosition)) {
                setPosition(newPosition);
                m_inCollision = false;
            } else {
                // Collision occurred - apply damage based on movement direction (only once)
                if (!m_inCollision && m_collisionDamageCooldown <= 0.0f) {
                    applyDamageFromCollision(newPosition);
                    m_collisionDamageCooldown = 0.4f;
                    m_inCollision = true;
                }
                m_speed = 0.0f;
            }
        }
    }
}

void Vehicle::setPilot(std::unique_ptr<Pilot> pilot) {
    m_pilot = std::move(pilot);
    if (m_pilot) {
        m_pilot->onAssign(this);
    }
}

void Vehicle::clearPilot() {
    if (m_pilot) {
        m_pilot->onRelease(this);
    }
    m_pilot.reset();
}

void Vehicle::triggerCarjack() {
    if (m_carjackCallback) {
        m_carjackCallback(m_position, m_rotation.z, m_size);
        // Clear the callback after triggering (one-time event)
        m_carjackCallback = nullptr;
    }
}

void Vehicle::render(Renderer* renderer) {
    if (!m_active || !renderer) return;

    if (m_texture) {
        if (m_exploding) {
            float progress = std::min(1.0f, m_explosionTimer / 1.2f);
            renderer->renderExplosionSprite(*m_texture, m_position, m_size,
                                            m_rotation.z, glm::vec3(1.0f), progress);
        } else if (m_burning) {
            // Burning vehicles: render with damage overlay if applicable, then fire on top
            if (m_damage.hasAnyDamage() && m_deltaTexture) {
                renderer->renderFireDamagedSprite(*m_texture, m_deltaTexture.get(),
                                                  m_position, m_size,
                                                  m_rotation.z, glm::vec3(1.0f),
                                                  m_damage.frontLeft, m_damage.frontRight,
                                                  m_damage.rearLeft, m_damage.rearRight,
                                                  getFireIntensity(), m_effectTime);
            } else {
                renderer->renderFireSprite(*m_texture, m_position, m_size,
                                           m_rotation.z, glm::vec3(1.0f), getFireIntensity(), m_effectTime);
            }
        } else if (m_hasExploded) {
            renderer->renderSprite(*m_texture, m_position, m_size,
                                   m_rotation.z, glm::vec3(1.0f));
        } else if (m_damage.hasAnyDamage() && m_deltaTexture) {
            renderer->renderDamagedSprite(*m_texture, m_deltaTexture.get(),
                                          m_position, m_size,
                                          m_rotation.z, glm::vec3(1.0f),
                                          m_damage.frontLeft, m_damage.frontRight,
                                          m_damage.rearLeft, m_damage.rearRight);
        } else {
            renderer->renderSprite(*m_texture, m_position, m_size,
                                   m_rotation.z, glm::vec3(1.0f));
        }
        return;
    }
    
    // Create a simple car mesh if we don't have one
    static std::shared_ptr<Mesh> carMesh = nullptr;
    if (!carMesh) {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        
        float length = 0.8f;
        float width = 0.5f;
        float height = 0.4f;
        
        // Car body
        vertices.push_back({{-width, -length, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
        vertices.push_back({{ width, -length, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}});
        vertices.push_back({{ width,  length, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}});
        vertices.push_back({{-width,  length, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{-width, -length, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
        vertices.push_back({{ width, -length, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}});
        vertices.push_back({{ width,  length, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}});
        vertices.push_back({{-width,  length, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{-width, -length, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
        vertices.push_back({{ width, -length, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}});
        vertices.push_back({{ width, -length, height}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}});
        vertices.push_back({{-width, -length, height}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{ width,  length, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
        vertices.push_back({{-width,  length, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
        vertices.push_back({{-width,  length, height}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
        vertices.push_back({{ width,  length, height}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{-width,  length, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        vertices.push_back({{-width, -length, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
        vertices.push_back({{-width, -length, height}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
        vertices.push_back({{-width,  length, height}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{ width, -length, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        vertices.push_back({{ width,  length, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
        vertices.push_back({{ width,  length, height}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
        vertices.push_back({{ width, -length, height}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
        
        GLuint face_indices[] = {
            0, 1, 2,   2, 3, 0,
            4, 7, 6,   6, 5, 4,
            8, 9, 10,  10, 11, 8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20
        };
        
        for (int i = 0; i < 36; ++i) {
            indices.push_back(face_indices[i]);
        }
        
        carMesh = std::make_shared<Mesh>(vertices, indices);
    }
    if (m_texture) {
        carMesh->setTexture(m_texture);
    }
    
    glm::mat4 modelMatrix = getModelMatrix();
    renderer->renderMesh(*carMesh, modelMatrix, "vehicle");
}

void Vehicle::moveForward(float deltaTime) {
    float maxSpeed = getCurrentMaxSpeed();
    
    // If under max speed, accelerate normally
    if (m_speed < maxSpeed) {
        m_speed += m_acceleration * deltaTime;
        if (m_speed > maxSpeed) {
            m_speed = maxSpeed;
        }
    } else if (m_speed > maxSpeed) {
        // If over max speed (e.g., entered rough terrain), decelerate gradually
        // Use a gentler deceleration than braking - surface resistance slows you down
        float surfaceDecel = m_acceleration * 0.5f;  // Half the normal acceleration
        m_speed -= surfaceDecel * deltaTime;
        if (m_speed < maxSpeed) {
            m_speed = maxSpeed;
        }
    }
}

void Vehicle::moveBackward(float deltaTime) {
    float maxSpeed = getCurrentMaxSpeed() * 0.5f;  // Reverse is slower
    
    // If not at reverse max speed, accelerate backward
    if (m_speed > -maxSpeed) {
        m_speed -= m_acceleration * deltaTime;
        if (m_speed < -maxSpeed) {
            m_speed = -maxSpeed;
        }
    } else if (m_speed < -maxSpeed) {
        // If going faster in reverse than allowed, decelerate
        float surfaceDecel = m_acceleration * 0.5f;
        m_speed += surfaceDecel * deltaTime;
        if (m_speed > -maxSpeed) {
            m_speed = -maxSpeed;
        }
    }
}

void Vehicle::turnRight(float deltaTime) {
    if (std::abs(m_speed) > 0.1f) {
        float maxSpeed = getCurrentMaxSpeed();
        if (maxSpeed <= 0.0f) return;
        // Minimum turn rate of 50% even at low speeds, scales up to 100% at high speed
        float speedRatio = std::abs(m_speed) / maxSpeed;
        float turnFactor = 0.5f + 0.5f * std::clamp(speedRatio, 0.0f, 1.0f);
        // Reverse direction when going backward
        if (m_speed < 0) turnFactor = -turnFactor;
        // Heading convention: CCW positive, so turning right (clockwise) decreases heading.
        m_rotation.z -= m_turnSpeed * deltaTime * turnFactor;
        m_rotation.z = Heading::wrapDegrees360(m_rotation.z);
    }
}

void Vehicle::turnLeft(float deltaTime) {
    if (std::abs(m_speed) > 0.1f) {
        float maxSpeed = getCurrentMaxSpeed();
        if (maxSpeed <= 0.0f) return;
        // Minimum turn rate of 50% even at low speeds, scales up to 100% at high speed
        float speedRatio = std::abs(m_speed) / maxSpeed;
        float turnFactor = 0.5f + 0.5f * std::clamp(speedRatio, 0.0f, 1.0f);
        // Reverse direction when going backward
        if (m_speed < 0) turnFactor = -turnFactor;
        // Heading convention: CCW positive, so turning left increases heading.
        m_rotation.z += m_turnSpeed * deltaTime * turnFactor;
        m_rotation.z = Heading::wrapDegrees360(m_rotation.z);
    }
}

float Vehicle::getCurrentMaxSpeed() const {
    float drivability = getCurrentDrivability();
    
    // Base speed depends on whether we're on a well-paved surface
    // Drivability of 1.0 means full speed, lower values reduce max speed
    float baseSpeed = m_maxSpeedRoad;
    
    // Apply drivability effect based on vehicle's resistance to poor surfaces
    const auto* def = VehicleConfig::getInstance().getDefinition(m_vehicleTypeId);
    float impact = def ? def->drivabilityImpact : 1.0f;
    
    // Calculate effective speed multiplier:
    // drivability=1.0 -> multiplier=1.0 (full speed)
    // drivability=0.0 -> multiplier=(1-impact) (minimum speed based on vehicle's resistance)
    // For impact=1.0 (fully affected): multiplier = drivability
    // For impact=0.0 (immune): multiplier = 1.0
    float multiplier = 1.0f - (impact * (1.0f - drivability));
    
    return baseSpeed * multiplier;
}

float Vehicle::getCurrentDrivability() const {
    if (!m_tileGrid) {
        return 0.5f;  // Default moderate drivability
    }
    return m_tileGrid->getDrivability(m_position);
}

bool Vehicle::isOnRoad() const {
    // Consider a surface "road-like" if drivability is high
    return getCurrentDrivability() >= 0.9f;
}

std::array<glm::vec3, 8> Vehicle::getCollisionOffsets() const {
    const float halfWidth = m_size.x * 0.5f;
    const float halfLength = m_size.y * 0.5f;
    glm::vec2 f2 = Heading::forwardFromHeadingDeg(m_rotation.z);
    const glm::vec3 forward(f2.x, f2.y, 0.0f);
    // Right is +90° clockwise from forward.
    const glm::vec3 right(forward.y, -forward.x, 0.0f);
    return {
        forward * halfLength + right * halfWidth,
        forward * halfLength - right * halfWidth,
        -forward * halfLength + right * halfWidth,
        -forward * halfLength - right * halfWidth,
        forward * halfLength,
        -forward * halfLength,
        right * halfWidth,
        -right * halfWidth
    };
}

bool Vehicle::canMoveTo(const glm::vec3& from, const glm::vec3& to) const {
    if (!m_tileGrid) {
        return true;
    }

    if (!m_tileGrid->canOccupy(from, to)) {
        return false;
    }

    const auto offsets = getCollisionOffsets();
    for (const auto& offset : offsets) {
        if (!m_tileGrid->canOccupy(from + offset, to + offset)) {
            return false;
        }
    }
    return true;
}

bool Vehicle::wouldCollideWithOther(const glm::vec3& newPosition) const {
    return m_collisionManager.wouldCollide(this, newPosition, m_rotation.z);
}

void Vehicle::setDeltaTexture(std::shared_ptr<Texture> deltaTexture) {
    m_deltaTexture = deltaTexture;
}

void Vehicle::applyDamage(CollisionDirection direction) {
    if (direction == CollisionDirection::None) {
        return;
    }

    applyHit(1);
    
    // Just set the damage flags - GPU shader handles the visual effect
    switch (direction) {
        case CollisionDirection::Front:
            m_damage.frontLeft = true;
            m_damage.frontRight = true;
            break;
        case CollisionDirection::Rear:
            m_damage.rearLeft = true;
            m_damage.rearRight = true;
            break;
        case CollisionDirection::Left:
            m_damage.frontLeft = true;
            m_damage.rearLeft = true;
            break;
        case CollisionDirection::Right:
            m_damage.frontRight = true;
            m_damage.rearRight = true;
            break;
        default:
            break;
    }
}

void Vehicle::applyDamageFromCollision(const glm::vec3& collisionPoint) {
    CollisionDirection direction = determineCollisionDirection(collisionPoint);
    applyDamage(direction);
}

void Vehicle::resetDamage() {
    m_damage.reset();
}

void Vehicle::applyHit(int amount) {
    if (amount <= 0 || m_exploding || m_hasExploded) {
        return;
    }

    m_health = std::max(0, m_health - amount);

    if (!m_burning && m_health > 0 && getHealthFraction() <= 0.4f) {
        startBurning();
    }

    if (m_health <= 0) {
        triggerExplosion();
    }
}

CollisionDirection Vehicle::determineCollisionDirection(const glm::vec3& collisionPoint) const {
    // Get direction from vehicle center to collision point in world space
    glm::vec2 toCollision(collisionPoint.x - m_position.x, collisionPoint.y - m_position.y);
    
    if (glm::length(toCollision) < 0.001f) {
        // Collision point is at center - use movement direction (speed)
        if (m_speed > 0) {
            return CollisionDirection::Front;
        } else if (m_speed < 0) {
            return CollisionDirection::Rear;
        }
        return CollisionDirection::Front;
    }
    
    toCollision = glm::normalize(toCollision);
    
    // Get vehicle's forward and right vectors
    glm::vec2 forward = Heading::forwardFromHeadingDeg(m_rotation.z);
    glm::vec2 right(forward.y, -forward.x);  // 90° clockwise
    
    // Project collision direction onto vehicle's local axes
    float forwardDot = glm::dot(toCollision, forward);
    float rightDot = glm::dot(toCollision, right);
    
    // Determine primary collision direction based on larger component
    if (std::abs(forwardDot) > std::abs(rightDot)) {
        // Front or rear collision
        return forwardDot > 0 ? CollisionDirection::Front : CollisionDirection::Rear;
    } else {
        // Left or right collision
        return rightDot > 0 ? CollisionDirection::Right : CollisionDirection::Left;
    }
}

void Vehicle::setVehicleType(const std::string& typeId) {
    m_vehicleTypeId = typeId;
    
    const auto* def = VehicleConfig::getInstance().getDefinition(typeId);
    if (def) {
        m_maxSpeed = def->maxSpeed;
        m_maxSpeedRoad = def->maxSpeed + def->maxSpeedVariance;
        m_acceleration = def->acceleration;
        m_maxHealth = def->maxHealth;
        m_health = m_maxHealth;
        m_burning = false;
        m_burnTimer = 0.0f;
        m_exploding = false;
        m_hasExploded = false;
        m_explosionTimer = 0.0f;
        m_effectTime = 0.0f;
        m_wasShotByPlayer = false;
        
        // Update delta texture for the new vehicle type
        if (!def->deltaTexturePath.empty()) {
            m_deltaTexture = TextureManager::instance().getTextureFromPath(def->deltaTexturePath);
        }
    } else {
        // Fallback to defaults if type not found
        m_maxSpeed = DEFAULT_MAX_SPEED;
        m_maxSpeedRoad = DEFAULT_MAX_SPEED_ROAD;
        m_acceleration = DEFAULT_ACCELERATION;
        m_maxHealth = 10;
        m_health = m_maxHealth;
        m_wasShotByPlayer = false;
        std::cerr << "Vehicle: Unknown vehicle type '" << typeId << "', using defaults" << std::endl;
    }
}

float Vehicle::getHealthFraction() const {
    if (m_maxHealth <= 0) {
        return 0.0f;
    }
    return static_cast<float>(m_health) / static_cast<float>(m_maxHealth);
}

float Vehicle::getFireIntensity() const {
    if (!m_burning || m_maxHealth <= 0) {
        return 0.0f;
    }

    const float fraction = getHealthFraction();
    const float normalized = std::clamp((0.4f - fraction) / 0.4f, 0.0f, 1.0f);
    return 0.25f + normalized * 0.75f;
}

void Vehicle::startBurning() {
    m_burning = true;
    m_burnTimer = 0.0f;
}

void Vehicle::triggerExplosion() {
    if (m_exploding || m_hasExploded) {
        return;
    }

    m_burning = false;
    m_exploding = true;
    m_explosionTimer = 0.0f;
    m_speed = 0.0f;
    clearPilot();
    resetDamage();

    if (m_explodeCallback) {
        m_explodeCallback(this);
    }
}

void Vehicle::setExplodedTextureIfNeeded() {
    if (!m_explodedTexture) {
        m_explodedTexture = TextureManager::instance().getTextureFromPath("assets/textures/car-exploded.png");
    }

    if (m_explodedTexture) {
        m_texture = m_explodedTexture;
    }
    m_deltaTexture.reset();
}
