#include "TileGrid.hpp"
#include "TextureManager.hpp"
#include "Renderer.hpp"
#include "ViewBounds.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

TileGrid::TileGrid(const glm::ivec3& gridSize, float tileSize)
    : m_gridSize(gridSize), m_tileSize(tileSize) {
}

bool TileGrid::initialize() {
    // Register texture aliases with the global TextureManager
    auto& texMgr = TextureManager::instance();
    texMgr.registerAlias("grass", "assets/textures/grass.png");
    texMgr.registerAlias("road", "assets/textures/road.png");
    texMgr.registerAlias("wall", "assets/textures/wall.png");
    texMgr.registerAlias("car", "assets/textures/car.png");
    texMgr.registerAlias("sidewalk", "assets/textures/sidewalk.jpeg");

    // Register the default vehicle type (car) for use by TrafficManager and others
    texMgr.registerVehicleType("car", "assets/textures/car.png");

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

void TileGrid::reset() {
    // Clear all tiles by rebuilding them
    rebuildTiles();
    
    // Load grass texture once using the TextureManager
    const std::string grassTexturePath = "assets/textures/grass.png";
    std::shared_ptr<Texture> grassTexture = TextureManager::instance().getTextureFromPath(grassTexturePath);
    
    // Fill the bottom layer (z=0) with grass (low drivability)
    for (int y = 0; y < m_gridSize.y; ++y) {
        for (int x = 0; x < m_gridSize.x; ++x) {
            Tile* tile = getTile(x, y, 0);
            if (tile) {
                tile->setTopSurface(true, grassTexturePath, grassTexture, CarDirection::None);
                tile->setDrivability(0.3f);  // Grass is hard to drive on
            }
        }
    }
    
    std::cout << "Reset tile grid to default state with grass on bottom layer" << std::endl;
}

void TileGrid::render(Renderer* renderer) {
    if (!renderer) return;

    int minX = 0;
    int maxX = m_gridSize.x - 1;
    int minY = 0;
    int maxY = m_gridSize.y - 1;

    if (Camera* camera = renderer->getCamera()) {
        constexpr int kTilePadding = 2;
        const ViewBounds bounds = ViewBounds::calculate(camera, renderer->getFovRadians(), renderer->getAspectRatio());
        const float halfTile = m_tileSize * 0.5f;

        minX = std::max(
            0,
            static_cast<int>(std::floor((bounds.minX + halfTile) / m_tileSize)) - kTilePadding
        );
        maxX = std::min(
            m_gridSize.x - 1,
            static_cast<int>(std::floor((bounds.maxX + halfTile) / m_tileSize)) + kTilePadding
        );
        minY = std::max(
            0,
            static_cast<int>(std::floor((bounds.minY + halfTile) / m_tileSize)) - kTilePadding
        );
        maxY = std::min(
            m_gridSize.y - 1,
            static_cast<int>(std::floor((bounds.maxY + halfTile) / m_tileSize)) + kTilePadding
        );
    }

    if (minX > maxX || minY > maxY) {
        return;
    }

    const int rowStride = m_gridSize.x;
    const int layerStride = m_gridSize.x * m_gridSize.y;
    for (int z = 0; z < m_gridSize.z; ++z) {
        const int layerBase = z * layerStride;
        for (int y = minY; y <= maxY; ++y) {
            int index = layerBase + y * rowStride + minX;
            for (int x = minX; x <= maxX; ++x, ++index) {
                Tile* tile = m_tiles[index].get();
                if (!tile || !tile->hasRenderableGeometry()) {
                    continue;
                }
                tile->render(renderer);
            }
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
    // Check the tile BELOW the entity (same logic as hasGroundSupport)
    // Entities walk on top of tiles, so we check z-1
    gridPos.z -= 1;
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

bool TileGrid::isSidewalkTile(const glm::vec3& worldPos) const {
    glm::ivec3 gridPos = worldToGrid(worldPos);
    // Check the tile BELOW the entity (same logic as hasGroundSupport)
    // Entities walk on top of tiles, so we check z-1
    gridPos.z -= 1;
    if (!isValidPosition(gridPos)) {
        return false;
    }
    return isSidewalkTile(gridPos);
}

bool TileGrid::isSidewalkTile(const glm::ivec3& gridPos) const {
    if (!isValidPosition(gridPos)) {
        return false;
    }

    const Tile* tile = getTile(gridPos);
    if (!tile) {
        return false;
    }

    return tile->isSidewalk();
}

float TileGrid::getDrivability(const glm::vec3& worldPos) const {
    glm::ivec3 gridPos = worldToGrid(worldPos);
    // Check the tile BELOW the entity (same logic as hasGroundSupport)
    gridPos.z -= 1;
    if (!isValidPosition(gridPos)) {
        return 0.5f;  // Default to moderate drivability for out-of-bounds
    }
    return getDrivability(gridPos);
}

float TileGrid::getDrivability(const glm::ivec3& gridPos) const {
    if (!isValidPosition(gridPos)) {
        return 0.5f;
    }

    const Tile* tile = getTile(gridPos);
    if (!tile) {
        return 0.5f;
    }

    return tile->getTopSurface().drivability;
}
