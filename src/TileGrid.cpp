#include "TileGrid.hpp"
#include "Renderer.hpp"
#include <cmath>
#include <iostream>

TileGrid::TileGrid(const glm::ivec3& gridSize, float tileSize)
    : m_gridSize(gridSize), m_tileSize(tileSize) {
}

bool TileGrid::initialize() {
    // Load shared textures once
    m_grassTexture = std::make_shared<Texture>();
    if (!m_grassTexture->loadFromFile("assets/textures/grass.png")) {
        std::cerr << "Failed to load grass texture" << std::endl;
    }
    
    m_roadTexture = std::make_shared<Texture>();
    if (!m_roadTexture->loadFromFile("assets/textures/road.png")) {
        std::cerr << "Failed to load road texture" << std::endl;
    }
    
    m_wallTexture = std::make_shared<Texture>();
    if (!m_wallTexture->loadFromFile("assets/textures/wall.png")) {
        std::cerr << "Failed to load wall texture" << std::endl;
    }
    
    // Create all tiles
    m_tiles.clear();
    m_tiles.reserve(m_gridSize.x * m_gridSize.y * m_gridSize.z);
    
    for (int z = 0; z < m_gridSize.z; z++) {
        for (int y = 0; y < m_gridSize.y; y++) {
            for (int x = 0; x < m_gridSize.x; x++) {
                auto tile = std::make_unique<Tile>(glm::ivec3(x, y, z), m_tileSize);
                m_tiles.push_back(std::move(tile));
            }
        }
    }
    
    std::cout << "Created tile grid: " << m_gridSize.x << "x" << m_gridSize.y << "x" << m_gridSize.z 
              << " (" << m_tiles.size() << " tiles)" << std::endl;
    
    return true;
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

void TileGrid::createTestGrid() {
    // Ground level (z = 0): Mix of roads and grass
    // With the z offset, these tiles render from world z=-3.0 to z=0.0 (top surface at z=0)
    for (int y = 0; y < m_gridSize.y; y++) {
        for (int x = 0; x < m_gridSize.x; x++) {
            Tile* tile = getTile(x, y, 0);
            if (!tile) continue;
            
            // Create a road pattern (cross pattern)
            bool isRoad = (x == 5 || x == 10 || y == 5 || y == 10);
            
            if (isRoad) {
                // Road tile - solid top surface at z=0
                tile->setTopSurface(true, m_roadTexture, CarDirection::EastWest);
                
                // Vertical roads
                if (x == 5 || x == 10) {
                    tile->setCarDirection(CarDirection::NorthSouth);
                }
            } else {
                // Grass/ground tile - solid top surface at z=0
                tile->setTopSurface(true, m_grassTexture, CarDirection::None);
            }
            
            // All walls are walkable at ground level
            for (int i = 0; i < 4; i++) {
                tile->setWallWalkable(static_cast<WallDirection>(i), true);
            }
        }
    }
    
    // Level 1 (z = 1): Buildings (base at z=0, top at z=3.0)
    for (int y = 2; y < m_gridSize.y - 2; y += 4) {
        for (int x = 2; x < m_gridSize.x - 2; x += 4) {
            // Skip road intersections
            if ((x == 5 || x == 10) && (y == 5 || y == 10)) continue;
            
            // Skip area where tall building will be (x=7-8, y=7-8)
            if ((x == 6 && y == 6)) continue;
            
            // Create a building (2x2 tiles)
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int bx = x + dx;
                    int by = y + dy;
                    
                    if (bx >= m_gridSize.x || by >= m_gridSize.y) continue;
                    
                    Tile* tile = getTile(bx, by, 1);
                    if (!tile) continue;
                    
                    // Solid walls on building edges
                    tile->setWall(WallDirection::North, dy < 1, m_wallTexture);
                    tile->setWall(WallDirection::South, dy > 0, m_wallTexture);
                    tile->setWall(WallDirection::East, dx < 1, m_wallTexture);
                    tile->setWall(WallDirection::West, dx > 0, m_wallTexture);
                    
                    // Solid roof
                    tile->setTopSurface(true, m_grassTexture, CarDirection::None);
                }
            }
        }
    }
    
    // Levels 2-3: Empty air or additional building floors
    for (int z = 2; z < m_gridSize.z; z++) {
        for (int y = 0; y < m_gridSize.y; y++) {
            for (int x = 0; x < m_gridSize.x; x++) {
                Tile* tile = getTile(x, y, z);
                if (!tile) continue;
                
                // Empty air - no solid surfaces
                tile->setTopSurface(false, "", CarDirection::None);
                
                // All walls walkable (air)
                for (int i = 0; i < 4; i++) {
                    tile->setWallWalkable(static_cast<WallDirection>(i), true);
                }
            }
        }
    }
    
    // Add some taller buildings (starting from z=1 to connect to ground)
    for (int z = 1; z <= 3; z++) {
        for (int y = 7; y < 9; y++) {
            for (int x = 7; x < 9; x++) {
                Tile* tile = getTile(x, y, z);
                if (!tile) continue;
                
                // Building walls - solid on outer edges
                tile->setWall(WallDirection::North, y < 8, m_wallTexture);   // Solid when y == 8 (top edge)
                tile->setWall(WallDirection::South, y > 7, m_wallTexture);   // Solid when y == 7 (bottom edge)
                tile->setWall(WallDirection::East, x < 8, m_wallTexture);    // Solid when x == 8 (right edge)
                tile->setWall(WallDirection::West, x > 7, m_wallTexture);    // Solid when x == 7 (left edge)
                
                // Solid floor/ceiling
                tile->setTopSurface(true, m_grassTexture, CarDirection::None);
            }
        }
    }
    
    std::cout << "Created test grid configuration with roads, buildings, and multiple levels" << std::endl;
}
