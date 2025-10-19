#include "GameObject.hpp"
#include <glm/gtc/matrix_transform.hpp>

GameObject::GameObject() 
    : m_position(0.0f)
    , m_rotation(0.0f)
    , m_scale(1.0f)
    , m_active(true) {
}

glm::mat4 GameObject::getModelMatrix() const {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);
    model = glm::rotate(model, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, m_scale);
    return model;
}