#pragma once

#include "Tile.hpp"
#include "Texture.hpp"
#include "TextureManager.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <string>

struct LevelData;
class TileGrid;

namespace LevelSerialization {
struct GridAccess;
bool loadLevel(const std::string& filePath, TileGrid& grid, LevelData& data);
}

class TileGrid {
private:
    glm::ivec3 m_gridSize;      // Grid dimensions (width, height, depth)
    float m_tileSize;           // Size of each tile
    std::vector<std::unique_ptr<Tile>> m_tiles;
    
public:
    TileGrid(const glm::ivec3& gridSize = glm::ivec3(16, 16, 4), float tileSize = 3.0f);
    ~TileGrid() = default;

    bool initialize();
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

    // Grid management
    bool resize(const glm::ivec3& newSize);
    void reset();  // Clear all tiles and fill bottom layer with grass

    // World/Grid conversion
    glm::vec3 gridToWorld(const glm::ivec3& gridPos) const;
    glm::ivec3 worldToGrid(const glm::vec3& worldPos) const;
    bool canOccupy(const glm::vec3& startPos, const glm::vec3& endPos) const;
    bool isRoadTile(const glm::vec3& worldPos) const;
    bool isRoadTile(const glm::ivec3& gridPos) const;

private:
    int getIndex(int x, int y, int z) const;
    bool hasGroundSupport(const glm::ivec3& tilePos) const;
    bool rebuildTiles();

    friend struct LevelSerialization::GridAccess;
    friend bool LevelSerialization::loadLevel(const std::string& filePath, TileGrid& grid, LevelData& data);
};
