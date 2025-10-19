#pragma once

#include <glm/glm.hpp>
#include <memory>
#include "Texture.hpp"
#include "Mesh.hpp"

class GameObject {
protected:
    glm::vec3 m_position;
    glm::vec3 m_rotation;
    glm::vec3 m_scale;
    bool m_active;

public:
    GameObject();
    virtual ~GameObject() = default;
    
    virtual void update(float deltaTime) = 0;
    virtual void render(class Renderer* renderer) = 0;
    
    // Transform methods
    void setPosition(const glm::vec3& position) { m_position = position; }
    void setRotation(const glm::vec3& rotation) { m_rotation = rotation; }
    void setScale(const glm::vec3& scale) { m_scale = scale; }
    
    const glm::vec3& getPosition() const { return m_position; }
    const glm::vec3& getRotation() const { return m_rotation; }
    const glm::vec3& getScale() const { return m_scale; }
    
    void setActive(bool active) { m_active = active; }
    bool isActive() const { return m_active; }
    
    glm::mat4 getModelMatrix() const;
};