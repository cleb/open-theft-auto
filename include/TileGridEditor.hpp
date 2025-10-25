#pragma once

#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Tile.hpp"

class TileGrid;
struct LevelData;
struct VehicleSpawnDefinition;
class Renderer;
class InputManager;
class Mesh;
class Texture;

class TileGridEditor {
public:
    TileGridEditor();
    ~TileGridEditor();

    void initialize(TileGrid* grid, LevelData* levelData);
    void setLevelPath(const std::string& path);
    void setCursor(const glm::ivec3& gridPos);
    const glm::ivec3& getCursor() const { return m_cursor; }

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void update(float deltaTime);
    void processInput(InputManager* input);
    void render(Renderer* renderer);
    void drawGui();

private:
    enum class BrushType {
        Grass,
        Road,
        Empty,
        Vehicle
    };

    enum class VehiclePlacementStatus {
        Valid,
        OutOfBounds,
        MissingSupport
    };

    static constexpr std::size_t TextureBufferSize = 256;
    static constexpr std::size_t PrefabNameBufferSize = 64;

    struct UiTileState {
        glm::ivec3 position{0};
        bool hasTile = false;
        bool topSolid = false;
        CarDirection topCarDirection = CarDirection::None;
        std::array<char, TextureBufferSize> topTexture{};
        std::array<bool, 4> wallWalkable{};
        std::array<std::array<char, TextureBufferSize>, 4> wallTextures{};
    };

    struct UiVehicleState {
        bool cursorHasVehicle = false;
        bool removeMode = false;
        float rotationDegrees = 0.0f;
        glm::vec2 size = glm::vec2(1.5f, 3.0f);
        std::array<char, TextureBufferSize> texture{};
    };

    struct AliasEntry {
        std::string name;
        std::string path;
    };

    struct PrefabEntry {
        std::string name;
        std::unique_ptr<Tile> tile;
    };

    TileGrid* m_grid;
    LevelData* m_levelData;
    bool m_enabled;
    glm::ivec3 m_cursor;
    glm::ivec3 m_lastAnnouncedCursor;
    BrushType m_brush;
    BrushType m_lastAnnouncedBrush;
    CarDirection m_roadDirection;
    std::string m_levelPath;

    std::unique_ptr<Mesh> m_cursorMesh;
    std::shared_ptr<Texture> m_cursorTexture;
    glm::vec3 m_cursorColor;

    bool m_helpPrinted;
    UiTileState m_uiTileState;
    std::vector<AliasEntry> m_aliasEntries;
    UiVehicleState m_uiVehicleState;
    std::vector<PrefabEntry> m_prefabs;
    std::array<char, PrefabNameBufferSize> m_newPrefabName{};
    int m_selectedPrefabIndex;
    int m_prefabAutoNameCounter;
    glm::ivec3 m_pendingGridSize;
    std::string m_gridResizeError;

    Tile* currentTile();
    const Tile* currentTile() const;

    VehicleSpawnDefinition* findVehicleSpawn(const glm::ivec3& gridPos);
    const VehicleSpawnDefinition* findVehicleSpawn(const glm::ivec3& gridPos) const;

    VehiclePlacementStatus evaluateVehiclePlacement(const glm::ivec3& position) const;

    void ensureCursorMesh();
    void refreshCursorColor();
    void announceCursor();
    void announceBrush();
    void printHelp() const;

    void refreshUiStateFromTile();
    void rebuildAliasList();
    void drawBrushControls();
    void drawVehicleBrushControls();
    void drawPrefabControls();
    void drawTileFaceTabs();
    void drawTopFaceControls(Tile* tile);
    void drawWallControls(Tile* tile, WallDirection direction, int wallIndex);
    void drawGridControls();
    bool drawTexturePicker(const char* label, std::array<char, TextureBufferSize>& buffer);
    std::string findAliasForPath(const std::string& path) const;
    void applyTopSurfaceFromUi();
    void applyWallFromUi(int wallIndex, WallDirection direction);
    void applyVehicleBrush();
    void removeVehicleAtCursor();
    void syncPendingGridSizeFromGrid();

    void applyBrush();
    void savePrefab(const std::string& name);
    void applyPrefab(std::size_t index);
    void deletePrefab(std::size_t index);
    void toggleWall(WallDirection direction);
    void changeLayer(int delta);
    void moveCursor(int dx, int dy);
    void clampCursor();
    void handleBrushHotkeys(InputManager* input);
    void handleWallHotkeys(InputManager* input);
    void handlePrefabHotkeys(InputManager* input);
    void handleSaveHotkey(InputManager* input);
};
