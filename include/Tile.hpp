#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Mesh.hpp"
#include "Texture.hpp"
#include "LevelData.hpp"

enum class WallDirection {
    North = 0,  // +Y
    South = 1,  // -Y
    East = 2,   // +X
    West = 3    // -X
};

enum class CarDirection {
    None = 0,
    North,
    South,
    East,
    West,
    SouthNorth,  // Bidirectional
    WestEast,    // Bidirectional
    // Diagonal directions
    NorthEast,
    NorthWest,
    SouthEast,
    SouthWest,
    // Bidirectional diagonals
    NorthEastSouthWest,
    NorthWestSouthEast,
    // Optional turn directions: vehicles randomly treat as curve or go straight
    OptionalNorthEast,
    OptionalNorthWest,
    OptionalSouthEast,
    OptionalSouthWest,
    // Bidirectional optional turns
    OptionalNorthEastSouthWest,
    OptionalNorthWestSouthEast
};

// Sidewalk directions - always bidirectional for pedestrian traffic
enum class SidewalkDirection {
    None = 0,
    NorthSouth,  // Bidirectional N-S
    EastWest,    // Bidirectional E-W
    // Diagonal directions (bidirectional)
    NorthEastSouthWest,
    NorthWestSouthEast
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
    SidewalkDirection sidewalkDirection = SidewalkDirection::None;
    // Drivability: 1.0 = fully drivable (road/sidewalk), 0.0 = impassable
    // Affects vehicle max speed based on their drivabilityImpact setting
    float drivability = 1.0f;
    // Spawn weights for each vehicle type on this road tile
    // Key: vehicle type ID (e.g., "sedan", "pickup")
    std::vector<VehicleSpawnWeight> vehicleSpawnWeights;
    
    TopSurfaceData() = default;
    TopSurfaceData(bool s, const std::string& path = "", CarDirection dir = CarDirection::None, SidewalkDirection sDir = SidewalkDirection::None, float driv = 1.0f)
        : solid(s), texturePath(path), carDirection(dir), sidewalkDirection(sDir), drivability(driv) {}
    
    // Get spawn weight for a specific vehicle type (returns default 1.0 if not specified)
    float getSpawnWeight(const std::string& typeId) const {
        for (const auto& weight : vehicleSpawnWeights) {
            if (weight.typeId == typeId) {
                return weight.weight;
            }
        }
        return 1.0f;  // Default weight
    }
    
    // Set spawn weight for a specific vehicle type
    void setSpawnWeight(const std::string& typeId, float weight) {
        for (auto& w : vehicleSpawnWeights) {
            if (w.typeId == typeId) {
                w.weight = weight;
                return;
            }
        }
        // Not found, add new entry
        vehicleSpawnWeights.push_back({typeId, weight});
    }
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
    struct WallUpdate {
        bool specified = false;
        bool walkable = true;
        std::string textureId;
    };

    struct Update {
        bool topSpecified = false;
        bool topSolid = false;
        std::string topTextureId;
        bool carSpecified = false;
        CarDirection carDirection = CarDirection::None;
        WallUpdate walls[4];
    };

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
    void setTopSurface(bool solid, const std::string& texturePath, std::shared_ptr<Texture> texture, CarDirection carDir = CarDirection::None);
    void setTopSolid(bool solid);
    void setTopTexture(const std::string& texturePath);
    void setTopTexture(std::shared_ptr<Texture> texture);
    void setCarDirection(CarDirection dir);
    void setSidewalkDirection(SidewalkDirection dir);
    const TopSurfaceData& getTopSurface() const { return m_topSurface; }
    bool isTopSolid() const { return m_topSurface.solid; }
    bool hasRenderableGeometry() const;
    CarDirection getCarDirection() const { return m_topSurface.carDirection; }
    SidewalkDirection getSidewalkDirection() const { return m_topSurface.sidewalkDirection; }
    bool isSidewalk() const { return m_topSurface.sidewalkDirection != SidewalkDirection::None; }
    float getDrivability() const { return m_topSurface.drivability; }
    void setDrivability(float drivability) { m_topSurface.drivability = drivability; }
    
    // Vehicle spawn weights for traffic spawning on road tiles
    float getVehicleSpawnWeight(const std::string& typeId) const { return m_topSurface.getSpawnWeight(typeId); }
    void setVehicleSpawnWeight(const std::string& typeId, float weight) { m_topSurface.setSpawnWeight(typeId, weight); }
    const std::vector<VehicleSpawnWeight>& getVehicleSpawnWeights() const { return m_topSurface.vehicleSpawnWeights; }
    
    // Position getters
    const glm::ivec3& getGridPosition() const { return m_gridPosition; }
    const glm::vec3& getWorldPosition() const { return m_worldPosition; }
    float getTileSize() const { return m_tileSize; }
    
    // Rendering
    void generateMeshes();
    void render(class Renderer* renderer);

    void applyUpdate(const Update& update,
                     const std::function<std::string(const std::string&)>& resolveTexture,
                     const std::function<std::shared_ptr<Texture>(const std::string&)>& loadTexture);
    void copyFrom(const Tile& other);

private:
    void createWallMesh(WallDirection dir);
    void createTopMesh();
    void updateWorldPosition();
};
