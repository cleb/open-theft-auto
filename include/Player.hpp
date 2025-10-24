#pragma once

#include "GameObject.hpp"
#include <memory>

class TileGrid;

class Player : public GameObject {
private:
    std::unique_ptr<Texture> m_texture;
    float m_speed;
    float m_rotationSpeed;
    glm::vec2 m_size;
    TileGrid* m_tileGrid;

    void applyMovement(const glm::vec3& delta);

public:
    Player();
    ~Player() = default;
    
    bool initialize();
    void update(float deltaTime) override;
    void render(class Renderer* renderer) override;
    
    void moveForward(float deltaTime);
    void moveBackward(float deltaTime);
    void turnLeft(float deltaTime);
    void turnRight(float deltaTime);
    void setTileGrid(TileGrid* tileGrid) { m_tileGrid = tileGrid; }
    
    float getSpeed() const { return m_speed; }
    void setSpeed(float speed) { m_speed = speed; }
};