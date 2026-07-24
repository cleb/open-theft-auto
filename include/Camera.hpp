#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
private:
    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up;
    glm::mat4 m_viewMatrix;
    float m_followHeight;
    float m_desiredFollowHeight;

    void updateViewMatrix();

public:
    static constexpr float DefaultHeight = 27.7f;
    // Speed at which the camera starts pulling back, and the speed at which it
    // reaches MaxFollowHeight.
    static constexpr float ZoomStartSpeed = 6.0f;
    static constexpr float ZoomFullSpeed = 36.0f;
    static constexpr float MaxFollowHeight = DefaultHeight * 1.35f;
    static constexpr float ZoomSmoothing = 2.5f;

    Camera();
    ~Camera() = default;
    
    void setPosition(const glm::vec3& position);
    void setTarget(const glm::vec3& target);
    void lookAt(const glm::vec3& target);
    
    void move(const glm::vec3& offset);
    void followTarget(const glm::vec3& targetPos, float speed = 0.0f);
    
    void update(float deltaTime);
    
    const glm::vec3& getPosition() const { return m_position; }
    const glm::vec3& getTarget() const { return m_target; }
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
    float getFollowHeight() const { return m_followHeight; }
};
