#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include "Mesh.hpp"
#include "Texture.hpp"

enum class WallDirection {
    North = 0,  // -Y
    South = 1,  // +Y
    East = 2,   // -X
    West = 3    // +X
};

enum class CarDirection {
    None = 0,
    North,
    South,
    East,
    West,
    NorthSouth,  // Bidirectional
    EastWest     // Bidirectional
};

struct WallData {
    bool walkable = true;
    std::string texturePath;
    std::shared_ptr<Texture> texture;
    
    WallData() = default;
    WallData(bool w, const std::string& path = "") 
        : walkable(w), texturePath(path) {}
};

struct TopSurfaceData {
    bool solid = true;
    std::string texturePath;
    std::shared_ptr<Texture> texture;
    CarDirection carDirection = CarDirection::None;
    
    TopSurfaceData() = default;
    TopSurfaceData(bool s, const std::string& path = "", CarDirection dir = CarDirection::None)
        : solid(s), texturePath(path), carDirection(dir) {}
};

class Tile {
private:
    glm::ivec3 m_gridPosition;  // Grid coordinates (x, y, z)
    glm::vec3 m_worldPosition;  // World position
    float m_tileSize;           // Size of each tile
    
    WallData m_walls[4];        // North, South, East, West
    TopSurfaceData m_topSurface;
    
    std::unique_ptr<Mesh> m_wallMeshes[4];
    std::unique_ptr<Mesh> m_topMesh;
    
    bool m_meshesGenerated;

public:
    Tile(const glm::ivec3& gridPos, float tileSize = 1.0f);
    ~Tile() = default;
    
    // Wall configuration
    void setWall(WallDirection dir, bool walkable, const std::string& texturePath = "");
    void setWall(WallDirection dir, bool walkable, std::shared_ptr<Texture> texture);
    void setWallWalkable(WallDirection dir, bool walkable);
    void setWallTexture(WallDirection dir, const std::string& texturePath);
    void setWallTexture(WallDirection dir, std::shared_ptr<Texture> texture);
    const WallData& getWall(WallDirection dir) const;
    bool isWallWalkable(WallDirection dir) const;
    
    // Top surface configuration
    void setTopSurface(bool solid, const std::string& texturePath = "", CarDirection carDir = CarDirection::None);
    void setTopSurface(bool solid, std::shared_ptr<Texture> texture, CarDirection carDir = CarDirection::None);
    void setTopSolid(bool solid);
    void setTopTexture(const std::string& texturePath);
    void setTopTexture(std::shared_ptr<Texture> texture);
    void setCarDirection(CarDirection dir);
    const TopSurfaceData& getTopSurface() const { return m_topSurface; }
    bool isTopSolid() const { return m_topSurface.solid; }
    CarDirection getCarDirection() const { return m_topSurface.carDirection; }
    
    // Position getters
    const glm::ivec3& getGridPosition() const { return m_gridPosition; }
    const glm::vec3& getWorldPosition() const { return m_worldPosition; }
    float getTileSize() const { return m_tileSize; }
    
    // Rendering
    void generateMeshes();
    void render(class Renderer* renderer);
    
private:
    void createWallMesh(WallDirection dir);
    void createTopMesh();
    void updateWorldPosition();
};
