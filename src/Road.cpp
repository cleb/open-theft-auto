#include "Road.hpp"
#include "Renderer.hpp"

Road::Road() : m_size(10.0f, 10.0f) {
}

bool Road::initialize(const glm::vec2& size) {
    m_size = size;
    createRoadMesh(size);
    return true;
}

void Road::update(float deltaTime) {
    // Roads are static
    (void)deltaTime;
}

void Road::render(Renderer* renderer) {
    if (!m_active || !renderer || !m_mesh) return;
    
    glm::mat4 modelMatrix = getModelMatrix();
    renderer->renderMesh(*m_mesh, modelMatrix, "road");
}

void Road::createRoadMesh(const glm::vec2& size) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    float halfX = size.x * 0.5f;
    float halfY = size.y * 0.5f;
    float roadHeight = 0.01f; // Slightly above ground
    
    // Simple flat road surface
    vertices.push_back({{-halfX, -halfY, roadHeight}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ halfX, -halfY, roadHeight}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ halfX,  halfY, roadHeight}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-halfX,  halfY, roadHeight}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});
    
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(2);
    indices.push_back(3);
    indices.push_back(0);
    
    m_mesh = std::make_unique<Mesh>(vertices, indices);
}