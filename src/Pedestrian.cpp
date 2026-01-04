#include "Pedestrian.hpp"
#include "Renderer.hpp"
#include "TileGrid.hpp"
#include "Heading.hpp"
#include <iostream>
#include <cmath>

Pedestrian::Pedestrian()
    : m_sharedAnimation(nullptr)
    , m_animationTime(0.0f)
    , m_tileGrid(nullptr)
    , m_speed(2.0f)  // Slower than player
    , m_size(0.8f, 0.8f)
    , m_walkingDirection(SidewalkDirection::NorthSouth) {
}

void Pedestrian::initialize(SpriteAnimation* sharedAnimation) {
    m_sharedAnimation = sharedAnimation;
    
    if (m_sharedAnimation) {
        // Calculate aspect ratio from frame dimensions for proper sprite size
        float aspectRatio = static_cast<float>(m_sharedAnimation->getFrameWidth()) / 
                           static_cast<float>(m_sharedAnimation->getFrameHeight());
        m_size = glm::vec2(0.8f * aspectRatio, 0.8f); // Slightly smaller than player
        
        // Randomize starting animation time so pedestrians aren't all in sync
        m_animationTime = static_cast<float>(rand()) / RAND_MAX * 0.8f;
    }
}

void Pedestrian::setWalkingDirection(SidewalkDirection dir) {
    m_walkingDirection = dir;
    
    // Set rotation based on direction
    // Since sidewalks are bidirectional, we randomly pick one direction
    // The rotation is set when the pedestrian spawns
    switch (dir) {
        case SidewalkDirection::NorthSouth:
            // Either facing North (90°) or South (270°)
            // Keep current rotation if already set
            break;
        case SidewalkDirection::EastWest:
            // Either facing East (0°) or West (180°)
            break;
        case SidewalkDirection::NorthEastSouthWest:
            // Either facing NE (45°) or SW (225°)
            break;
        case SidewalkDirection::NorthWestSouthEast:
            // Either facing NW (135°) or SE (315°)
            break;
        default:
            break;
    }
}

void Pedestrian::update(float deltaTime) {
    if (!m_active) return;
    
    // Update per-instance animation timer
    m_animationTime += deltaTime;
    
    // Update movement
    updateMovement(deltaTime);
}

void Pedestrian::updateMovement(float deltaTime) {
    if (!m_tileGrid) return;
    
    // Move in the direction the pedestrian is facing
    glm::vec2 forward = Heading::forwardFromHeadingDeg(m_rotation.z);
    glm::vec3 delta(forward.x * m_speed * deltaTime, forward.y * m_speed * deltaTime, 0.0f);
    
    glm::vec3 newPosition = m_position + delta;
    
    // Check if the new position is still on a sidewalk
    glm::ivec3 currentGrid = m_tileGrid->worldToGrid(m_position);
    glm::ivec3 newGrid = m_tileGrid->worldToGrid(newPosition);
    
    // If we're moving to a new tile, check if it's still a sidewalk
    if (newGrid != currentGrid) {
        if (!m_tileGrid->isSidewalkTile(newPosition)) {
            // Not a sidewalk - turn around
            m_rotation.z = Heading::wrapDegrees360(m_rotation.z + 180.0f);
            return;
        }
        // Note: We don't change direction based on the new tile's sidewalk direction
        // Pedestrians keep walking in their current direction until they hit a dead end
    }
    
    // Check if we can move (wall collision)
    if (m_tileGrid->canOccupy(m_position, newPosition)) {
        m_position = newPosition;
    } else {
        // Hit a wall - turn around
        m_rotation.z = Heading::wrapDegrees360(m_rotation.z + 180.0f);
    }
}

glm::vec4 Pedestrian::getCurrentFrameUV() const {
    if (!m_sharedAnimation) {
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
    
    // Calculate frame based on this pedestrian's animation time
    // Assuming 8 frames at 0.1s each = 0.8s cycle
    const float frameDuration = 0.1f;
    const int numFrames = 8;
    const float animCycle = frameDuration * numFrames;
    
    float wrappedTime = fmod(m_animationTime, animCycle);
    int frameIndex = static_cast<int>(wrappedTime / frameDuration) % numFrames;
    
    int frameWidth = m_sharedAnimation->getFrameWidth();
    int frameHeight = m_sharedAnimation->getFrameHeight();
    
    // Get texture dimensions from the animation
    const Texture* tex = m_sharedAnimation->getTexture();
    if (!tex) return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    
    float texWidth = static_cast<float>(tex->getWidth());
    float texHeight = static_cast<float>(tex->getHeight());
    
    // Calculate UV for this frame (frames are horizontal)
    float u = (frameIndex * frameWidth) / texWidth;
    float v = 0.0f;
    float uScale = frameWidth / texWidth;
    float vScale = frameHeight / texHeight;
    
    return glm::vec4(u, v, uScale, vScale);
}

void Pedestrian::render(Renderer* renderer) {
    if (!m_active || !renderer) return;
    
    if (m_sharedAnimation && m_sharedAnimation->getTexture()) {
        glm::vec4 uvOffsetScale = getCurrentFrameUV();
        renderer->renderAnimatedSprite(*m_sharedAnimation->getTexture(),
                                       glm::vec2(m_position.x, m_position.y),
                                       m_size, uvOffsetScale, m_rotation.z, glm::vec3(1.0f));
    }
}
