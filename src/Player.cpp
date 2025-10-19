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
    // For now, we'll create without loading a texture file
    // In a real implementation, you'd load: m_texture->loadFromFile("assets/textures/player.png");
    
    setPosition(glm::vec3(0.0f, 0.0f, 0.1f)); // Slightly above ground
    
    return true;
}

void Player::update(float deltaTime) {
    // Player updates will be handled by input in the main game loop
    (void)deltaTime; // Suppress unused parameter warning
}

void Player::render(Renderer* renderer) {
    if (!m_active || !renderer) return;
    
    // Create a simple player mesh if we don't have one
    static std::unique_ptr<Mesh> playerMesh = nullptr;
    if (!playerMesh) {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        
        float size = 0.4f;
        float height = 0.8f;
        
        // Simple box for player
        vertices.push_back({{-size, -size, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
        vertices.push_back({{ size, -size, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}});
        vertices.push_back({{ size,  size, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}});
        vertices.push_back({{-size,  size, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{-size, -size, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
        vertices.push_back({{ size, -size, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}});
        vertices.push_back({{ size,  size, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}});
        vertices.push_back({{-size,  size, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{-size, -size, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
        vertices.push_back({{ size, -size, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}});
        vertices.push_back({{ size, -size, height}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}});
        vertices.push_back({{-size, -size, height}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{ size,  size, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
        vertices.push_back({{-size,  size, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
        vertices.push_back({{-size,  size, height}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
        vertices.push_back({{ size,  size, height}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{-size,  size, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        vertices.push_back({{-size, -size, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
        vertices.push_back({{-size, -size, height}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
        vertices.push_back({{-size,  size, height}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
        
        vertices.push_back({{ size, -size, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        vertices.push_back({{ size,  size, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
        vertices.push_back({{ size,  size, height}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
        vertices.push_back({{ size, -size, height}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
        
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
        
        playerMesh = std::make_unique<Mesh>(vertices, indices);
    }
    
    glm::mat4 modelMatrix = getModelMatrix();
    renderer->renderMesh(*playerMesh, modelMatrix, "player");
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