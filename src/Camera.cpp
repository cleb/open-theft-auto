#include "Camera.hpp"

#include <algorithm>
#include <cmath>

Camera::Camera() 
    : m_position(0.0f, 0.0f, DefaultHeight)
    , m_target(0.0f, 0.0f, 0.0f)
    , m_up(0.0f, 1.0f, 0.0f)   // Y-axis is up
    , m_followHeight(DefaultHeight)
    , m_desiredFollowHeight(DefaultHeight) {
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

void Camera::followTarget(const glm::vec3& targetPos, float speed) {
    const float absSpeed = std::abs(speed);
    float zoomFactor = (absSpeed - ZoomStartSpeed) / (ZoomFullSpeed - ZoomStartSpeed);
    zoomFactor = std::clamp(zoomFactor, 0.0f, 1.0f);
    m_desiredFollowHeight = DefaultHeight + (MaxFollowHeight - DefaultHeight) * zoomFactor;

    glm::vec3 offset(0.0f, 0.0f, m_followHeight);
    glm::vec3 desiredPosition = targetPos + offset;

    setPosition(desiredPosition);
    setTarget(targetPos);
}

void Camera::update(float deltaTime) {
    // Smoothly ease the follow height toward the speed-driven zoom level.
    const float blend = 1.0f - std::exp(-ZoomSmoothing * std::max(deltaTime, 0.0f));
    m_followHeight += (m_desiredFollowHeight - m_followHeight) * blend;
}

void Camera::updateViewMatrix() {
    m_viewMatrix = glm::lookAt(m_position, m_target, m_up);
}
