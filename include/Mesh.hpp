#pragma once

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

class Mesh {
private:
    std::vector<Vertex> m_vertices;
    std::vector<GLuint> m_indices;
    
    GLuint m_VAO, m_VBO, m_EBO;
    bool m_setupDone;
    
    void setupMesh();

public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices);
    ~Mesh();
    
    void render() const;
    
    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<GLuint>& getIndices() const { return m_indices; }
};