#include "TileGrid.hpp"
#include "Renderer.hpp"
#include <cmath>
#include <iostream>

TileGrid::TileGrid(const glm::ivec3& gridSize, float tileSize)
    : m_gridSize(gridSize), m_tileSize(tileSize) {
}

bool TileGrid::initialize() {
    m_textureCache.clear();
    m_textureAliases.clear();

    registerTextureAlias("grass", "assets/textures/grass.png");
    registerTextureAlias("road", "assets/textures/road.png");
    registerTextureAlias("wall", "assets/textures/wall.png");
    registerTextureAlias("car", "assets/textures/car.png");

    if (!rebuildTiles()) {
        std::cerr << "Failed to initialize tile grid tiles" << std::endl;
        return false;
    }

    std::cout << "Initialized tile grid: " << m_gridSize.x << "x" << m_gridSize.y << "x" << m_gridSize.z
              << " (" << m_tiles.size() << " tiles)" << std::endl;

    return true;
}

bool TileGrid::rebuildTiles() {
    if (m_gridSize.x <= 0 || m_gridSize.y <= 0 || m_gridSize.z <= 0) {
        std::cerr << "Invalid grid size: " << m_gridSize.x << "x" << m_gridSize.y << "x" << m_gridSize.z << std::endl;
        return false;
    }

    m_tiles.clear();
    const size_t total = static_cast<size_t>(m_gridSize.x) * static_cast<size_t>(m_gridSize.y) * static_cast<size_t>(m_gridSize.z);
    m_tiles.reserve(total);

    for (int z = 0; z < m_gridSize.z; ++z) {
        for (int y = 0; y < m_gridSize.y; ++y) {
            for (int x = 0; x < m_gridSize.x; ++x) {
                m_tiles.push_back(std::make_unique<Tile>(glm::ivec3(x, y, z), m_tileSize));
            }
        }
    }

    return true;
}

bool TileGrid::resize(const glm::ivec3& newSize) {
    if (newSize.x <= 0 || newSize.y <= 0 || newSize.z <= 0) {
        std::cerr << "TileGrid::resize: invalid grid size requested: " << newSize.x << "x" << newSize.y << "x" << newSize.z
                  << std::endl;
        return false;
    }

    if (newSize == m_gridSize) {
        return true;
    }

    const glm::ivec3 oldSize = m_gridSize;
    auto oldTiles = std::move(m_tiles);

    m_gridSize = newSize;
    if (!rebuildTiles()) {
        m_gridSize = oldSize;
        m_tiles = std::move(oldTiles);
        return false;
    }

    const int copyX = std::min(oldSize.x, newSize.x);
    const int copyY = std::min(oldSize.y, newSize.y);
    const int copyZ = std::min(oldSize.z, newSize.z);

    auto oldIndex = [&](int x, int y, int z) -> size_t {
        return static_cast<size_t>((z * oldSize.y + y) * oldSize.x + x);
    };

    for (int z = 0; z < copyZ; ++z) {
        for (int y = 0; y < copyY; ++y) {
            for (int x = 0; x < copyX; ++x) {
                const size_t idx = oldIndex(x, y, z);
                if (idx >= oldTiles.size()) {
                    continue;
                }

                const Tile* oldTile = oldTiles[idx].get();
                Tile* newTile = getTile(x, y, z);
                if (!oldTile || !newTile) {
                    continue;
                }

                newTile->copyFrom(*oldTile);
            }
        }
    }

    std::cout << "Resized tile grid: " << oldSize.x << "x" << oldSize.y << "x" << oldSize.z << " -> " << newSize.x << "x"
              << newSize.y << "x" << newSize.z << std::endl;

    return true;
}

void TileGrid::registerTextureAlias(const std::string& alias, const std::string& path) {
    if (alias.empty() || path.empty()) {
        return;
    }
    m_textureAliases[alias] = path;
}

std::shared_ptr<Texture> TileGrid::loadTexture(const std::string& identifier) {
    const std::string resolvedPath = resolveTexturePath(identifier);
    return loadTextureFromPath(resolvedPath);
}

std::shared_ptr<Texture> TileGrid::loadTextureFromPath(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    auto cacheIt = m_textureCache.find(path);
    if (cacheIt != m_textureCache.end()) {
        return cacheIt->second;
    }

    auto texture = std::make_shared<Texture>();
    if (!texture->loadFromFile(path)) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return nullptr;
    }

    m_textureCache[path] = texture;
    return texture;
}

std::string TileGrid::resolveTexturePath(const std::string& identifier) const {
    auto aliasIt = m_textureAliases.find(identifier);
    if (aliasIt != m_textureAliases.end()) {
        return aliasIt->second;
    }
    return identifier;
}

void TileGrid::render(Renderer* renderer) {
    if (!renderer) return;
    
    for (auto& tile : m_tiles) {
        if (tile) {
            tile->render(renderer);
        }
    }
}

Tile* TileGrid::getTile(int x, int y, int z) {
    if (!isValidPosition(x, y, z)) {
        return nullptr;
    }
    
    int index = getIndex(x, y, z);
    return m_tiles[index].get();
}

Tile* TileGrid::getTile(const glm::ivec3& gridPos) {
    return getTile(gridPos.x, gridPos.y, gridPos.z);
}

const Tile* TileGrid::getTile(int x, int y, int z) const {
    if (!isValidPosition(x, y, z)) {
        return nullptr;
    }

    int index = getIndex(x, y, z);
    return m_tiles[index].get();
}

const Tile* TileGrid::getTile(const glm::ivec3& gridPos) const {
    return getTile(gridPos.x, gridPos.y, gridPos.z);
}

bool TileGrid::isValidPosition(int x, int y, int z) const {
    return x >= 0 && x < m_gridSize.x &&
           y >= 0 && y < m_gridSize.y &&
           z >= 0 && z < m_gridSize.z;
}

bool TileGrid::isValidPosition(const glm::ivec3& gridPos) const {
    return isValidPosition(gridPos.x, gridPos.y, gridPos.z);
}

int TileGrid::getIndex(int x, int y, int z) const {
    return x + y * m_gridSize.x + z * m_gridSize.x * m_gridSize.y;
}

bool TileGrid::hasGroundSupport(const glm::ivec3& tilePos) const {
    int groundZ = tilePos.z - 1;
    if (groundZ < 0) {
        return false;
    }

    const Tile* groundTile = getTile(tilePos.x, tilePos.y, groundZ);
    if (!groundTile) {
        return false;
    }

    return groundTile->isTopSolid();
}

glm::vec3 TileGrid::gridToWorld(const glm::ivec3& gridPos) const {
    return glm::vec3(
        gridPos.x * m_tileSize,
        gridPos.y * m_tileSize,
        (gridPos.z - 1) * m_tileSize
    );
}

glm::ivec3 TileGrid::worldToGrid(const glm::vec3& worldPos) const {
    const float halfSize = m_tileSize * 0.5f;
    return glm::ivec3(
        static_cast<int>(std::floor((worldPos.x + halfSize) / m_tileSize)),
        static_cast<int>(std::floor((worldPos.y + halfSize) / m_tileSize)),
        static_cast<int>(std::floor((worldPos.z + m_tileSize) / m_tileSize))
    );
}

bool TileGrid::canOccupy(const glm::vec3& startPos, const glm::vec3& endPos) const {
    glm::ivec3 startTile = worldToGrid(startPos);
    glm::ivec3 endTile = worldToGrid(endPos);

    if (!isValidPosition(startTile.x, startTile.y, startTile.z)) {
        return false;
    }

    if (!isValidPosition(endTile.x, endTile.y, endTile.z)) {
        return false;
    }

    if (startTile == endTile) {
        return hasGroundSupport(endTile);
    }

    glm::ivec3 diff = endTile - startTile;
    if (diff.z != 0) {
        return false;
    }

    int manhattan = glm::abs(diff.x) + glm::abs(diff.y);
    if (manhattan > 1) {
        return false;
    }

    WallDirection fromDir;
    WallDirection toDir;

    if (diff.x == 1) {
        fromDir = WallDirection::East;
        toDir = WallDirection::West;
    } else if (diff.x == -1) {
        fromDir = WallDirection::West;
        toDir = WallDirection::East;
    } else if (diff.y == 1) {
        fromDir = WallDirection::North;
        toDir = WallDirection::South;
    } else if (diff.y == -1) {
        fromDir = WallDirection::South;
        toDir = WallDirection::North;
    } else {
        return hasGroundSupport(endTile);
    }

    const Tile* fromTile = getTile(startTile);
    const Tile* toTile = getTile(endTile);

    if (!fromTile || !toTile) {
        return false;
    }

    if (!fromTile->isWallWalkable(fromDir) || !toTile->isWallWalkable(toDir)) {
        return false;
    }

    return hasGroundSupport(endTile);
}

bool TileGrid::isRoadTile(const glm::vec3& worldPos) const {
    glm::ivec3 gridPos = worldToGrid(worldPos);
    if (!isValidPosition(gridPos)) {
        return false;
    }
    return isRoadTile(gridPos);
}

bool TileGrid::isRoadTile(const glm::ivec3& gridPos) const {
    if (!isValidPosition(gridPos)) {
        return false;
    }

    const Tile* tile = getTile(gridPos);
    if (!tile) {
        return false;
    }

    return tile->getCarDirection() != CarDirection::None;
}

