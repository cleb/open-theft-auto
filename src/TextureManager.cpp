#include "TextureManager.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace {
bool isSupportedTextureExtension(std::string extension) {
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga";
}
}

TextureManager& TextureManager::instance() {
    static TextureManager instance;
    return instance;
}

void TextureManager::registerAlias(const std::string& alias, const std::string& path) {
    if (alias.empty() || path.empty()) {
        return;
    }
    m_aliases[alias] = path;
}

std::shared_ptr<Texture> TextureManager::getTexture(const std::string& identifier) {
    const std::string resolvedPath = resolvePath(identifier);
    return getTextureFromPath(resolvedPath);
}

std::shared_ptr<Texture> TextureManager::getTextureFromPath(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    // Check cache first
    auto cacheIt = m_cache.find(path);
    if (cacheIt != m_cache.end()) {
        return cacheIt->second;
    }

    // Load the texture
    auto texture = std::make_shared<Texture>();
    if (!texture->loadFromFile(path)) {
        std::cerr << "TextureManager: Failed to load texture: " << path << std::endl;
        return nullptr;
    }

    // Cache and return
    m_cache[path] = texture;
    return texture;
}

size_t TextureManager::preloadTexturesFromDirectory(const std::string& directoryPath) {
    namespace fs = std::filesystem;

    if (directoryPath.empty() || !fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
        std::cerr << "TextureManager: Cannot preload textures from missing directory: " << directoryPath << std::endl;
        return 0;
    }

    std::vector<std::string> texturePaths;
    for (const auto& entry : fs::recursive_directory_iterator(directoryPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path& path = entry.path();
        if (isSupportedTextureExtension(path.extension().string())) {
            texturePaths.push_back(path.generic_string());
        }
    }

    std::sort(texturePaths.begin(), texturePaths.end());

    size_t loadedCount = 0;
    for (const std::string& path : texturePaths) {
        if (getTextureFromPath(path)) {
            ++loadedCount;
        }
    }

    std::cout << "TextureManager: Preloaded " << loadedCount << " of " << texturePaths.size()
              << " textures from " << directoryPath << std::endl;
    return loadedCount;
}

bool TextureManager::hasAlias(const std::string& alias) const {
    return m_aliases.find(alias) != m_aliases.end();
}

std::string TextureManager::resolvePath(const std::string& identifier) const {
    auto aliasIt = m_aliases.find(identifier);
    if (aliasIt != m_aliases.end()) {
        return aliasIt->second;
    }
    // Return as-is if not an alias (assume it's a direct path)
    return identifier;
}

void TextureManager::clearCache() {
    m_cache.clear();
}

void TextureManager::clearTexture(const std::string& path) {
    m_cache.erase(path);
}

// --- Vehicle texture support ---

void TextureManager::registerVehicleType(const std::string& vehicleType,
                                         const std::string& basePath,
                                         const std::string& deltaPath) {
    if (vehicleType.empty() || basePath.empty()) {
        std::cerr << "TextureManager: Cannot register vehicle type with empty type or base path" << std::endl;
        return;
    }

    m_vehicleTextures[vehicleType] = VehicleTexturePaths{basePath, deltaPath};
}

std::shared_ptr<Texture> TextureManager::getVehicleTexture(const std::string& vehicleType) {
    auto it = m_vehicleTextures.find(vehicleType);
    if (it == m_vehicleTextures.end()) {
        std::cerr << "TextureManager: Unknown vehicle type: " << vehicleType << std::endl;
        return nullptr;
    }

    return getTextureFromPath(it->second.basePath);
}

std::shared_ptr<Texture> TextureManager::getVehicleDeltaTexture(const std::string& vehicleType) {
    auto it = m_vehicleTextures.find(vehicleType);
    if (it == m_vehicleTextures.end()) {
        std::cerr << "TextureManager: Unknown vehicle type: " << vehicleType << std::endl;
        return nullptr;
    }

    if (it->second.deltaPath.empty()) {
        return nullptr;  // No delta texture registered for this type
    }

    return getTextureFromPath(it->second.deltaPath);
}

bool TextureManager::hasVehicleType(const std::string& vehicleType) const {
    return m_vehicleTextures.find(vehicleType) != m_vehicleTextures.end();
}

std::vector<std::string> TextureManager::getVehicleTypes() const {
    std::vector<std::string> types;
    types.reserve(m_vehicleTextures.size());
    for (const auto& pair : m_vehicleTextures) {
        types.push_back(pair.first);
    }
    return types;
}
