#pragma once

#include "GameObject.hpp"
#include <memory>
#include <glm/glm.hpp>

class Vehicle : public GameObject {
private:
    std::shared_ptr<Texture> m_texture;
    float m_speed;
    float m_maxSpeed;
    float m_acceleration;
    float m_turnSpeed;
    glm::vec2 m_size;
    bool m_playerControlled;

public:
    Vehicle();
    ~Vehicle() = default;
    
    bool initialize(const std::string& texturePath);
    void update(float deltaTime) override;
    void render(class Renderer* renderer) override;
    
    void accelerate(float deltaTime);
    void brake(float deltaTime);
    void turnLeft(float deltaTime);
    void turnRight(float deltaTime);
    void setSpriteSize(const glm::vec2& size) { m_size = size; }
    const glm::vec2& getSpriteSize() const { return m_size; }
    
    void setPlayerControlled(bool controlled) { m_playerControlled = controlled; }
    bool isPlayerControlled() const { return m_playerControlled; }
};