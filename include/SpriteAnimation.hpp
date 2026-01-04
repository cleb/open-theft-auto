#pragma once

#include "Texture.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Represents a single frame in a sprite animation
struct AnimationFrame {
    int x;           // X coordinate of top-left corner in texture
    int y;           // Y coordinate of top-left corner in texture
};

// Represents a single animation (e.g., "walk", "idle")
struct Animation {
    std::string name;
    std::vector<AnimationFrame> frames;
    float frameDuration;    // Duration of each frame in seconds
    bool loop;              // Whether the animation loops
};

// Manages sprite sheet animations loaded from JSON definition files
class SpriteAnimation {
private:
    std::unique_ptr<Texture> m_texture;
    std::unordered_map<std::string, Animation> m_animations;
    
    int m_frameWidth;
    int m_frameHeight;
    int m_textureWidth;
    int m_textureHeight;
    
    // Current animation state
    std::string m_currentAnimation;
    int m_currentFrame;
    float m_frameTimer;
    bool m_isPlaying;
    bool m_animationFinished;

    bool parseAnimationFile(const std::string& jsonPath);

public:
    SpriteAnimation();
    ~SpriteAnimation() = default;
    
    // Load animation definition from JSON file
    bool loadFromFile(const std::string& jsonPath);
    
    // Animation control
    void play(const std::string& animationName);
    void stop();
    void pause();
    void resume();
    void reset();
    
    // Update animation state (call each frame)
    void update(float deltaTime);
    
    // Get current frame's UV coordinates for rendering
    // Returns: uvOffset (xy) and uvScale (zw) for the current frame
    glm::vec4 getCurrentFrameUV() const;
    
    // Accessors
    const Texture* getTexture() const { return m_texture.get(); }
    int getFrameWidth() const { return m_frameWidth; }
    int getFrameHeight() const { return m_frameHeight; }
    bool isPlaying() const { return m_isPlaying; }
    bool isAnimationFinished() const { return m_animationFinished; }
    const std::string& getCurrentAnimationName() const { return m_currentAnimation; }
    int getCurrentFrameIndex() const { return m_currentFrame; }
    
    // Check if an animation exists
    bool hasAnimation(const std::string& name) const;
    
    // Get list of available animations
    std::vector<std::string> getAnimationNames() const;
};
