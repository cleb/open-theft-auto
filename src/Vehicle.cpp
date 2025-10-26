#include "Vehicle.hpp"
#include "Renderer.hpp"
#include "TileGrid.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

Vehicle::Vehicle() 
    : m_speed(0.0f)
    , m_maxSpeed(24.0f)
    , m_maxSpeedRoad(36.0f)
    , m_acceleration(12.0f)
    , m_turnSpeed(210.0f)
    , m_size(1.5f, 3.0f)
    , m_playerControlled(false)
    , m_tileGrid(nullptr) {
}

bool Vehicle::initialize(const std::string& texturePath) {
    if (!texturePath.empty()) {
        m_texture = std::make_shared<Texture>();
        if (!m_texture->loadFromFile(texturePath)) {
            std::cerr << "Failed to load vehicle texture: " << texturePath << std::endl;
            m_texture.reset();
        }
    }
    
    setPosition(glm::vec3(0.0f, 0.0f, 0.1f)); // Slightly above ground
    return true;
}

void Vehicle::update(float deltaTime) {
    // Apply friction so the car gradually coasts to a stop
    const float damping = m_playerControlled ? 0.985f : 0.95f;
    m_speed *= damping;
    if (std::abs(m_speed) < 0.01f) {
        m_speed = 0.0f;
    }
    
    // Move forward based on current speed
    if (std::abs(m_speed) > 0.01f) {
        float radians = glm::radians(m_rotation.z);
        glm::vec3 forward(std::sin(radians), std::cos(radians), 0.0f);
        glm::vec3 delta = forward * m_speed * deltaTime;

        if (m_tileGrid) {
            glm::vec3 newPosition = m_position;

            if (delta.x != 0.0f) {
                glm::vec3 candidate = newPosition + glm::vec3(delta.x, 0.0f, 0.0f);
                if (canMoveTo(newPosition, candidate)) {
                    newPosition.x = candidate.x;
                } else {
                    m_speed = 0.0f;
                    delta.x = 0.0f;
                }
            }

            if (delta.y != 0.0f) {
                glm::vec3 startForY = newPosition;
                glm::vec3 candidate = startForY + glm::vec3(0.0f, delta.y, 0.0f);
                if (canMoveTo(startForY, candidate)) {
                    newPosition.y = candidate.y;
                } else {
                    m_speed = 0.0f;
                }
            }

            setPosition(newPosition);
        } else {
            setPosition(m_position + delta);
        }
    }
}

void Vehicle::render(Renderer* renderer) {
    if (!m_active || !renderer) return;

    if (m_texture) {
        renderer->renderSprite(*m_texture, glm::vec2(m_position.x, m_position.y), m_size,
                               360.0f - m_rotation.z, glm::vec3(1.0f));
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
        float speedRatio = std::clamp(m_speed / maxSpeed, -1.0f, 1.0f);
        m_rotation.z += m_turnSpeed * deltaTime * speedRatio;
        if (m_rotation.z < 0.0f) {
            m_rotation.z += 360.0f;
        } else if (m_rotation.z >= 360.0f) {
            m_rotation.z -= 360.0f;
        }
    }
}

void Vehicle::turnLeft(float deltaTime) {
    if (std::abs(m_speed) > 0.1f) {
        float maxSpeed = getCurrentMaxSpeed();
        if (maxSpeed <= 0.0f) return;
        float speedRatio = std::clamp(m_speed / maxSpeed, -1.0f, 1.0f);
        m_rotation.z -= m_turnSpeed * deltaTime * speedRatio;
        if (m_rotation.z < 0.0f) {
            m_rotation.z += 360.0f;
        } else if (m_rotation.z >= 360.0f) {
            m_rotation.z -= 360.0f;
        }
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
    const float radians = glm::radians(m_rotation.z);
    const glm::vec3 forward(std::sin(radians), std::cos(radians), 0.0f);
    const glm::vec3 right(std::cos(radians), -std::sin(radians), 0.0f);
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