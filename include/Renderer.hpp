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
    float m_fovRadians;
    float m_aspectRatio;
    
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
    
    // 3D rendering
    void renderMesh(const Mesh& mesh, const glm::mat4& modelMatrix, const std::string& shaderName, const glm::vec3& tint = glm::vec3(1.0f));
    
    // 2D sprite rendering
    void renderSprite(const Texture& texture, const glm::vec2& position, const glm::vec2& size,
                     float rotation = 0.0f, const glm::vec3& color = glm::vec3(1.0f));
    void renderSprite(const Texture& texture, const glm::vec3& position, const glm::vec2& size,
                     float rotation = 0.0f, const glm::vec3& color = glm::vec3(1.0f));

    // 2D animated sprite rendering (sprite sheet with UV coordinates)
    // uvOffsetScale: xy = UV offset (top-left), zw = UV scale (width/height in UV space)
    void renderAnimatedSprite(const Texture& texture, const glm::vec2& position, const glm::vec2& size,
                              const glm::vec4& uvOffsetScale, float rotation = 0.0f,
                              const glm::vec3& color = glm::vec3(1.0f));
    void renderAnimatedSprite(const Texture& texture, const glm::vec3& position, const glm::vec2& size,
                              const glm::vec4& uvOffsetScale, float rotation = 0.0f,
                              const glm::vec3& color = glm::vec3(1.0f));

    // 2D sprite rendering with damage overlay (GPU-accelerated)
    void renderDamagedSprite(const Texture& texture, const Texture* deltaTexture,
                             const glm::vec2& position, const glm::vec2& size,
                             float rotation, const glm::vec3& color,
                             bool damageFrontLeft, bool damageFrontRight,
                             bool damageRearLeft, bool damageRearRight);
    void renderDamagedSprite(const Texture& texture, const Texture* deltaTexture,
                             const glm::vec3& position, const glm::vec2& size,
                             float rotation, const glm::vec3& color,
                             bool damageFrontLeft, bool damageFrontRight,
                             bool damageRearLeft, bool damageRearRight);

    // 2D sprite rendering with fire shader
    void renderFireSprite(const Texture& texture, const glm::vec2& position, const glm::vec2& size,
                          float rotation, const glm::vec3& color, float fireIntensity, float timeSeconds);
    void renderFireSprite(const Texture& texture, const glm::vec3& position, const glm::vec2& size,
                          float rotation, const glm::vec3& color, float fireIntensity, float timeSeconds);

    // 2D sprite rendering with fire shader and damage overlay
    void renderFireDamagedSprite(const Texture& texture, const Texture* deltaTexture,
                                 const glm::vec2& position, const glm::vec2& size,
                                 float rotation, const glm::vec3& color,
                                 bool damageFrontLeft, bool damageFrontRight,
                                 bool damageRearLeft, bool damageRearRight,
                                 float fireIntensity, float timeSeconds);
    void renderFireDamagedSprite(const Texture& texture, const Texture* deltaTexture,
                                 const glm::vec3& position, const glm::vec2& size,
                                 float rotation, const glm::vec3& color,
                                 bool damageFrontLeft, bool damageFrontRight,
                                 bool damageRearLeft, bool damageRearRight,
                                 float fireIntensity, float timeSeconds);

    // 2D sprite rendering with explosion shader
    void renderExplosionSprite(const Texture& texture, const glm::vec2& position, const glm::vec2& size,
                               float rotation, const glm::vec3& color, float explosionProgress);
    void renderExplosionSprite(const Texture& texture, const glm::vec3& position, const glm::vec2& size,
                               float rotation, const glm::vec3& color, float explosionProgress);
    
    // Shader management
    bool loadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);
    Shader* getShader(const std::string& name);
    
    // Camera
    Camera* getCamera() const { return m_camera.get(); }
    float getFovRadians() const { return m_fovRadians; }
    float getAspectRatio() const { return m_aspectRatio; }

    void onWindowResize(int width, int height);
    bool saveScreenshot(const std::string& path, int width, int height) const;
    
    // Convert screen coordinates to world position at a given Z plane
    // Uses inverse view-projection matrix to unproject screen coordinates
    bool screenToWorldPosition(double mouseX, double mouseY, int windowWidth, int windowHeight,
                               float planeZ, glm::vec3& outWorldPos) const;
    
    // Debug rendering
    void renderDebugMarker(const glm::vec2& position, const glm::vec2& size, const glm::vec3& color);
};
