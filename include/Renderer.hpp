#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <unordered_map>
#include <string>
#include "Shader.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "Texture.hpp"

class Renderer {
private:
    std::unordered_map<std::string, std::unique_ptr<Shader>> m_shaders;
    std::unique_ptr<Camera> m_camera;
    
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewMatrix;
    
    // Sprite rendering
    GLuint m_spriteVAO;
    GLuint m_spriteVBO;
    void initializeSpriteData();
    
public:
    Renderer();
    ~Renderer();
    
    bool initialize(int windowWidth, int windowHeight);
    void shutdown();
    
    void beginFrame();
    void endFrame();
    
    void setProjectionMatrix(const glm::mat4& projection) { m_projectionMatrix = projection; }
    void setViewMatrix(const glm::mat4& view) { m_viewMatrix = view; }
    const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }

    // 3D rendering
    void renderMesh(const Mesh& mesh, const glm::mat4& modelMatrix, const std::string& shaderName, const glm::vec3& tint = glm::vec3(1.0f));
    
    // 2D sprite rendering
    void renderSprite(const Texture& texture, const glm::vec2& position, const glm::vec2& size, 
                     float rotation = 0.0f, const glm::vec3& color = glm::vec3(1.0f));
    
    // Shader management
    bool loadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);
    Shader* getShader(const std::string& name);
    glm::vec3 getCameraPosition() const;

    // Camera
    Camera* getCamera() const { return m_camera.get(); }
    
    void onWindowResize(int width, int height);
};