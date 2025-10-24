#pragma once

#include "GameObject.hpp"
#include <memory>
#include <array>
#include <glm/glm.hpp>

class TileGrid;

class Vehicle : public GameObject {
private:
    std::shared_ptr<Texture> m_texture;
    float m_speed;
    float m_maxSpeed;
    float m_maxSpeedRoad;
    float m_acceleration;
    float m_turnSpeed;
    glm::vec2 m_size;
    bool m_playerControlled;
    TileGrid* m_tileGrid;

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
    void setTileGrid(class TileGrid* tileGrid) { m_tileGrid = tileGrid; }
    
    void setPlayerControlled(bool controlled) { m_playerControlled = controlled; }
    bool isPlayerControlled() const { return m_playerControlled; }

private:
    float getCurrentMaxSpeed() const;
    bool isOnRoad() const;
    std::array<glm::vec3, 8> getCollisionOffsets() const;
    bool canMoveTo(const glm::vec3& from, const glm::vec3& to) const;
};