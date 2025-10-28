#include "Renderer.hpp"
#include <iostream>
#include <GL/glew.h>

Renderer::Renderer() : m_spriteVAO(0), m_spriteVBO(0) {
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(int windowWidth, int windowHeight) {
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set viewport
    glViewport(0, 0, windowWidth, windowHeight);
    
    // Initialize projection matrix (orthographic for 2.5D)
    float aspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    m_projectionMatrix = glm::perspective(1.57f, aspectRatio, 0.1f, 64.0f);
    
    // Initialize camera
    m_camera = std::make_unique<Camera>();
    m_camera->setPosition(glm::vec3(0.0f, -8.0f, 12.0f));
    m_camera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    
    // Initialize sprite rendering data
    initializeSpriteData();
    
    // Load default shaders
    if (!loadShader("sprite", "assets/shaders/sprite.vert", "assets/shaders/sprite.frag")) {
        std::cerr << "Failed to load sprite shader" << std::endl;
    }
    
    if (!loadShader("model", "assets/shaders/model.vert", "assets/shaders/model.frag")) {
        std::cerr << "Failed to load model shader" << std::endl;
    }
    
    if (!loadShader("player", "assets/shaders/model.vert", "assets/shaders/model.frag")) {
        std::cerr << "Failed to load player shader" << std::endl;
    }
    
    if (!loadShader("vehicle", "assets/shaders/model.vert", "assets/shaders/model.frag")) {
        std::cerr << "Failed to load vehicle shader" << std::endl;
    }
    
    if (!loadShader("road", "assets/shaders/model.vert", "assets/shaders/model.frag")) {
        std::cerr << "Failed to load road shader" << std::endl;
    }
    
    return true;
}

void Renderer::shutdown() {
    if (m_spriteVAO) {
        glDeleteVertexArrays(1, &m_spriteVAO);
        m_spriteVAO = 0;
    }
    if (m_spriteVBO) {
        glDeleteBuffers(1, &m_spriteVBO);
        m_spriteVBO = 0;
    }
    
    m_shaders.clear();
    m_camera.reset();
}

void Renderer::beginFrame() {
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Update view matrix from camera
    if (m_camera) {
        m_viewMatrix = m_camera->getViewMatrix();
    }
}

void Renderer::endFrame() {
    // Nothing specific needed here for now
}

void Renderer::initializeSpriteData() {
    // Sprite quad vertices (for 2D sprites in XY plane)
    // Centered at origin, will be transformed by model matrix
    float vertices[] = {
        // Positions        // Texture coords
        -0.5f, -0.5f,       0.0f, 0.0f,
         0.5f,  0.5f,       1.0f, 1.0f,
        -0.5f,  0.5f,       0.0f, 1.0f,
        
        -0.5f, -0.5f,       0.0f, 0.0f,
         0.5f, -0.5f,       1.0f, 0.0f,
         0.5f,  0.5f,       1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &m_spriteVAO);
    glGenBuffers(1, &m_spriteVBO);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindVertexArray(m_spriteVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::renderMesh(const Mesh& mesh, const glm::mat4& modelMatrix, const std::string& shaderName, const glm::vec3& tint) {
    Shader* shader = getShader(shaderName);
    if (!shader) return;
    
    shader->use();
    shader->setMat4("model", modelMatrix);
    shader->setMat4("view", m_viewMatrix);
    shader->setMat4("projection", m_projectionMatrix);
    
    // Set lighting uniforms
    shader->setVec3("lightPos", glm::vec3(10.0f, 10.0f, 10.0f));
    shader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));

    const auto& texture = mesh.getTexture();
    const bool hasTexture = static_cast<bool>(texture);
    shader->setInt("useTexture", hasTexture ? 1 : 0);
    shader->setVec3("objectColor", tint);
    if (hasTexture) {
        shader->setInt("texture_diffuse1", 0);
        texture->bind(0);
    }
    
    mesh.render();
    if (hasTexture) {
        texture->unbind();
    }
    
    shader->unuse();
}

void Renderer::renderSprite(const Texture& texture, const glm::vec2& position, const glm::vec2& size, 
                           float rotation, const glm::vec3& color) {
    Shader* shader = getShader("sprite");
    if (!shader) return;
    
    shader->use();
    
    // Create model matrix for sprite (positioned in XY plane, facing up for top-down view)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position.x, position.y, 0.1f)); // Slightly above ground
    model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));
    
    shader->setMat4("model", model);
    shader->setMat4("view", m_viewMatrix);
    shader->setMat4("projection", m_projectionMatrix);
    shader->setVec3("spriteColor", color);
    shader->setInt("sprite", 0);
    
    texture.bind(0);
    
    glBindVertexArray(m_spriteVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    shader->unuse();
}

bool Renderer::loadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath) {
    auto shader = std::make_unique<Shader>();
    if (shader->loadFromFiles(vertexPath, fragmentPath)) {
        m_shaders[name] = std::move(shader);
        return true;
    }
    return false;
}

Shader* Renderer::getShader(const std::string& name) {
    auto it = m_shaders.find(name);
    return (it != m_shaders.end()) ? it->second.get() : nullptr;
}

void Renderer::onWindowResize(int width, int height) {
    glViewport(0, 0, width, height);

    // Update projection matrix
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float viewSize = 20.0f;
    m_projectionMatrix = glm::ortho(-viewSize * aspectRatio, viewSize * aspectRatio,
                                   -viewSize, viewSize, 0.1f, 100.0f);
}

bool Renderer::screenToWorldPosition(double mouseX, double mouseY, int windowWidth, int windowHeight,
                                      float planeZ, glm::vec3& outWorldPos) const {
    if (!m_camera || windowWidth <= 0 || windowHeight <= 0) {
        return false;
    }

    // Step 1: Convert screen coordinates to Normalized Device Coordinates (NDC)
    // Screen space: origin at top-left, Y increases downward
    // NDC space: origin at center, X and Y range from -1 to 1
    const float ndcX = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(windowWidth) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(windowHeight);
    
    // Step 2: Create a ray in clip space (NDC with depth and w component)
    // Z = -1 points into the screen (near plane in NDC)
    glm::vec4 rayClipSpace(ndcX, ndcY, -1.0f, 1.0f);
    
    // Step 3: Transform ray from clip space to view space (camera space)
    // Inverse projection removes the perspective/orthographic transformation
    glm::vec4 rayViewSpace = glm::inverse(m_projectionMatrix) * rayClipSpace;
    // For direction vector, we want w=0 (vector, not point)
    rayViewSpace = glm::vec4(rayViewSpace.x, rayViewSpace.y, -1.0f, 0.0f);
    
    // Step 4: Transform ray from view space to world space
    glm::vec3 rayWorldDirection = glm::vec3(glm::inverse(m_viewMatrix) * rayViewSpace);
    rayWorldDirection = glm::normalize(rayWorldDirection);
    
    // Step 5: Get ray origin (camera position in world space)
    const glm::vec3 rayOrigin = m_camera->getPosition();
    
    // Step 6: Perform ray-plane intersection
    // Plane equation: all points where Z = planeZ
    // Plane normal: (0, 0, 1) pointing up along Z-axis
    const glm::vec3 planeNormal(0.0f, 0.0f, 1.0f);
    const glm::vec3 planePoint(0.0f, 0.0f, planeZ);
    
    // Calculate denominator: how aligned is the ray with the plane normal?
    const float denominator = glm::dot(rayWorldDirection, planeNormal);
    
    // Check if ray is parallel to plane (denominator near zero)
    if (std::abs(denominator) < 1e-6f) {
        return false; // No intersection or ray lies in plane
    }
    
    // Calculate distance along ray to intersection point
    // Formula: t = dot(planePoint - rayOrigin, planeNormal) / dot(rayDirection, planeNormal)
    const glm::vec3 originToPlane = planePoint - rayOrigin;
    const float t = glm::dot(originToPlane, planeNormal) / denominator;
    
    // Check if intersection is behind the camera (negative t)
    if (t < 0.0f) {
        return false;
    }
    
    // Calculate final world position of intersection
    outWorldPos = rayOrigin + rayWorldDirection * t;
    return true;
}