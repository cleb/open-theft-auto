#include "Camera.hpp"

Camera::Camera() 
    : m_position(0.0f, -8.0f, 12.0f)  // Position at angle for isometric-like view
    , m_target(0.0f, 0.0f, 0.0f)
    , m_up(0.0f, 0.0f, 1.0f) {  // Z-axis is up
    updateViewMatrix();
}

void Camera::setPosition(const glm::vec3& position) {
    m_position = position;
    updateViewMatrix();
}

void Camera::setTarget(const glm::vec3& target) {
    m_target = target;
    updateViewMatrix();
}

void Camera::lookAt(const glm::vec3& target) {
    setTarget(target);
}

void Camera::move(const glm::vec3& offset) {
    m_position += offset;
    m_target += offset;
    updateViewMatrix();
}

void Camera::followTarget(const glm::vec3& targetPos, float followSpeed, float deltaTime) {
    // Camera offset: angled view like original GTA
    glm::vec3 offset(0.0f, -8.0f, 12.0f);
    glm::vec3 desiredPosition = targetPos + offset;
    glm::vec3 smoothedPosition = glm::mix(m_position, desiredPosition, followSpeed * deltaTime);
    
    setPosition(smoothedPosition);
    setTarget(targetPos);
}

void Camera::update(float deltaTime) {
    // Camera can be updated here if needed (e.g., for animations)
    (void)deltaTime; // Suppress unused parameter warning
}

void Camera::updateViewMatrix() {
    m_viewMatrix = glm::lookAt(m_position, m_target, m_up);
}