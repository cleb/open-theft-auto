#pragma once

#include "GameObject.hpp"
#include "SpriteAnimation.hpp"
#include "Tile.hpp"
#include <memory>
#include <glm/glm.hpp>

class TileGrid;
class Renderer;

class Pedestrian : public GameObject {
public:
    Pedestrian();
    ~Pedestrian() = default;

    // Initialize with a shared animation (texture shared across all pedestrians)
    void initialize(SpriteAnimation* sharedAnimation);
    void update(float deltaTime) override;
    void render(Renderer* renderer) override;

    void setTileGrid(TileGrid* tileGrid) { m_tileGrid = tileGrid; }
    void setSpeed(float speed) { m_speed = speed; }
    float getSpeed() const { return m_speed; }
    
    // Set the walking direction based on sidewalk direction
    void setWalkingDirection(SidewalkDirection dir);
    SidewalkDirection getWalkingDirection() const { return m_walkingDirection; }
    
    glm::vec2 getSize() const { return m_size; }

private:
    SpriteAnimation* m_sharedAnimation;  // Shared animation (not owned)
    float m_animationTime;               // Per-instance animation timer
    
    TileGrid* m_tileGrid;
    float m_speed;
    glm::vec2 m_size;
    SidewalkDirection m_walkingDirection;
    
    void updateMovement(float deltaTime);
    glm::vec4 getCurrentFrameUV() const;
};
