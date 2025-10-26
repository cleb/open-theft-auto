#include "Tile.hpp"
#include "Renderer.hpp"
#include <iostream>

Tile::Tile(const glm::ivec3& gridPos, float tileSize) 
    : m_gridPosition(gridPos), m_tileSize(tileSize), m_meshesGenerated(false) {
    updateWorldPosition();
    
    // Initialize with default values
    for (int i = 0; i < 4; i++) {
        m_walls[i] = WallData(true, "");  // All walls walkable by default
    }
    m_topSurface = TopSurfaceData(false, "", CarDirection::None);
}

void Tile::updateWorldPosition() {
    // Offset z by -1 tile so grid z=0 renders from world z=-tileSize to z=0
    m_worldPosition = glm::vec3(
        m_gridPosition.x * m_tileSize,
        m_gridPosition.y * m_tileSize,
        (m_gridPosition.z - 1) * m_tileSize  // Shift down by 1 level
    );
}

void Tile::setWall(WallDirection dir, bool walkable, const std::string& texturePath) {
    int index = static_cast<int>(dir);
    m_walls[index].walkable = walkable;
    m_walls[index].texturePath = texturePath;
    m_walls[index].texture = nullptr;  // Will be loaded when needed
    m_meshesGenerated = false;  // Regenerate meshes
}

void Tile::setWall(WallDirection dir, bool walkable, std::shared_ptr<Texture> texture) {
    int index = static_cast<int>(dir);
    m_walls[index].walkable = walkable;
    m_walls[index].texture = texture;
    m_meshesGenerated = false;  // Regenerate meshes
}

void Tile::setWallWalkable(WallDirection dir, bool walkable) {
    int index = static_cast<int>(dir);
    m_walls[index].walkable = walkable;
}

void Tile::setWallTexture(WallDirection dir, const std::string& texturePath) {
    int index = static_cast<int>(dir);
    m_walls[index].texturePath = texturePath;
    m_walls[index].texture = nullptr;  // Will be loaded when needed
    m_meshesGenerated = false;  // Regenerate meshes
}

void Tile::setWallTexture(WallDirection dir, std::shared_ptr<Texture> texture) {
    int index = static_cast<int>(dir);
    m_walls[index].texture = texture;
    m_meshesGenerated = false;  // Regenerate meshes
}

const WallData& Tile::getWall(WallDirection dir) const {
    return m_walls[static_cast<int>(dir)];
}

bool Tile::isWallWalkable(WallDirection dir) const {
    return m_walls[static_cast<int>(dir)].walkable;
}

void Tile::setTopSurface(bool solid, const std::string& texturePath, CarDirection carDir) {
    m_topSurface.solid = solid;
    m_topSurface.texturePath = texturePath;
    m_topSurface.carDirection = carDir;
    m_topSurface.texture = nullptr;  // Will be loaded when needed
    m_meshesGenerated = false;  // Regenerate meshes
}

void Tile::setTopSurface(bool solid, std::shared_ptr<Texture> texture, CarDirection carDir) {
    m_topSurface.solid = solid;
    m_topSurface.texture = texture;
    m_topSurface.carDirection = carDir;
    m_meshesGenerated = false;  // Regenerate meshes
}

void Tile::setTopSolid(bool solid) {
    m_topSurface.solid = solid;
}

void Tile::setTopTexture(const std::string& texturePath) {
    m_topSurface.texturePath = texturePath;
    m_topSurface.texture = nullptr;  // Will be loaded when needed
    m_meshesGenerated = false;  // Regenerate meshes
}

void Tile::setTopTexture(std::shared_ptr<Texture> texture) {
    m_topSurface.texture = texture;
    m_meshesGenerated = false;  // Regenerate meshes
}

void Tile::setCarDirection(CarDirection dir) {
    m_topSurface.carDirection = dir;
}

void Tile::applyUpdate(const Update& update,
                       const std::function<std::string(const std::string&)>& resolveTexture,
                       const std::function<std::shared_ptr<Texture>(const std::string&)>& loadTexture) {
    if (update.topSpecified) {
        if (update.topSolid) {
            std::string resolved = update.topTextureId;
            if (!resolved.empty() && resolveTexture) {
                resolved = resolveTexture(resolved);
            }

            std::shared_ptr<Texture> texture;
            if (!resolved.empty() && loadTexture) {
                texture = loadTexture(resolved);
            }

            setTopSurface(true, resolved, CarDirection::None);
            if (texture) {
                setTopTexture(texture);
            }
        } else {
            setTopSurface(false, "", CarDirection::None);
        }
    }

    if (update.carSpecified) {
        setCarDirection(update.carDirection);
    }

    for (int i = 0; i < 4; ++i) {
        const WallUpdate& wall = update.walls[i];
        if (!wall.specified) {
            continue;
        }

        const auto dir = static_cast<WallDirection>(i);
        std::string resolved = wall.textureId;
        if (!resolved.empty() && resolveTexture) {
            resolved = resolveTexture(resolved);
        }

        setWall(dir, wall.walkable, resolved);

        if (!resolved.empty() && loadTexture) {
            if (auto texture = loadTexture(resolved)) {
                setWallTexture(dir, texture);
            }
        }
    }
}

void Tile::generateMeshes() {
    // Create wall meshes for non-walkable walls
    for (int i = 0; i < 4; i++) {
        if (!m_walls[i].walkable) {
            createWallMesh(static_cast<WallDirection>(i));
        } else {
            m_wallMeshes[i] = nullptr;
        }
    }
    
    // Create top mesh if solid
    if (m_topSurface.solid) {
        createTopMesh();
    } else {
        m_topMesh = nullptr;
    }
    
    m_meshesGenerated = true;
}

void Tile::createWallMesh(WallDirection dir) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    float halfSize = m_tileSize / 2.0f;
    float height = m_tileSize;  // Wall height
    
    // Define vertices based on wall direction
    switch (dir) {
        case WallDirection::North:  // +Y wall
            vertices.push_back({{-halfSize, halfSize, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
            vertices.push_back({{ halfSize, halfSize, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
            vertices.push_back({{ halfSize, halfSize, height}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
            vertices.push_back({{-halfSize, halfSize, height}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
            break;
            
        case WallDirection::South:  // -Y wall
            vertices.push_back({{-halfSize, -halfSize, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
            vertices.push_back({{ halfSize, -halfSize, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}});
            vertices.push_back({{ halfSize, -halfSize, height}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}});
            vertices.push_back({{-halfSize, -halfSize, height}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}});
            break;
            
        case WallDirection::East:  // +X wall
            vertices.push_back({{halfSize, -halfSize, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            vertices.push_back({{halfSize,  halfSize, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
            vertices.push_back({{halfSize,  halfSize, height}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
            vertices.push_back({{halfSize, -halfSize, height}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
            break;
            
        case WallDirection::West:  // -X wall
            vertices.push_back({{-halfSize,  halfSize, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            vertices.push_back({{-halfSize, -halfSize, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
            vertices.push_back({{-halfSize, -halfSize, height}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
            vertices.push_back({{-halfSize,  halfSize, height}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
            break;
    }
    
    // Two triangles per wall
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(2);
    indices.push_back(3);
    indices.push_back(0);
    
    auto& wallData = m_walls[static_cast<int>(dir)];
    m_wallMeshes[static_cast<int>(dir)] = std::make_unique<Mesh>(vertices, indices);

    // Load texture if specified
    const std::string& texPath = wallData.texturePath;
    if (!texPath.empty() && !wallData.texture) {
        wallData.texture = std::make_shared<Texture>();
        if (!wallData.texture->loadFromFile(texPath)) {
            std::cerr << "Failed to load wall texture: " << texPath << std::endl;
        }
    }
    if (wallData.texture) {
        m_wallMeshes[static_cast<int>(dir)]->setTexture(wallData.texture);
    }
}

void Tile::createTopMesh() {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    float halfSize = m_tileSize / 2.0f;
    float height = m_tileSize;  // Top surface is at the top of the tile
    
    // Top surface (horizontal quad)
    vertices.push_back({{-halfSize, -halfSize, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ halfSize, -halfSize, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ halfSize,  halfSize, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-halfSize,  halfSize, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});
    
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(2);
    indices.push_back(3);
    indices.push_back(0);
    
    m_topMesh = std::make_unique<Mesh>(vertices, indices);

    // Load texture if specified
    if (!m_topSurface.texturePath.empty() && !m_topSurface.texture) {
        m_topSurface.texture = std::make_shared<Texture>();
        if (!m_topSurface.texture->loadFromFile(m_topSurface.texturePath)) {
            std::cerr << "Failed to load top surface texture: " << m_topSurface.texturePath << std::endl;
        }
    }
    if (m_topSurface.texture) {
        m_topMesh->setTexture(m_topSurface.texture);
    }
}

void Tile::render(Renderer* renderer) {
    if (!renderer) return;

    if (!m_meshesGenerated) {
        generateMeshes();
    }

    // Set up model matrix for this tile
    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_worldPosition);

    // Render non-walkable walls
    for (int i = 0; i < 4; i++) {
        if (m_wallMeshes[i]) {
            renderer->renderMesh(*m_wallMeshes[i], model, "model");
        }
    }

    // Render top surface
    if (m_topMesh) {
        renderer->renderMesh(*m_topMesh, model, "model");
    }
}

void Tile::copyFrom(const Tile& other) {
    if (this == &other) {
        return;
    }

    m_topSurface = other.m_topSurface;
    m_topMesh.reset();

    for (int i = 0; i < 4; ++i) {
        m_walls[i] = other.m_walls[i];
        m_wallMeshes[i].reset();
    }

    m_meshesGenerated = false;
}
