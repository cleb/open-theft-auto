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
    // Let the pilot control the vehicle if one is assigned
    if (m_pilot) {
        m_pilot->update(this, m_tileGrid, deltaTime);
    }
    
    // Apply friction so the car gradually coasts to a stop
    bool hasUserPilot = m_pilot != nullptr; // TODO: could check pilot type
    const float damping = hasUserPilot ? 0.985f : 0.95f;
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
            glm::vec3 collisionDirection(0.0f);

            if (delta.x != 0.0f) {
                glm::vec3 candidate = newPosition + glm::vec3(delta.x, 0.0f, 0.0f);
                bool tileBlocked = !canMoveTo(newPosition, candidate);
                bool vehicleBlocked = wouldCollideWithOther(candidate);
                if (!tileBlocked && !vehicleBlocked) {
                    newPosition.x = candidate.x;
                } else {
                    if (vehicleBlocked && !m_inCollision) {
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
                    if (vehicleBlocked && !m_inCollision) {
                        collisionOccurred = true;
                        collisionDirection += glm::vec3(0.0f, delta.y > 0 ? 1.0f : -1.0f, 0.0f);
                    }
                    m_speed = 0.0f;
                }
            }

            // Apply damage if collision occurred (only once per collision)
            if (collisionOccurred && glm::length(collisionDirection) > 0.0f) {
                glm::vec3 collisionPoint = m_position + glm::normalize(collisionDirection) * glm::length(glm::vec2(m_size.x, m_size.y)) * 0.5f;
                applyDamageFromCollision(collisionPoint);
                m_inCollision = true;
            } else if (!wouldCollideWithOther(newPosition)) {
                // Clear collision state when no longer colliding
                m_inCollision = false;
            }

            setPosition(newPosition);
        } else {
            // No tile grid - still check vehicle collisions
            glm::vec3 newPosition = m_position + delta;
            if (!wouldCollideWithOther(newPosition)) {
                setPosition(newPosition);
                m_inCollision = false;
            } else {
                // Collision occurred - apply damage based on movement direction (only once)
                if (!m_inCollision) {
                    applyDamageFromCollision(newPosition);
                    m_inCollision = true;
                }
                m_speed = 0.0f;
            }
        }
    }
}

void Vehicle::setPilot(std::unique_ptr<Pilot> pilot) {
    m_pilot = std::move(pilot);
}

void Vehicle::clearPilot() {
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
        // Use GPU-accelerated damage rendering if we have damage
        if (m_damage.hasAnyDamage() && m_deltaTexture) {
            renderer->renderDamagedSprite(*m_texture, m_deltaTexture.get(),
                                          glm::vec2(m_position.x, m_position.y), m_size,
                                          m_rotation.z, glm::vec3(1.0f),
                                          m_damage.frontLeft, m_damage.frontRight,
                                          m_damage.rearLeft, m_damage.rearRight);
        } else {
            renderer->renderSprite(*m_texture, glm::vec2(m_position.x, m_position.y), m_size,
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
    m_speed += m_acceleration * deltaTime;
    float maxSpeed = getCurrentMaxSpeed();
    if (m_speed > maxSpeed) {
        m_speed = maxSpeed;
    }
}

void Vehicle::moveBackward(float deltaTime) {
    m_speed -= m_acceleration * deltaTime;
    float maxSpeed = getCurrentMaxSpeed() * 0.5f;
    if (m_speed < -maxSpeed) { // Reverse is slower
        m_speed = -maxSpeed;
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
    return isOnRoad() ? m_maxSpeedRoad : m_maxSpeed;
}

bool Vehicle::isOnRoad() const {
    if (!m_tileGrid) {
        return false;
    }
    return m_tileGrid->isRoadTile(m_position);
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
        
        // Update delta texture for the new vehicle type
        if (!def->deltaTexturePath.empty()) {
            m_deltaTexture = TextureManager::instance().getTextureFromPath(def->deltaTexturePath);
        }
    } else {
        // Fallback to defaults if type not found
        m_maxSpeed = DEFAULT_MAX_SPEED;
        m_maxSpeedRoad = DEFAULT_MAX_SPEED_ROAD;
        m_acceleration = DEFAULT_ACCELERATION;
        std::cerr << "Vehicle: Unknown vehicle type '" << typeId << "', using defaults" << std::endl;
    }
}