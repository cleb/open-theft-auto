#pragma once

#include "Tile.hpp"
#include "Texture.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <string>

class TileGrid {
private:
    glm::ivec3 m_gridSize;      // Grid dimensions (width, height, depth)
    float m_tileSize;           // Size of each tile
    std::vector<std::unique_ptr<Tile>> m_tiles;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textureCache;
    std::unordered_map<std::string, std::string> m_textureAliases;

public:
    struct VehicleSpawn {
        glm::ivec3 gridPosition{0};
        float rotationDegrees = 0.0f;
        std::string texturePath;
        glm::vec2 size = glm::vec2(1.5f, 3.0f);
    };

    TileGrid(const glm::ivec3& gridSize = glm::ivec3(16, 16, 4), float tileSize = 3.0f);
    ~TileGrid() = default;
    
    bool initialize();
    bool loadFromFile(const std::string& filePath);
    bool saveToFile(const std::string& filePath) const;
    void render(class Renderer* renderer);
    
    // Tile access
    Tile* getTile(int x, int y, int z);
    Tile* getTile(const glm::ivec3& gridPos);
    const Tile* getTile(int x, int y, int z) const;
    const Tile* getTile(const glm::ivec3& gridPos) const;
    
    // Grid info
    const glm::ivec3& getGridSize() const { return m_gridSize; }
    float getTileSize() const { return m_tileSize; }
    bool isValidPosition(int x, int y, int z) const;
    bool isValidPosition(const glm::ivec3& gridPos) const;
    const std::unordered_map<std::string, std::string>& getTextureAliases() const { return m_textureAliases; }
    
    // World/Grid conversion
    glm::vec3 gridToWorld(const glm::ivec3& gridPos) const;
    glm::ivec3 worldToGrid(const glm::vec3& worldPos) const;
    bool canOccupy(const glm::vec3& startPos, const glm::vec3& endPos) const;
    bool isRoadTile(const glm::vec3& worldPos) const;
    bool isRoadTile(const glm::ivec3& gridPos) const;

    // Vehicle spawns
    const std::vector<VehicleSpawn>& getVehicleSpawns() const { return m_vehicleSpawns; }
    VehicleSpawn* findVehicleSpawn(const glm::ivec3& gridPos);
    const VehicleSpawn* findVehicleSpawn(const glm::ivec3& gridPos) const;
    void addOrUpdateVehicleSpawn(const VehicleSpawn& spawn);
    bool removeVehicleSpawnAt(const glm::ivec3& gridPos);

private:
    int getIndex(int x, int y, int z) const;
    bool hasGroundSupport(const glm::ivec3& tilePos) const;
    bool rebuildTiles();
    void registerTextureAlias(const std::string& alias, const std::string& path);
    std::shared_ptr<Texture> loadTexture(const std::string& identifier);
    std::shared_ptr<Texture> loadTextureFromPath(const std::string& path);
    std::string resolveTexturePath(const std::string& identifier) const;

    std::vector<VehicleSpawn> m_vehicleSpawns;
};
