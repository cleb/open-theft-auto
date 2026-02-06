#pragma once

#include <glm/glm.hpp>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Tile.hpp"
#include "LevelData.hpp"

class TileGrid;
struct LevelData;
class Renderer;
class InputManager;
class Mesh;
class Texture;
class Window;

class TileGridEditor {
public:
    TileGridEditor();
    ~TileGridEditor();

    void initialize(TileGrid* grid, LevelData* levelData);
    void setLevelPath(const std::string& path);
    const std::string& getLevelPath() const { return m_levelPath; }
    void setCursor(const glm::ivec3& gridPos);
    const glm::ivec3& getCursor() const { return m_cursor; }

    // Level management
    bool newLevel(const glm::ivec3& gridSize, float tileSize = 3.0f);
    bool loadLevel(const std::string& path);
    bool saveLevel();
    bool saveLevelAs(const std::string& path);

    void setWindow(Window* window) { m_window = window; }
    void setRenderer(Renderer* renderer) { m_renderer = renderer; }

    // Callback for when level is changed (new/load)
    using LevelChangedCallback = std::function<void()>;
    void setLevelChangedCallback(LevelChangedCallback callback) { m_levelChangedCallback = std::move(callback); }

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void update(float deltaTime);
    void processInput(InputManager* input, float deltaTime);
    void render(Renderer* renderer);
    void drawGui();

private:
    enum class BrushType {
        Grass,
        Road,
        Sidewalk,
        Empty,
        Vehicle,
    PlayerSpawn,
    Pickup
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
        SidewalkDirection topSidewalkDirection = SidewalkDirection::None;
        float drivability = 1.0f;  // Surface drivability (0.0-1.0)
        std::array<char, TextureBufferSize> topTexture{};
        std::array<bool, 4> wallWalkable{};
        std::array<std::array<char, TextureBufferSize>, 4> wallTextures{};
        // Vehicle spawn weights for traffic spawning (indexed by vehicle type order from config)
        std::vector<float> spawnWeights;
    };

    struct UiVehicleState {
        bool cursorHasVehicle = false;
        bool removeMode = false;
        float rotationDegrees = 0.0f;
        glm::vec2 size = glm::vec2(1.5f, 3.0f);
        std::array<char, TextureBufferSize> texture{};
        int vehicleTypeIndex = 0;  // Index into VehicleConfig definitions
    };

    struct UiPlayerSpawnState {
        float rotationDegrees = 0.0f;
    };

    struct UiPickupState {
        bool cursorHasPickup = false;
        bool removeMode = false;
        int pickupTypeIndex = 0;
    };

    struct AliasEntry {
        std::string name;
        std::string path;
    };

    struct PrefabEntry {
        std::string name;
        std::unique_ptr<Tile> tile;
    };

    struct EditorSnapshot {
        glm::ivec3 gridSize{0};
        glm::ivec3 cursor{0};
        std::vector<std::unique_ptr<Tile>> tiles;
        LevelData levelData;
    };

    TileGrid* m_grid;
    LevelData* m_levelData;
    Window* m_window;
    Renderer* m_renderer;
    bool m_enabled;
    glm::ivec3 m_cursor;
    glm::ivec3 m_lastAnnouncedCursor;
    BrushType m_brush;
    BrushType m_lastAnnouncedBrush;
    CarDirection m_roadDirection;
    SidewalkDirection m_sidewalkDirection;
    std::string m_levelPath;

    std::unique_ptr<Mesh> m_cursorMesh;
    std::shared_ptr<Texture> m_cursorTexture;
    glm::vec3 m_cursorColor;
    std::unique_ptr<Mesh> m_arrowMesh;
    glm::vec3 m_arrowColor;

    // Selection state
    std::vector<glm::ivec3> m_selectedTiles;
    bool m_isSelecting;
    glm::ivec3 m_selectionStart;
    glm::ivec3 m_selectionEnd;
    std::unique_ptr<Mesh> m_selectionMesh;
    glm::vec3 m_selectionColor;
    bool m_moveMode;
    glm::ivec3 m_moveOffset;
    bool m_hasHoverTile;
    glm::ivec3 m_hoverTile;
    glm::vec3 m_hoverColor;
    int m_hoverLayerOffset;
    bool m_isTileDragPainting;
    glm::ivec3 m_tileDragStart;
    glm::ivec3 m_tileDragEnd;
    
    // Edge scrolling
    float m_edgeScrollSpeed;
    float m_edgeScrollMargin;

    bool m_helpPrinted;
    UiTileState m_uiTileState;
    std::vector<AliasEntry> m_aliasEntries;
    UiVehicleState m_uiVehicleState;
    std::vector<PrefabEntry> m_prefabs;
    std::array<char, PrefabNameBufferSize> m_newPrefabName{};
    int m_selectedPrefabIndex;
    int m_prefabAutoNameCounter;
    std::vector<EditorSnapshot> m_undoStack;
    glm::ivec3 m_pendingGridSize;
    std::string m_gridResizeError;
    UiPlayerSpawnState m_uiPlayerSpawnState;
    UiPickupState m_uiPickupState;
    std::unique_ptr<Mesh> m_playerSpawnMesh;
    glm::vec3 m_playerSpawnColor;

    // File dialog state
    bool m_showNewLevelDialog;
    bool m_showLoadLevelDialog;
    bool m_showSaveAsDialog;
    glm::ivec3 m_newLevelSize;
    float m_newLevelTileSize;
    std::array<char, 512> m_filePathBuffer;
    std::string m_fileDialogError;
    std::vector<std::string> m_availableLevelFiles;
    LevelChangedCallback m_levelChangedCallback;

    Tile* currentTile();
    const Tile* currentTile() const;

    VehicleSpawnDefinition* findVehicleSpawn(const glm::ivec3& gridPos);
    const VehicleSpawnDefinition* findVehicleSpawn(const glm::ivec3& gridPos) const;
    PickupSpawnDefinition* findPickupSpawn(const glm::ivec3& gridPos);
    const PickupSpawnDefinition* findPickupSpawn(const glm::ivec3& gridPos) const;

    VehiclePlacementStatus evaluateVehiclePlacement(const glm::ivec3& position) const;

    void ensureCursorMesh();
    void ensureArrowMesh();
    void ensureSelectionMesh();
    void ensurePlayerSpawnMesh();
    void refreshCursorColor();
    void announceCursor();
    void announceBrush();
    void printHelp() const;

    void refreshUiStateFromTile();
    void rebuildAliasList();
    void drawBrushControls();
    void drawVehicleBrushControls();
    void drawPlayerSpawnBrushControls();
    void drawPickupBrushControls();
    void drawPrefabControls();
    void drawTileFaceTabs();
    void drawTopFaceControls(Tile* tile);
    void drawWallControls(Tile* tile, WallDirection direction, int wallIndex);
    void drawGridControls();
    bool drawTexturePicker(const char* label, std::array<char, TextureBufferSize>& buffer);
    std::string findAliasForPath(const std::string& path) const;
    void applyTopSurfaceFromUi();
    void applySpawnWeightsFromUi();
    void applyDrivabilityFromUi();
    void applyWallFromUi(int wallIndex, WallDirection direction);
    void applyVehicleBrush();
    void removeVehicleAtCursor();
    void clearTileAtCursor();
    void applyPlayerSpawnBrush();
    void applyPickupBrush();
    void syncPendingGridSizeFromGrid();
    void applySelectedTileToRect(const glm::ivec3& start, const glm::ivec3& end);
    bool pushUndoState();
    void clearUndoHistory();
    bool restoreSnapshot(const EditorSnapshot& snapshot);
    void undoLastEdit();

    void applyBrush();
    void applyBucketFill();
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
    void handleUndoHotkey(InputManager* input);
    void handleSaveHotkey(InputManager* input);
    void handleSelectionHotkeys(InputManager* input);

    // File management methods
    void drawFileManagementControls();
    void drawNewLevelDialog();
    void drawLoadLevelDialog();
    void drawSaveAsDialog();
    void scanAvailableLevelFiles();

    // Selection methods
    void clearSelection();
    void addToSelection(const glm::ivec3& pos);
    void removeFromSelection(const glm::ivec3& pos);
    bool isSelected(const glm::ivec3& pos) const;
    void selectArea(const glm::ivec3& start, const glm::ivec3& end);
    void selectAll();
    void handleMouseSelection(InputManager* input);
    void startMove();
    void applyMove(const glm::ivec3& offset);
    void renderSelection(Renderer* renderer);
    void renderTileDragPreview(Renderer* renderer);
    void drawSelectionControls();
    bool getTileAtScreenPosition(double mouseX, double mouseY, glm::ivec3& outTilePos) const;
};
