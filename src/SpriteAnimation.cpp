#include "SpriteAnimation.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// Simple JSON parsing helpers (minimal implementation for our format)
namespace {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string extractStringValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t startQuote = json.find('"', colonPos);
    if (startQuote == std::string::npos) return "";
    
    size_t endQuote = json.find('"', startQuote + 1);
    if (endQuote == std::string::npos) return "";
    
    return json.substr(startQuote + 1, endQuote - startQuote - 1);
}

int extractIntValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return 0;
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return 0;
    
    size_t start = colonPos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;
    
    size_t end = start;
    while (end < json.size() && (std::isdigit(json[end]) || json[end] == '-')) end++;
    
    if (end == start) return 0;
    return std::stoi(json.substr(start, end - start));
}

float extractFloatValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return 0.0f;
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return 0.0f;
    
    size_t start = colonPos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;
    
    size_t end = start;
    while (end < json.size() && (std::isdigit(json[end]) || json[end] == '.' || json[end] == '-')) end++;
    
    if (end == start) return 0.0f;
    return std::stof(json.substr(start, end - start));
}

bool extractBoolValue(const std::string& json, const std::string& key, bool defaultValue = true) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return defaultValue;
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return defaultValue;
    
    size_t start = colonPos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;
    
    if (json.substr(start, 4) == "true") return true;
    if (json.substr(start, 5) == "false") return false;
    
    return defaultValue;
}

std::string extractObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t bracePos = json.find('{', keyPos);
    if (bracePos == std::string::npos) return "";
    
    int braceCount = 1;
    size_t endPos = bracePos + 1;
    while (endPos < json.size() && braceCount > 0) {
        if (json[endPos] == '{') braceCount++;
        else if (json[endPos] == '}') braceCount--;
        endPos++;
    }
    
    return json.substr(bracePos, endPos - bracePos);
}

std::string extractArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t bracketPos = json.find('[', keyPos);
    if (bracketPos == std::string::npos) return "";
    
    int bracketCount = 1;
    size_t endPos = bracketPos + 1;
    while (endPos < json.size() && bracketCount > 0) {
        if (json[endPos] == '[') bracketCount++;
        else if (json[endPos] == ']') bracketCount--;
        endPos++;
    }
    
    return json.substr(bracketPos, endPos - bracketPos);
}

std::vector<std::string> parseArrayOfObjects(const std::string& arrayJson) {
    std::vector<std::string> objects;
    
    size_t pos = 0;
    while (pos < arrayJson.size()) {
        size_t braceStart = arrayJson.find('{', pos);
        if (braceStart == std::string::npos) break;
        
        int braceCount = 1;
        size_t braceEnd = braceStart + 1;
        while (braceEnd < arrayJson.size() && braceCount > 0) {
            if (arrayJson[braceEnd] == '{') braceCount++;
            else if (arrayJson[braceEnd] == '}') braceCount--;
            braceEnd++;
        }
        
        objects.push_back(arrayJson.substr(braceStart, braceEnd - braceStart));
        pos = braceEnd;
    }
    
    return objects;
}

std::vector<std::pair<std::string, std::string>> parseObjectKeys(const std::string& objectJson) {
    std::vector<std::pair<std::string, std::string>> keyValues;
    
    size_t pos = 1; // Skip opening brace
    while (pos < objectJson.size()) {
        // Find key
        size_t keyStart = objectJson.find('"', pos);
        if (keyStart == std::string::npos) break;
        
        size_t keyEnd = objectJson.find('"', keyStart + 1);
        if (keyEnd == std::string::npos) break;
        
        std::string key = objectJson.substr(keyStart + 1, keyEnd - keyStart - 1);
        
        // Find value (could be object, array, string, number, bool)
        size_t colonPos = objectJson.find(':', keyEnd);
        if (colonPos == std::string::npos) break;
        
        size_t valueStart = colonPos + 1;
        while (valueStart < objectJson.size() && 
               (objectJson[valueStart] == ' ' || objectJson[valueStart] == '\t' || objectJson[valueStart] == '\n')) {
            valueStart++;
        }
        
        std::string value;
        if (objectJson[valueStart] == '{') {
            // Object value
            int braceCount = 1;
            size_t valueEnd = valueStart + 1;
            while (valueEnd < objectJson.size() && braceCount > 0) {
                if (objectJson[valueEnd] == '{') braceCount++;
                else if (objectJson[valueEnd] == '}') braceCount--;
                valueEnd++;
            }
            value = objectJson.substr(valueStart, valueEnd - valueStart);
            pos = valueEnd;
        } else {
            // Skip to next key or end
            pos = objectJson.find('"', valueStart);
            if (pos == std::string::npos) pos = objectJson.size();
        }
        
        if (!value.empty()) {
            keyValues.push_back({key, value});
        }
    }
    
    return keyValues;
}

} // namespace

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
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonContent = buffer.str();
    file.close();
    
    return parseAnimationFile(jsonContent);
}

bool SpriteAnimation::parseAnimationFile(const std::string& json) {
    // Extract texture filename
    std::string textureFile = extractStringValue(json, "texture");
    if (textureFile.empty()) {
        std::cerr << "No texture specified in animation file" << std::endl;
        return false;
    }
    
    // Load texture
    m_texture = std::make_unique<Texture>();
    std::string texturePath = "assets/textures/" + textureFile;
    if (!m_texture->loadFromFile(texturePath)) {
        std::cerr << "Failed to load animation texture: " << texturePath << std::endl;
        return false;
    }
    
    m_textureWidth = m_texture->getWidth();
    m_textureHeight = m_texture->getHeight();
    
    // Extract frame dimensions
    m_frameWidth = extractIntValue(json, "frameWidth");
    m_frameHeight = extractIntValue(json, "frameHeight");
    
    if (m_frameWidth <= 0 || m_frameHeight <= 0) {
        std::cerr << "Invalid frame dimensions in animation file" << std::endl;
        return false;
    }
    
    // Extract animations object
    std::string animationsObj = extractObject(json, "animations");
    if (animationsObj.empty()) {
        std::cerr << "No animations found in animation file" << std::endl;
        return false;
    }
    
    // Parse each animation
    auto animationEntries = parseObjectKeys(animationsObj);
    for (const auto& entry : animationEntries) {
        const std::string& animName = entry.first;
        const std::string& animObj = entry.second;
        
        Animation anim;
        anim.name = animName;
        anim.frameDuration = extractFloatValue(animObj, "frameDuration");
        anim.loop = extractBoolValue(animObj, "loop", true);
        
        if (anim.frameDuration <= 0.0f) {
            anim.frameDuration = 0.1f; // Default to 10 FPS
        }
        
        // Parse frames array
        std::string framesArray = extractArray(animObj, "frames");
        auto frameObjects = parseArrayOfObjects(framesArray);
        
        for (const auto& frameObj : frameObjects) {
            AnimationFrame frame;
            frame.x = extractIntValue(frameObj, "x");
            frame.y = extractIntValue(frameObj, "y");
            anim.frames.push_back(frame);
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
