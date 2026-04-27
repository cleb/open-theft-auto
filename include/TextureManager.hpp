#pragma once

#include "Texture.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * TextureManager - Centralized texture loading and caching
 * 
 * This singleton manages all textures in the game, ensuring each texture
 * is loaded only once and shared across all users.
 * 
 * Usage:
 *   auto& tm = TextureManager::instance();
 *   tm.registerAlias("grass", "assets/textures/grass.png");
 *   auto tex = tm.getTexture("grass");  // or tm.getTexture("assets/textures/grass.png");
 * 
 * Future support for vehicle textures:
 *   Each vehicle type will have a base texture and optional delta textures.
 *   Use getVehicleTexture(vehicleType) and getVehicleDeltaTexture(vehicleType)
 *   to retrieve them. Vehicle textures are expected to follow naming conventions:
 *     - Base: "assets/textures/vehicles/<type>.png"
 *     - Delta: "assets/textures/vehicles/<type>_delta.png"
 */
class TextureManager {
public:
    // Singleton access
    static TextureManager& instance();

    // Prevent copying
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Register an alias for a texture path (e.g., "grass" -> "assets/textures/grass.png")
    void registerAlias(const std::string& alias, const std::string& path);

    // Get a texture by alias or path (loads if not cached)
    std::shared_ptr<Texture> getTexture(const std::string& identifier);

    // Get a texture, loading from a specific path (bypasses alias resolution)
    std::shared_ptr<Texture> getTextureFromPath(const std::string& path);

    // Recursively preload all supported image textures in a directory.
    size_t preloadTexturesFromDirectory(const std::string& directoryPath);

    // Check if an alias exists
    bool hasAlias(const std::string& alias) const;

    // Get the resolved path for an identifier (alias or direct path)
    std::string resolvePath(const std::string& identifier) const;

    // Get all registered aliases (useful for debugging/editor)
    const std::unordered_map<std::string, std::string>& getAliases() const { return m_aliases; }

    // Clear all cached textures (useful for reloading)
    void clearCache();

    // Clear a specific texture from cache
    void clearTexture(const std::string& path);

    // --- Vehicle texture support (prepared for future use) ---
    
    // Register a vehicle type with its textures
    // In the future, call this with vehicleType="sedan", basePath="assets/textures/vehicles/sedan.png"
    // deltaPath can be empty if no delta texture exists
    void registerVehicleType(const std::string& vehicleType, 
                             const std::string& basePath,
                             const std::string& deltaPath = "");

    // Get the base texture for a vehicle type
    std::shared_ptr<Texture> getVehicleTexture(const std::string& vehicleType);

    // Get the delta/damage texture for a vehicle type (may return nullptr if not registered)
    std::shared_ptr<Texture> getVehicleDeltaTexture(const std::string& vehicleType);

    // Check if a vehicle type is registered
    bool hasVehicleType(const std::string& vehicleType) const;

    // Get all registered vehicle types
    std::vector<std::string> getVehicleTypes() const;

private:
    TextureManager() = default;
    ~TextureManager() = default;

    // Texture cache: path -> texture
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_cache;

    // Alias map: alias -> path
    std::unordered_map<std::string, std::string> m_aliases;

    // Vehicle texture paths: vehicleType -> {basePath, deltaPath}
    struct VehicleTexturePaths {
        std::string basePath;
        std::string deltaPath;  // Empty if no delta texture
    };
    std::unordered_map<std::string, VehicleTexturePaths> m_vehicleTextures;
};
