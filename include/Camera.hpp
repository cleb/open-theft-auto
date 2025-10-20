#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
private:
    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up;
    glm::mat4 m_viewMatrix;
    
    void updateViewMatrix();

public:
    Camera();
    ~Camera() = default;
    
    void setPosition(const glm::vec3& position);
    void setTarget(const glm::vec3& target);
    void lookAt(const glm::vec3& target);
    
    void move(const glm::vec3& offset);
    void followTarget(const glm::vec3& targetPos);
    
    void update(float deltaTime);
    
    const glm::vec3& getPosition() const { return m_position; }
    const glm::vec3& getTarget() const { return m_target; }
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
};