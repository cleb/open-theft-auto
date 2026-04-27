#include "SpriteAnimation.hpp"
#include "TextureManager.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

SpriteAnimation::SpriteAnimation()
    : m_frameWidth(0)
    , m_frameHeight(0)
    , m_textureWidth(0)
    , m_textureHeight(0)
    , m_currentFrame(0)
    , m_frameTimer(0.0f)
    , m_isPlaying(false)
    , m_animationFinished(false) {
}

bool SpriteAnimation::loadFromFile(const std::string& jsonPath) {
    // Read JSON file
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open animation file: " << jsonPath << std::endl;
        return false;
    }
    
    try {
        nlohmann::json json;
        file >> json;
        file.close();
        return parseAnimationFile(json);
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parse error in " << jsonPath << ": " << e.what() << std::endl;
        return false;
    }
}

bool SpriteAnimation::parseAnimationFile(const nlohmann::json& json) {
    // Extract texture filename
    if (!json.contains("texture") || !json["texture"].is_string()) {
        std::cerr << "No texture specified in animation file" << std::endl;
        return false;
    }
    std::string textureFile = json["texture"].get<std::string>();
    
    // Load texture
    std::string texturePath = "assets/textures/" + textureFile;
    m_texture = TextureManager::instance().getTextureFromPath(texturePath);
    if (!m_texture) {
        std::cerr << "Failed to load animation texture: " << texturePath << std::endl;
        return false;
    }
    
    m_textureWidth = m_texture->getWidth();
    m_textureHeight = m_texture->getHeight();
    
    // Extract frame dimensions
    m_frameWidth = json.value("frameWidth", 0);
    m_frameHeight = json.value("frameHeight", 0);
    
    if (m_frameWidth <= 0 || m_frameHeight <= 0) {
        std::cerr << "Invalid frame dimensions in animation file" << std::endl;
        return false;
    }
    
    // Extract animations object
    if (!json.contains("animations") || !json["animations"].is_object()) {
        std::cerr << "No animations found in animation file" << std::endl;
        return false;
    }
    
    // Parse each animation
    for (auto& [animName, animObj] : json["animations"].items()) {
        Animation anim;
        anim.name = animName;
        anim.frameDuration = animObj.value("frameDuration", 0.1f);
        anim.loop = animObj.value("loop", true);
        
        if (anim.frameDuration <= 0.0f) {
            anim.frameDuration = 0.1f; // Default to 10 FPS
        }
        
        // Parse frames array
        if (animObj.contains("frames") && animObj["frames"].is_array()) {
            for (const auto& frameObj : animObj["frames"]) {
                AnimationFrame frame;
                frame.x = frameObj.value("x", 0);
                frame.y = frameObj.value("y", 0);
                anim.frames.push_back(frame);
            }
        }
        
        if (!anim.frames.empty()) {
            m_animations[animName] = anim;
            std::cout << "Loaded animation '" << animName << "' with " << anim.frames.size() << " frames" << std::endl;
        }
    }
    
    return !m_animations.empty();
}

void SpriteAnimation::play(const std::string& animationName) {
    if (m_animations.find(animationName) == m_animations.end()) {
        std::cerr << "Animation not found: " << animationName << std::endl;
        return;
    }
    
    // Only reset if changing animation
    if (m_currentAnimation != animationName) {
        m_currentAnimation = animationName;
        m_currentFrame = 0;
        m_frameTimer = 0.0f;
        m_animationFinished = false;
    }
    
    m_isPlaying = true;
}

void SpriteAnimation::stop() {
    m_isPlaying = false;
    m_currentFrame = 0;
    m_frameTimer = 0.0f;
}

void SpriteAnimation::pause() {
    m_isPlaying = false;
}

void SpriteAnimation::resume() {
    m_isPlaying = true;
}

void SpriteAnimation::reset() {
    m_currentFrame = 0;
    m_frameTimer = 0.0f;
    m_animationFinished = false;
}

void SpriteAnimation::update(float deltaTime) {
    if (!m_isPlaying || m_currentAnimation.empty()) {
        return;
    }
    
    auto it = m_animations.find(m_currentAnimation);
    if (it == m_animations.end() || it->second.frames.empty()) {
        return;
    }
    
    const Animation& anim = it->second;
    
    m_frameTimer += deltaTime;
    
    while (m_frameTimer >= anim.frameDuration) {
        m_frameTimer -= anim.frameDuration;
        m_currentFrame++;
        
        if (m_currentFrame >= static_cast<int>(anim.frames.size())) {
            if (anim.loop) {
                m_currentFrame = 0;
            } else {
                m_currentFrame = static_cast<int>(anim.frames.size()) - 1;
                m_isPlaying = false;
                m_animationFinished = true;
                break;
            }
        }
    }
}

glm::vec4 SpriteAnimation::getCurrentFrameUV() const {
    if (m_currentAnimation.empty() || m_textureWidth == 0 || m_textureHeight == 0) {
        // Return full texture UV if no animation
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
    
    auto it = m_animations.find(m_currentAnimation);
    if (it == m_animations.end() || it->second.frames.empty()) {
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
    
    const Animation& anim = it->second;
    int frameIdx = std::min(m_currentFrame, static_cast<int>(anim.frames.size()) - 1);
    const AnimationFrame& frame = anim.frames[frameIdx];
    
    // Calculate UV coordinates
    // UV offset is the top-left corner of the frame in normalized coordinates
    // UV scale is the size of the frame in normalized coordinates
    float uvOffsetX = static_cast<float>(frame.x) / static_cast<float>(m_textureWidth);
    float uvOffsetY = static_cast<float>(frame.y) / static_cast<float>(m_textureHeight);
    float uvScaleX = static_cast<float>(m_frameWidth) / static_cast<float>(m_textureWidth);
    float uvScaleY = static_cast<float>(m_frameHeight) / static_cast<float>(m_textureHeight);
    
    return glm::vec4(uvOffsetX, uvOffsetY, uvScaleX, uvScaleY);
}

bool SpriteAnimation::hasAnimation(const std::string& name) const {
    return m_animations.find(name) != m_animations.end();
}

std::vector<std::string> SpriteAnimation::getAnimationNames() const {
    std::vector<std::string> names;
    names.reserve(m_animations.size());
    for (const auto& pair : m_animations) {
        names.push_back(pair.first);
    }
    return names;
}
