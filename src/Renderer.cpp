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
    m_camera->setPosition(glm::vec3(0.0f, 0.0f, 12.0f));
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

    // Convert screen coordinates to Normalized Device Coordinates (NDC)
    // Screen: origin at top-left, Y down. NDC: origin at center, both X and Y in [-1, 1]
    const float ndcX = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(windowWidth) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(windowHeight);

    // For orthographic projection, we can directly map NDC to world space
    // by inverting the view-projection transformation
    const glm::mat4 viewProjection = m_projectionMatrix * m_viewMatrix;
    const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);
    
    // Create a point in NDC space at the target Z plane
    // We need to find the correct NDC Z value that corresponds to our world Z
    // For now, we'll use a point and solve for where it intersects planeZ
    
    // Transform NDC point to world space (at near plane, z=-1 in NDC)
    glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    nearPoint /= nearPoint.w; // Perspective divide
    
    // Transform NDC point to world space (at far plane, z=1 in NDC)
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    farPoint /= farPoint.w; // Perspective divide
    
    // Now we have a ray from nearPoint to farPoint in world space
    // Find where this ray intersects the plane at Z = planeZ
    const float nearZ = nearPoint.z;
    const float farZ = farPoint.z;
    
    // Check if the plane is between near and far points
    if ((planeZ < nearZ && planeZ < farZ) || (planeZ > nearZ && planeZ > farZ)) {
        return false; // Plane is outside the ray
    }
    
    // Linear interpolation to find the intersection point
    // t = (planeZ - nearZ) / (farZ - nearZ)
    const float t = (planeZ - nearZ) / (farZ - nearZ);
    
    outWorldPos = glm::vec3(
        nearPoint.x + t * (farPoint.x - nearPoint.x),
        nearPoint.y + t * (farPoint.y - nearPoint.y),
        planeZ
    );
    
    return true;
}