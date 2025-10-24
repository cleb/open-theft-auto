#include "Building.hpp"
#include "Renderer.hpp"
#include <iostream>

Building::Building() : m_size(2.0f, 2.0f, 4.0f) {
}

bool Building::initialize(const glm::vec3& size, const std::string& texturePath) {
    m_size = size;
    
    createBuildingMesh(size);
    
    if (!texturePath.empty()) {
        m_texture = std::make_shared<Texture>();
        if (!m_texture->loadFromFile(texturePath)) {
            std::cerr << "Failed to load building texture: " << texturePath << std::endl;
        }
        if (m_mesh) {
            m_mesh->setTexture(m_texture);
        }
    } else if (m_texture) {
        // Reapply existing texture if we already have one
        m_mesh->setTexture(m_texture);
    }
    
    return true;
}

void Building::update(float deltaTime) {
    // Buildings are static, no update needed
    (void)deltaTime; // Suppress unused parameter warning
}

void Building::render(Renderer* renderer) {
    if (!m_active || !renderer || !m_mesh) return;
    
    glm::mat4 modelMatrix = getModelMatrix();
    renderer->renderMesh(*m_mesh, modelMatrix, "model");
}

void Building::createBuildingMesh(const glm::vec3& size) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    float halfX = size.x * 0.5f;
    float halfY = size.y * 0.5f;
    float height = size.z;
    
    // Create a simple box mesh for the building
    // Bottom face (z = 0)
    vertices.push_back({{-halfX, -halfY, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ halfX, -halfY, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ halfX,  halfY, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-halfX,  halfY, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}});
    
    // Top face (z = height)
    vertices.push_back({{-halfX, -halfY, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ halfX, -halfY, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ halfX,  halfY, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-halfX,  halfY, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});
    
    // Front face (y = -halfY)
    vertices.push_back({{-halfX, -halfY, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ halfX, -halfY, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ halfX, -halfY, height}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-halfX, -halfY, height}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}});
    
    // Back face (y = halfY)
    vertices.push_back({{ halfX,  halfY, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{-halfX,  halfY, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{-halfX,  halfY, height}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{ halfX,  halfY, height}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    
    // Left face (x = -halfX)
    vertices.push_back({{-halfX,  halfY, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{-halfX, -halfY, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{-halfX, -halfY, height}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-halfX,  halfY, height}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
    
    // Right face (x = halfX)
    vertices.push_back({{ halfX, -halfY, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ halfX,  halfY, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ halfX,  halfY, height}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{ halfX, -halfY, height}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
    
    // Indices for each face (2 triangles per face)
    GLuint face_indices[] = {
        0, 1, 2,   2, 3, 0,    // Bottom
        4, 7, 6,   6, 5, 4,    // Top
        8, 9, 10,  10, 11, 8,  // Front
        12, 13, 14, 14, 15, 12, // Back
        16, 17, 18, 18, 19, 16, // Left
        20, 21, 22, 22, 23, 20  // Right
    };
    
    for (int i = 0; i < 36; ++i) {
        indices.push_back(face_indices[i]);
    }
    
    m_mesh = std::make_unique<Mesh>(vertices, indices);
    if (m_texture) {
        m_mesh->setTexture(m_texture);
    }
}