#include "TileGrid.hpp"
#include "Mesh.hpp"
#include "Renderer.hpp"
#include "TextureManager.hpp"

#include <cassert>

// Rendering-only stubs: this test exercises grid occupancy/fall queries and
// never touches the OpenGL rendering path.
Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices)
    : m_vertices(vertices), m_indices(indices), m_VAO(0), m_VBO(0), m_EBO(0), m_setupDone(false) {}
Mesh::~Mesh() = default;
void Mesh::setTexture(const std::shared_ptr<Texture>& texture) { m_texture = texture; }
void Renderer::renderMesh(const Mesh&, const glm::mat4&, const std::string&, const glm::vec3&) {}
TextureManager& TextureManager::instance() {
    static TextureManager manager;
    return manager;
}
void TextureManager::registerAlias(const std::string&, const std::string&) {}
void TextureManager::registerVehicleType(const std::string&, const std::string&, const std::string&) {}
std::shared_ptr<Texture> TextureManager::getTextureFromPath(const std::string&) { return nullptr; }

namespace {

constexpr float kTileSize = 3.0f;

// Fills a grid so that column (1,1) is a plateau of `plateauHeight` tiles
// stacked on the ground layer, surrounded by flat ground.
void makePlateauGrid(TileGrid& grid, int plateauHeight) {
    const bool initialized = grid.initialize();
    assert(initialized);
    (void)initialized;
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            Tile* ground = grid.getTile(x, y, 0);
            assert(ground);
            ground->setTopSolid(true);
        }
    }
    for (int z = 1; z <= plateauHeight; ++z) {
        Tile* stacked = grid.getTile(1, 1, z);
        assert(stacked);
        stacked->setTopSolid(true);
    }
}

glm::vec3 topOfColumn(int gridX, int gridY, int topSolidLayer) {
    return glm::vec3(gridX * kTileSize, gridY * kTileSize, topSolidLayer * kTileSize);
}

}  // namespace

int main() {
    // Stepping off a one-tile plateau is allowed and reports a one-tile fall.
    {
        TileGrid grid(glm::ivec3(4, 4, 4), kTileSize);
        makePlateauGrid(grid, 1);
        const glm::vec3 onPlateau = topOfColumn(1, 1, 1);
        const glm::vec3 offPlateau(2 * kTileSize, 1 * kTileSize, onPlateau.z);

        assert(grid.canOccupy(onPlateau, offPlateau));
        assert(grid.getFallHeightTiles(onPlateau, onPlateau.z) == 0);
        assert(grid.getFallHeightTiles(offPlateau, onPlateau.z) == 1);

        // The entity lands on the ground surface below.
        const float landingZ = grid.getSurfaceHeight(offPlateau.x, offPlateau.y, onPlateau.z);
        assert(landingZ < onPlateau.z);
    }

    // A two-tile plateau reports a two-tile fall.
    {
        TileGrid grid(glm::ivec3(4, 4, 4), kTileSize);
        makePlateauGrid(grid, 2);
        const glm::vec3 onPlateau = topOfColumn(1, 1, 2);
        const glm::vec3 offPlateau(2 * kTileSize, 1 * kTileSize, onPlateau.z);

        assert(grid.canOccupy(onPlateau, offPlateau));
        assert(grid.getFallHeightTiles(offPlateau, onPlateau.z) == 2);
    }

    // Flat ground never reports a fall.
    {
        TileGrid grid(glm::ivec3(4, 4, 4), kTileSize);
        makePlateauGrid(grid, 0);
        const glm::vec3 from = topOfColumn(2, 2, 0);
        const glm::vec3 to(3 * kTileSize, 2 * kTileSize, from.z);
        assert(grid.canOccupy(from, to));
        assert(grid.getFallHeightTiles(to, from.z) == 0);
    }

    return 0;
}
