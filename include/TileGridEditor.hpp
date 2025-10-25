#pragma once

#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Tile.hpp"

class TileGrid;
class Renderer;
class InputManager;
class Mesh;
class Texture;

class TileGridEditor {
public:
    TileGridEditor();
    ~TileGridEditor();

    void initialize(TileGrid* grid);
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
        Empty
    };

    static constexpr std::size_t TextureBufferSize = 256;

    struct UiTileState {
        glm::ivec3 position{0};
        bool hasTile = false;
        bool topSolid = false;
        CarDirection topCarDirection = CarDirection::None;
        std::array<char, TextureBufferSize> topTexture{};
        std::array<bool, 4> wallWalkable{};
        std::array<std::array<char, TextureBufferSize>, 4> wallTextures{};
    };

    struct AliasEntry {
        std::string name;
        std::string path;
    };

    TileGrid* m_grid;
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
    glm::ivec3 m_pendingGridSize;
    std::string m_gridResizeError;

    Tile* currentTile();
    const Tile* currentTile() const;

    void ensureCursorMesh();
    void refreshCursorColor();
    void announceCursor();
    void announceBrush();
    void printHelp() const;

    void refreshUiStateFromTile();
    void rebuildAliasList();
    void drawBrushControls();
    void drawTileFaceTabs();
    void drawTopFaceControls(Tile* tile);
    void drawWallControls(Tile* tile, WallDirection direction, int wallIndex);
    void drawGridControls();
    bool drawTexturePicker(const char* label, std::array<char, TextureBufferSize>& buffer);
    std::string findAliasForPath(const std::string& path) const;
    void applyTopSurfaceFromUi();
    void applyWallFromUi(int wallIndex, WallDirection direction);
    void syncPendingGridSizeFromGrid();

    void applyBrush();
    void toggleWall(WallDirection direction);
    void changeLayer(int delta);
    void moveCursor(int dx, int dy);
    void clampCursor();
    void handleBrushHotkeys(InputManager* input);
    void handleWallHotkeys(InputManager* input);
    void handleSaveHotkey(InputManager* input);
};
