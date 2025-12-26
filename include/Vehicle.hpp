#pragma once

#include "ControllableObject.hpp"
#include <memory>
#include <array>
#include <glm/glm.hpp>

class TileGrid;
class Pilot;

class Vehicle : public ControllableObject {
private:
    std::shared_ptr<Texture> m_texture;
    std::unique_ptr<Pilot> m_pilot;
    float m_speed;
    float m_maxSpeed;
    float m_maxSpeedRoad;
    float m_acceleration;
    float m_turnSpeed;
    glm::vec2 m_size;
    TileGrid* m_tileGrid;

public:
    Vehicle();
    ~Vehicle();
    
    bool initialize(const std::string& texturePath);
    bool initialize(std::shared_ptr<Texture> texture);
    void update(float deltaTime) override;
    void render(class Renderer* renderer) override;
    
    void moveForward(float deltaTime) override;
    void moveBackward(float deltaTime) override;
    void turnLeft(float deltaTime) override;
    void turnRight(float deltaTime) override;
    void setSpriteSize(const glm::vec2& size) { m_size = size; }
    const glm::vec2& getSpriteSize() const { return m_size; }
    void setTileGrid(class TileGrid* tileGrid) { m_tileGrid = tileGrid; }
    TileGrid* getTileGrid() const { return m_tileGrid; }
    
    // Pilot management
    void setPilot(std::unique_ptr<Pilot> pilot);
    Pilot* getPilot() const { return m_pilot.get(); }
    void clearPilot();
    bool hasPilot() const { return m_pilot != nullptr; }
    
    // Speed access for pilots
    float getSpeed() const { return m_speed; }
    void setSpeed(float speed) { m_speed = speed; }
    float getMaxSpeed() const { return m_maxSpeed; }
    float getAcceleration() const { return m_acceleration; }
    float getTurnSpeed() const { return m_turnSpeed; }

private:
    float getCurrentMaxSpeed() const;
    bool isOnRoad() const;
    std::array<glm::vec3, 8> getCollisionOffsets() const;
    bool canMoveTo(const glm::vec3& from, const glm::vec3& to) const;
};