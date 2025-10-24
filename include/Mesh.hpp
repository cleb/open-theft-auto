#pragma once

#include <vector>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

class Texture;

class Mesh {
private:
    std::vector<Vertex> m_vertices;
    std::vector<GLuint> m_indices;
    
    GLuint m_VAO, m_VBO, m_EBO;
    bool m_setupDone;
    std::shared_ptr<Texture> m_texture;
    
    void setupMesh();

public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices);
    ~Mesh();
    
    void render() const;
    void setTexture(const std::shared_ptr<Texture>& texture);
    const std::shared_ptr<Texture>& getTexture() const { return m_texture; }
    bool hasTexture() const { return static_cast<bool>(m_texture); }
    
    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<GLuint>& getIndices() const { return m_indices; }
};