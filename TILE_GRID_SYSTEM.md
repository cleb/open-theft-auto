# Tile Grid System Documentation

## Overview

The tile grid system replaces the previous road and building generation with a flexible 3D voxel-based world representation. Each tile in the grid can be independently configured with wall properties and surface characteristics.

## Architecture

### Core Components

#### 1. **Tile** (`Tile.hpp`, `Tile.cpp`)
Represents a single cubic tile in the 3D grid.

**Properties:**
- **Grid Position**: 3D integer coordinates (x, y, z)
- **World Position**: Calculated 3D world position based on grid position and tile size
- **Tile Size**: Physical size of each tile (default: 3.0 units)

**Wall Configuration** (4 walls per tile):
- `WallDirection::North` (+Y direction)
- `WallDirection::South` (-Y direction)  
- `WallDirection::East` (+X direction)
- `WallDirection::West` (-X direction)

Each wall has:
- `walkable` (bool): Whether entities can pass through
- `texturePath` (string): Path to wall texture
- `texture` (shared_ptr): Loaded texture resource

**Top Surface Configuration:**
- `solid` (bool): Whether the surface exists and can be stood on
- `texturePath` (string): Path to surface texture
- `texture` (shared_ptr): Loaded texture resource
- `carDirection` (enum): Direction(s) cars can travel on this surface
  - `None`: Not a road
  - `North`, `South`, `East`, `West`: One-way traffic
  - `SouthNorth`: Bidirectional S-N traffic
  - `WestEast`: Bidirectional W-E traffic
  - Diagonal: `NorthEast`, `NorthWest`, `SouthEast`, `SouthWest`
  - Bidirectional diagonal: `NorthEastSouthWest`, `NorthWestSouthEast`
  - Optional turns: `OptionalNorthEast`, `OptionalNorthWest`, `OptionalSouthEast`, `OptionalSouthWest`
    - Vehicles randomly treat these as a curve (same as the non-optional diagonal) or ignore them and continue straight ahead.
  - Bidirectional optional turns: `OptionalNorthEastSouthWest`, `OptionalNorthWestSouthEast`

**Key Methods:**
```cpp
// Wall configuration
void setWall(WallDirection dir, bool walkable, const string& texturePath = "");
void setWallWalkable(WallDirection dir, bool walkable);
void setWallTexture(WallDirection dir, const string& texturePath);
bool isWallWalkable(WallDirection dir) const;

// Top surface configuration
void setTopSurface(bool solid, const string& texturePath = "", CarDirection carDir = CarDirection::None);
void setTopSolid(bool solid);
void setTopTexture(const string& texturePath);
void setCarDirection(CarDirection dir);
bool isTopSolid() const;
CarDirection getCarDirection() const;

// Rendering
void generateMeshes();  // Creates meshes for non-walkable walls and solid surfaces
void render(Renderer* renderer);
```

#### 2. **TileGrid** (`TileGrid.hpp`, `TileGrid.cpp`)
Manages the entire 3D grid of tiles.

**Configuration:**
- Default size: 16x16x4 (width × height × depth)
- Default tile size: 3.0 units
- Configurable via constructor

**Key Methods:**
```cpp
bool initialize();                              // Creates all tiles
void render(Renderer* renderer);                // Renders entire grid
bool loadFromFile(const std::string& filePath); // Loads configuration from disk

// Tile access
Tile* getTile(int x, int y, int z);
Tile* getTile(const glm::ivec3& gridPos);
bool isValidPosition(int x, int y, int z) const;

// Coordinate conversion
glm::vec3 gridToWorld(const glm::ivec3& gridPos) const;
glm::ivec3 worldToGrid(const glm::vec3& worldPos) const;
```

### Integration with Scene

The `Scene` class has been updated to use the tile grid system:

```cpp
// Scene.hpp
std::unique_ptr<TileGrid> m_tileGrid;
TileGrid* getTileGrid() const { return m_tileGrid.get(); }

// Scene.cpp initialization
m_tileGrid = std::make_unique<TileGrid>(glm::ivec3(16, 16, 4), 3.0f);
m_tileGrid->initialize();
if (!m_tileGrid->loadFromFile("assets/levels/test_grid.tg")) {
  // Handle load failure (log, fallback, etc.)
}
```

Legacy road and building vectors are still present but deprecated.

## Usage Examples

### Creating a Simple Building

```cpp
// Get the tile grid from scene
TileGrid* grid = scene->getTileGrid();

// Define a 2x2 building at ground level (z=1)
for (int y = 5; y < 7; y++) {
    for (int x = 5; x < 7; x++) {
        Tile* tile = grid->getTile(x, y, 1);
        if (!tile) continue;
        
        // Make walls solid (non-walkable) on edges
        tile->setWall(WallDirection::North, y < 6, "");
        tile->setWall(WallDirection::South, y > 5, "");
        tile->setWall(WallDirection::East, x < 6, "");
        tile->setWall(WallDirection::West, x > 5, "");
        
        // Add solid roof
        tile->setTopSurface(true, "", CarDirection::None);
    }
}
```

### Creating Roads

```cpp
// Horizontal road at ground level (z=0)
for (int x = 0; x < 16; x++) {
    Tile* tile = grid->getTile(x, 8, 0);
    if (!tile) continue;
    
    // Ground with road marking
    tile->setTopSurface(true, "", CarDirection::EastWest);
    
    // All walls walkable (no barriers)
    for (int i = 0; i < 4; i++) {
        tile->setWallWalkable(static_cast<WallDirection>(i), true);
    }
}
```

### Creating Empty Air Space

```cpp
// Upper levels with no solid surfaces
for (int z = 2; z < 4; z++) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            Tile* tile = grid->getTile(x, y, z);
            if (!tile) continue;
            
            // No solid surface
            tile->setTopSurface(false, "", CarDirection::None);
            
            // All walls walkable
            for (int i = 0; i < 4; i++) {
                tile->setWallWalkable(static_cast<WallDirection>(i), true);
            }
        }
    }
}
```

## Test Configuration

Sample content is stored in `assets/levels/test_grid.tg`. The format uses line-based commands:

- `grid <w> <h> <d>` sets grid dimensions (must appear before tile commands).
- `tile_size <size>` updates world-space tile scale.
- `texture <alias> <path>` registers reusable texture IDs (aliases are case-sensitive).
- `tile x y z <properties>` applies properties to one tile.
- `fill x=a-b y=c-d z=e-f <properties>` applies the same properties to every tile in an inclusive region.

Property examples:

- `top=solid:grass`, `top=none` – toggles the walkable surface and texture.
- `car=eastwest`, `car=northsouth`, etc. – assigns vehicle directions.
- `slope=north|south|east|west|none` – turns the top surface into a simple ramp. The two vertices on the named side are raised one level (one `tile_size`), while the other two stay at this tile's base top level. Omit or set to `none` for a flat surface. The player, pedestrians, and vehicles query `TileGrid::getSurfaceHeight(...)` after each movement, so entities smoothly ride up and down slopes.
- On sloped tiles, solid side walls keep their normal rectangular wall face and automatically add the triangular ramp side face above it.
- `wallN=solid:wall`, `wallE=walkable` – controls wall collision and textures per face.

The bundled `test_grid.tg` recreates the previous hard-coded scene:

**Level 0 (Ground):**
- Cross-pattern road network at x/y = 5 and 10.
- Roads configured with appropriate car directions.
- Grass/ground tiles elsewhere.

**Level 1:**
- 2×2 buildings scattered across non-road areas.
- Buildings have solid perimeter walls and roofs.

**Levels 1-3 (Tall building core):**
- A taller 2×2 structure at (7–8, 7–8) spanning z = 1..3.
- Outer walls are solid; interior walls remain walkable.

## Future Enhancements

### Collision Detection
Add methods to TileGrid for checking collisions:
```cpp
bool canMoveTo(const glm::vec3& worldPos, const glm::vec3& direction) const;
bool isPositionWalkable(const glm::vec3& worldPos) const;
```

### Texture Support
Currently texture paths can be set but rendering uses a default shader. Future work:
- Load and bind textures per wall/surface
- Support for texture atlases
- Different materials (glass, metal, concrete, etc.)

### Dynamic Modification
Add runtime tile modification:
```cpp
void setTileConfiguration(int x, int y, int z, const TileConfig& config);
void clearTile(int x, int y, int z);
```

### Pathfinding Integration
The car direction enums can be used for:
- AI vehicle navigation
- Road network pathfinding
- Traffic flow simulation

### Serialization
Save/load tile grid configurations:
```cpp
bool saveToFile(const string& filename) const;
bool loadFromFile(const string& filename);
```

## Performance Considerations

- **Mesh Generation**: Meshes are generated lazily on first render
- **Culling**: Currently renders all tiles; future optimization could add frustum culling
- **Memory**: 16×16×4 grid = 1,024 tiles, each with up to 5 meshes (4 walls + 1 top)
- **Optimization Ideas**:
  - Merge adjacent tiles with same properties
  - Use instanced rendering for repeated tile types
  - Only create meshes for visible walls (adjacent to air/walkable tiles)

## File Structure

```
include/
  ├── Tile.hpp           # Tile class definition
  └── TileGrid.hpp       # Grid management class

src/
  ├── Tile.cpp           # Tile implementation
  └── TileGrid.cpp       # Grid implementation

Modified files:
  ├── include/Scene.hpp  # Added TileGrid member
  └── src/Scene.cpp      # Integrated tile grid system
```

## Building

The tile grid system is automatically included in the build:
```bash
cd build
cmake ..
make
```

The project will compile with the new tile system integrated alongside the existing game objects.
