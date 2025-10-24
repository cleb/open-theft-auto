#include "Vehicle.hpp"
#include "Renderer.hpp"
#include <iostream>

Vehicle::Vehicle() 
    : m_speed(0.0f)
    , m_maxSpeed(15.0f)
    , m_acceleration(5.0f)
    , m_turnSpeed(120.0f)
    , m_size(1.5f, 3.0f)
    , m_playerControlled(false) {
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
    const float damping = m_playerControlled ? 0.98f : 0.95f;
    m_speed *= damping;
    if (std::abs(m_speed) < 0.01f) {
        m_speed = 0.0f;
    }
    
    // Move forward based on current speed
    if (std::abs(m_speed) > 0.01f) {
        float radians = glm::radians(m_rotation.z);
        glm::vec3 forward(std::sin(radians), std::cos(radians), 0.0f);
        m_position += forward * m_speed * deltaTime;
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

void Vehicle::accelerate(float deltaTime) {
    m_speed += m_acceleration * deltaTime;
    if (m_speed > m_maxSpeed) {
        m_speed = m_maxSpeed;
    }
}

void Vehicle::brake(float deltaTime) {
    m_speed -= m_acceleration * deltaTime;
    if (m_speed < -m_maxSpeed * 0.5f) { // Reverse is slower
        m_speed = -m_maxSpeed * 0.5f;
    }
}

void Vehicle::turnLeft(float deltaTime) {
    if (std::abs(m_speed) > 0.1f) {
        m_rotation.z += m_turnSpeed * deltaTime * (m_speed / m_maxSpeed);
    }
}

void Vehicle::turnRight(float deltaTime) {
    if (std::abs(m_speed) > 0.1f) {
        m_rotation.z -= m_turnSpeed * deltaTime * (m_speed / m_maxSpeed);
    }
}